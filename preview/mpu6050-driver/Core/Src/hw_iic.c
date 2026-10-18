/**
 * @file    hw_iic.c
 * @brief   硬件 I2C 引擎实现（STM32F4 标准外设库，事件法 + 全程超时）
 *
 * 实现策略（与软件位带 I2C 的细粒度契约语义对齐）：
 *
 *  - pf_iic_start     : 产生 START 并等待 SB 标志（超时退出）。
 *  - pf_iic_send_byte : 写入 DR 并等待 TXE（地址字节则会连同等 ADDR 之前的 TXE）。
 *  - pf_iic_wait_ack  : 轮询 ADDR/TXE/BTF/AF：
 *        AF 置位  -> 从机 NACK，清标志返回 0；
 *        ADDR 置位 -> 地址阶段 ACK 到达，读 SR1+SR2 清除后返回 1；
 *        TXE/BTF  -> 数据阶段 ACK 完成，返回 1；
 *        超时     -> 返回 0。
 *  - pf_iic_receive_byte : 等待 RXNE 后读 DR。
 *  - pf_iic_send_ack / not_ack : 切换 ACK 位，控制"下一个收到字节"的应答。
 *        契约调用顺序 receive(N-1) → send_not_ack → receive(N) → stop，
 *        在 400kHz（22.5us/字节）下两次调用间隔远小于一字节时间，时序安全。
 *  - pf_iic_stop      : 产生 STOP 并等待 BUSY 清零。
 *  - pf_iic_bus_reset : BUSY 卡死恢复——外设复位 + 手动 9 个 SCL 时钟
 *        + STOP + 重新初始化。
 *
 * 超时实现：迭代计数上限 HW_IIC_EVT_LOOPS。按 100MHz 主频、每次轮询循环约
 * 8~12 个周期估算，HW_IIC_EVT_LOOPS = 25000 ≈ 2~3ms 上限，足以覆盖 400kHz
 * 下一字节的传输时间且不长时间占用 CPU。若主频或总线速率差异大，可在
 * 本文件顶部调整。
 *
 * @note 所有函数仅在任务上下文调用（由上层互斥锁保护，同一时刻一条总线
 *       只有一个任务在操作外设）。
 */
#include "hw_iic.h"

/* ------------------------------------------------------------------ */
/* 私有配置                                                            */
/* ------------------------------------------------------------------ */
#define HW_IIC_EVT_LOOPS      25000u   /*!< 单次事件等待的轮询上限(≈2-3ms) */
#define HW_IIC_STOP_LOOPS     25000u   /*!< 等 BUSY 清零的轮询上限          */
#define HW_IIC_BUSY_LOOPS     25000u   /*!< 等 BUSY 空闲的轮询上限          */
#define HW_IIC_SCLK_CYCLES    9u       /*!< 总线恢复脉冲数                  */

/* ------------------------------------------------------------------ */
/* 私有工具：SPL 时钟/复用映射                                         */
/* ------------------------------------------------------------------ */

static void hw_iic_gpio_config(iic_bus_t * const bus, uint8_t to_af)
{
    GPIO_InitTypeDef g;
    uint8_t scl_src, sda_src;

    /* 引脚源编号：GPIO_Pin_6 -> 6 */
    scl_src = (uint8_t)__builtin_ctz(bus->scl_pin);   /* GCC 内建，返回最低置位位序 */
    sda_src = (uint8_t)__builtin_ctz(bus->sda_pin);

    if (to_af) {
        g.GPIO_Pin   = bus->scl_pin;
        g.GPIO_Mode  = GPIO_Mode_AF;
        g.GPIO_OType = GPIO_OType_OD;
        g.GPIO_PuPd  = GPIO_PuPd_UP;
        g.GPIO_Speed = GPIO_High_Speed;
        GPIO_Init(bus->scl_port, &g);
        GPIO_PinAFConfig(bus->scl_port, scl_src, GPIO_AF_I2C1);
        GPIO_PinAFConfig(bus->sda_port, sda_src, GPIO_AF_I2C1);
    } else {
        /* 恢复为普通开漏输出（总线恢复用） */
        g.GPIO_Pin   = bus->scl_pin;
        g.GPIO_Mode  = GPIO_Mode_OUT;
        g.GPIO_OType = GPIO_OType_OD;
        g.GPIO_PuPd  = GPIO_PuPd_UP;
        g.GPIO_Speed = GPIO_High_Speed;
        GPIO_Init(bus->scl_port, &g);
        GPIO_SetBits(bus->scl_port, bus->scl_pin);
    }
}

static void hw_iic_rcc_on(iic_bus_t * const bus)
{
    /* GPIO 端口时钟 */
    if      (bus->scl_port == GPIOA) RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    else if (bus->scl_port == GPIOB) RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    else if (bus->scl_port == GPIOC) RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    else                             RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    /* 外设时钟 */
    if      (bus->i2c_periph == I2C1) RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    else if (bus->i2c_periph == I2C2) RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    else                              RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C3, ENABLE);
}

static void hw_iic_rcc_reset(iic_bus_t * const bus)
{
    if      (bus->i2c_periph == I2C1) { RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, ENABLE);  RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, DISABLE); }
    else if (bus->i2c_periph == I2C2) { RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C2, ENABLE);  RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C2, DISABLE); }
    else                              { RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C3, ENABLE);  RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C3, DISABLE); }
}

/* ------------------------------------------------------------------ */
/* 契约实现                                                            */
/* ------------------------------------------------------------------ */

int32_t hw_iic_init(iic_bus_t * const bus)
{
    I2C_InitTypeDef i;

    if (bus == NULL || bus->i2c_periph == NULL) {
        return 0;
    }

    hw_iic_rcc_on(bus);
    hw_iic_gpio_config(bus, 1);

    /* 外设先复位一次，确保从干净状态开始 */
    hw_iic_rcc_reset(bus);

    if (bus->i2c_clk_speed == 0u) {
        bus->i2c_clk_speed = 400000u;   /* 默认快速模式 */
    }

    I2C_StructInit(&i);
    i.I2C_ClockSpeed          = bus->i2c_clk_speed;
    i.I2C_Mode                = I2C_Mode_I2C;
    i.I2C_DutyCycle           = I2C_DutyCycle_2;
    i.I2C_OwnAddress1         = 0x00u;
    i.I2C_Ack                 = I2C_Ack_Enable;
    i.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(bus->i2c_periph, &i);

    I2C_Cmd(bus->i2c_periph, ENABLE);
    I2C_AcknowledgeConfig(bus->i2c_periph, ENABLE);

    return 1;
}

int32_t hw_iic_deinit(iic_bus_t * const bus)
{
    if (bus == NULL || bus->i2c_periph == NULL) {
        return 0;
    }
    I2C_Cmd(bus->i2c_periph, DISABLE);
    return 1;
}

int32_t hw_iic_start(iic_bus_t * const bus)
{
    uint32_t n = HW_IIC_EVT_LOOPS;

    if (bus == NULL) {
        return 0;
    }
    /* 若上一次 STOP 后总线仍忙，短暂等待 */
    while (I2C_GetFlagStatus(bus->i2c_periph, I2C_FLAG_BUSY) && (--n)) {
        /* 空转轮询 */
    }
    if (n == 0u) {
        return 0;   /* 总线被占死 */
    }

    I2C_GenerateSTART(bus->i2c_periph, ENABLE);
    n = HW_IIC_EVT_LOOPS;
    while (!I2C_GetFlagStatus(bus->i2c_periph, I2C_FLAG_SB) && (--n)) {
        /* 等待 SB */
    }
    return (n != 0u) ? 1 : 0;
}

int32_t hw_iic_send_byte(iic_bus_t * const bus, uint8_t byte)
{
    uint32_t n = HW_IIC_EVT_LOOPS;

    if (bus == NULL) {
        return 0;
    }
    I2C_SendData(bus->i2c_periph, byte);
    /* 等待 DR 清空（TXE）——ACK 的判定交给 wait_ack */
    while (!I2C_GetFlagStatus(bus->i2c_periph, I2C_FLAG_TXE) && (--n)) {
        /* 等待 TXE */
    }
    return (n != 0u) ? 1 : 0;
}

int32_t hw_iic_wait_ack(iic_bus_t * const bus)
{
    uint32_t n = HW_IIC_EVT_LOOPS;

    if (bus == NULL) {
        return 0;
    }
    for (; n != 0u; --n) {
        FlagStatus af   = I2C_GetFlagStatus(bus->i2c_periph, I2C_FLAG_AF);
        FlagStatus addr = I2C_GetFlagStatus(bus->i2c_periph, I2C_FLAG_ADDR);
        FlagStatus txe  = I2C_GetFlagStatus(bus->i2c_periph, I2C_FLAG_TXE);

        if (af == SET) {
            I2C_ClearFlag(bus->i2c_periph, I2C_FLAG_AF);   /* NACK：清标志 */
            return 0;
        }
        if (addr == SET) {
            /* 地址阶段 ACK：读 SR1 + SR2 清除 ADDR（读 SR2 即清） */
            (void)bus->i2c_periph->SR1;
            (void)bus->i2c_periph->SR2;
            return 1;
        }
        if (txe == SET) {
            return 1;   /* 数据阶段 ACK 完成 */
        }
    }
    return 0;   /* 超时 */
}

int32_t hw_iic_receive_byte(iic_bus_t * const bus, uint8_t * const out)
{
    uint32_t n = HW_IIC_EVT_LOOPS;

    if (bus == NULL || out == NULL) {
        return 0;
    }
    while (!I2C_GetFlagStatus(bus->i2c_periph, I2C_FLAG_RXNE) && (--n)) {
        /* 等待 RXNE */
    }
    if (n == 0u) {
        return 0;
    }
    *out = I2C_ReceiveData(bus->i2c_periph);
    return 1;
}

int32_t hw_iic_send_ack(iic_bus_t * const bus)
{
    if (bus == NULL) {
        return 0;
    }
    I2C_AcknowledgeConfig(bus->i2c_periph, ENABLE);
    return 1;
}

int32_t hw_iic_send_not_ack(iic_bus_t * const bus)
{
    if (bus == NULL) {
        return 0;
    }
    I2C_AcknowledgeConfig(bus->i2c_periph, DISABLE);
    return 1;
}

int32_t hw_iic_stop(iic_bus_t * const bus)
{
    uint32_t n = HW_IIC_STOP_LOOPS;

    if (bus == NULL) {
        return 0;
    }
    I2C_GenerateSTOP(bus->i2c_periph, ENABLE);
    while (I2C_GetFlagStatus(bus->i2c_periph, I2C_FLAG_BUSY) && (--n)) {
        /* 等待 BUSY 清零 */
    }
    /* 恢复 ACK 使能，为下一次传输做准备 */
    I2C_AcknowledgeConfig(bus->i2c_periph, ENABLE);
    return (n != 0u) ? 1 : 0;
}

int32_t hw_iic_bus_reset(iic_bus_t * const bus)
{
    uint32_t k;

    if (bus == NULL || bus->i2c_periph == NULL) {
        return 0;
    }

    /* 1) 外设先下电复位 */
    I2C_Cmd(bus->i2c_periph, DISABLE);
    hw_iic_rcc_reset(bus);

    /* 2) 引脚切普通开漏输出，手动发 9 个 SCL 脉冲释放从机 */
    hw_iic_gpio_config(bus, 0);

    GPIO_SetBits(bus->sda_port, bus->sda_pin);   /* SDA 释放 */
    for (k = 0; k < HW_IIC_SCLK_CYCLES; ++k) {
        GPIO_SetBits(bus->scl_port, bus->scl_pin);
        { volatile uint32_t d = 50u; while (d--) __NOP(); }
        GPIO_ResetBits(bus->scl_port, bus->scl_pin);
        { volatile uint32_t d = 50u; while (d--) __NOP(); }
    }
    /* STOP：SCL 高时 SDA 由低到高 */
    GPIO_ResetBits(bus->sda_port, bus->sda_pin);
    { volatile uint32_t d = 50u; while (d--) __NOP(); }
    GPIO_SetBits(bus->scl_port, bus->scl_pin);
    { volatile uint32_t d = 50u; while (d--) __NOP(); }
    GPIO_SetBits(bus->sda_port, bus->sda_pin);
    { volatile uint32_t d = 50u; while (d--) __NOP(); }

    /* 3) 重新初始化外设 + 引脚复用 */
    return hw_iic_init(bus);
}

/* ------------------------------------------------------------------ */
/* 便捷挂载                                                            */
/* ------------------------------------------------------------------ */

void hw_iic_mount_interface(iic_driver_interface_t *itf, iic_bus_t *bus)
{
    if (itf == NULL) {
        return;
    }
    itf->pf_iic_init          = hw_iic_init;
    itf->pf_iic_deinit        = hw_iic_deinit;
    itf->pf_iic_start         = hw_iic_start;
    itf->pf_iic_stop          = hw_iic_stop;
    itf->pf_iic_send_byte     = hw_iic_send_byte;
    itf->pf_iic_receive_byte  = hw_iic_receive_byte;
    itf->pf_iic_wait_ack      = hw_iic_wait_ack;
    itf->pf_iic_send_ack      = hw_iic_send_ack;
    itf->pf_iic_send_not_ack  = hw_iic_send_not_ack;
    itf->pf_iic_bus_reset     = hw_iic_bus_reset;
    itf->iic_bus              = bus;
}
