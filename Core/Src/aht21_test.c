/*
 * aht21_test.c
 *
 * AHT21 硬件自检：参考 AHT21-Test 工程的探测思路（发地址查 ACK + 读状态字），
 * 以自包含函数的形式加进主工程，不改动任何现有模块。
 *
 * 用法：在 TIM2 初始化（timer_2_init）和 UART（printf 重定向）就绪后，
 *       任意位置调用一次 aht21_hw_test()，例如 main() 中或 app_task_create() 开头。
 *
 * v3 关键改进：
 *   - 探测判据改为 "0x70 和 0x71 都收到 ACK" 才算真设备。
 *     因为空总线上的假 ACK 只出现在末位=0 的字节（0x70 末位0 → 假ACK，
 *     0x71 末位1 → 假NACK），真从机对两个地址都会应答，据此彻底排除假 ACK。
 *   - 自动探测两组引脚：PB13/PB14（主工程当前接线）和 PB8/PB9（文档标称接线）。
 *   - 读数据相位加稳定延时；打印空闲电平与原始 6 字节。
 */

#include "aht21_test.h"

#include <stdio.h>

#include "stm32f4xx.h"
#include "iic.h"
#include "timer.h"

/* AHT21 命令/地址 */
#define AHT21_TEST_WRITE_ADDR   0x70   /* 7bit 地址 0x38 左移 1 位 */
#define AHT21_TEST_READ_ADDR    0x71
#define AHT21_TEST_MEASURE_CMD  0xAC
#define AHT21_TEST_MEASURE_ARG1 0x33
#define AHT21_TEST_MEASURE_ARG2 0x00
#define AHT21_TEST_MEASURE_MS   80     /* 测量等待时间 */

/* iic.c 中的底层位带原语（iic.h 未声明，此处补 extern 声明） */
extern void SDA_Input_Mode(iic_bus_t *bus);
extern void SDA_Output_Mode(iic_bus_t *bus);
extern void SCL_Output(iic_bus_t *bus, uint16_t val);

/* 待探测的引脚组合 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t sda_pin;
    uint16_t scl_pin;
    const char *desc;
} aht21_test_pins_t;

static const aht21_test_pins_t s_test_pin_sets[] = {
    { GPIOB, GPIO_Pin_13, GPIO_Pin_14, "PB13(SDA)/PB14(SCL) - 主工程当前接线" },
    { GPIOB, GPIO_Pin_9,  GPIO_Pin_8,  "PB9(SDA)/PB8(SCL)   - 文档/注释标称接线" },
};
#define AHT21_TEST_PIN_SET_COUNT (sizeof(s_test_pin_sets) / sizeof(s_test_pin_sets[0]))

/**
 * @brief 等待从机 ACK（自实现，采样前稳定 10us）
 * @return 1 = 收到 ACK；0 = 超时/NACK（内部已发 STOP 复位总线）
 */
static unsigned char aht21_test_wait_ack(iic_bus_t *bus)
{
    unsigned short err_time = 5;    /* 5 x 1us = 5us 超时，与 13_AHT21 一致 */
    SDA_Input_Mode(bus);
    SCL_Output(bus, 1);
    while (GPIO_ReadInputDataBit(bus->IIC_SDA_PORT, bus->IIC_SDA_PIN) == Bit_SET) {
        if (0 == --err_time) {
            SDA_Output_Mode(bus);
            IICStop(bus);           /* 复位总线 */
            return 0;               /* 无应答 */
        }
        delay_us(1);
    }
    SDA_Output_Mode(bus);
    SCL_Output(bus, 0);
    delay_us(2);
    return 1;                       /* 收到 ACK */
}

/**
 * @brief 接收 1 字节（SCL 拉高后稳定 10us 再采样）
 */
static uint8_t aht21_test_receive_byte(iic_bus_t *bus)
{
    uint8_t i = 8;
    uint8_t val = 0;

    SDA_Input_Mode(bus);
    while (i--) {
        val <<= 1;
        SCL_Output(bus, 0);
        delay_us(2);
        SCL_Output(bus, 1);
        delay_us(1);                /* 与 13_AHT21 一致的快速采样时序 */
        if (GPIO_ReadInputDataBit(bus->IIC_SDA_PORT, bus->IIC_SDA_PIN) == Bit_SET) {
            val |= 0x01;
        }
    }
    SCL_Output(bus, 0);
    SDA_Output_Mode(bus);
    return val;
}

/**
 * @brief 发一个地址字节并等待 ACK（探测用）
 * @return 1 = ACK；0 = NACK
 */
static unsigned char aht21_test_probe_addr(iic_bus_t *bus, uint8_t addr)
{
    unsigned char ack;

    IICStart(bus);
    IICSendByte(bus, addr);
    ack = aht21_test_wait_ack(bus);
    IICStop(bus);
    return ack;
}

/**
 * @brief 发 0x71 读 1 字节状态字
 * @return 状态字节；读不到时返回 0xFF
 */
static uint8_t aht21_test_read_status(iic_bus_t *bus)
{
    uint8_t status = 0;

    IICStart(bus);
    IICSendByte(bus, AHT21_TEST_READ_ADDR);
    if (!aht21_test_wait_ack(bus)) {
        IICStop(bus);
        return 0xFF;
    }
    status = aht21_test_receive_byte(bus);
    IICSendNotAck(bus);
    IICStop(bus);
    return status;
}

/**
 * @brief 触发测量并读回温湿度
 * @return 0 = 成功；-1 = 通信失败；-2 = 测量超时
 */
static int aht21_test_measure(iic_bus_t *bus, float *p_temp, float *p_humi)
{
    uint8_t cnt = 5;
    uint8_t b[6] = {0};
    uint32_t raw;

    /* 触发测量 */
    IICStart(bus);
    IICSendByte(bus, AHT21_TEST_WRITE_ADDR);
    if (!aht21_test_wait_ack(bus)) return -1;
    IICSendByte(bus, AHT21_TEST_MEASURE_CMD);
    if (!aht21_test_wait_ack(bus)) return -1;
    IICSendByte(bus, AHT21_TEST_MEASURE_ARG1);
    if (!aht21_test_wait_ack(bus)) return -1;
    IICSendByte(bus, AHT21_TEST_MEASURE_ARG2);
    if (!aht21_test_wait_ack(bus)) return -1;
    IICStop(bus);

    /* 等待测量完成 */
    delay_ms(AHT21_TEST_MEASURE_MS);

    /* 轮询状态字 Bit[7]，0 表示测量完成 */
    while ((0x80 == (aht21_test_read_status(bus) & 0x80)) && cnt) {
        delay_ms(5);
        cnt--;
    }
    if (0 == cnt) {
        return -2;                  /* 超时 */
    }

    /* 连续读 6 字节：b[0]=状态字，b[1..5]=数据 */
    IICStart(bus);
    IICSendByte(bus, AHT21_TEST_READ_ADDR);
    if (!aht21_test_wait_ack(bus)) return -1;
    b[0] = aht21_test_receive_byte(bus);
    for (uint8_t i = 1; i < 6; i++) {
        IICSendAck(bus);
        b[i] = aht21_test_receive_byte(bus);
    }
    IICSendNotAck(bus);
    IICStop(bus);

    /* 原始字节供诊断 */
    printf("        raw 6 bytes: %02X %02X %02X %02X %02X %02X\r\n",
           b[0], b[1], b[2], b[3], b[4], b[5]);

    /* 换算（与 bsp_aht21_driver.c 相同公式） */
    raw = ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    raw >>= 4;
    *p_humi = (float)raw * 100.0f / 1048576.0f;

    raw = (((uint32_t)b[3] & 0x0F) << 16) | ((uint32_t)b[4] << 8) | b[5];
    *p_temp = (float)raw * 200.0f / 1048576.0f - 50.0f;

    return 0;
}

int aht21_hw_test(void)
{
    uint8_t status = 0;
    float temp = 0.0f;
    float humi = 0.0f;
    int ret = 1;

    printf("\r\n========== AHT21 Hardware Test ==========\r\n");
    printf("[clk] SystemCoreClock = %lu Hz\r\n", SystemCoreClock);

    /* 确保 GPIOB 时钟打开（重复使能无害） */
    // RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    /* 上电等待 >= 100ms（官方流程第 1 步） */
    delay_ms(100);

    for (uint32_t i = 0; i < 1; i++) {
        iic_bus_t bus = {
            .IIC_SDA_PORT = s_test_pin_sets[i].port,
            .IIC_SCL_PORT = s_test_pin_sets[i].port,
            .IIC_SDA_PIN  = s_test_pin_sets[i].sda_pin,
            .IIC_SCL_PIN  = s_test_pin_sets[i].scl_pin,
        };
        unsigned char ack_w = 0, ack_r = 0;

        printf("\r\n--- probing %s ---\r\n", s_test_pin_sets[i].desc);

        IICInit(&bus);

        /* 空闲电平探测 */
        {
            uint8_t sda_idle = GPIO_ReadInputDataBit(bus.IIC_SDA_PORT, bus.IIC_SDA_PIN);
            uint8_t scl_idle = GPIO_ReadInputDataBit(bus.IIC_SCL_PORT, bus.IIC_SCL_PIN);
            printf("[idle] SDA=%d SCL=%d (1=high normal, 0=line held LOW!)\r\n",
                   sda_idle, scl_idle);
        }

        /* 真设备判据：写地址 0x70 与读地址 0x71 都必须 ACK。
           空总线只在末位=0 的字节（0x70）出现假 ACK，0x71（末位1）会假 NACK。 */
        ack_w = aht21_test_probe_addr(&bus, AHT21_TEST_WRITE_ADDR);
        ack_r = aht21_test_probe_addr(&bus, AHT21_TEST_READ_ADDR);
        printf("[probe] ACK(0x70)=%d  ACK(0x71)=%d  -> %s\r\n",
               ack_w, ack_r,
               (ack_w && ack_r) ? "REAL device present!" :
               ((ack_w || ack_r) ? "spurious ACK (dead bus, no real slave)" :
                                   "no response"));

        if (!(ack_w && ack_r)) {
            continue;   /* 这组引脚不是真设备，试下一组 */
        }

        /* 读状态字（0x71） */
        status = aht21_test_read_status(&bus);
        printf("[status] byte = 0x%02X", status);
        if (0x18 == status) {
            printf("  (calibrated, idle) GOOD\r\n");
        } else if (((status & 0x18) == 0x18) && (0xFF != status)) {
            printf("  (calibrated, extra bits set)\r\n");
        } else if (0xFF == status) {
            printf("  (all ones! ACK was real but no data bits driven)\r\n");
            printf("    -> chip may NOT be AHT21-compatible (AHT30 shares addr 0x38),\r\n");
            printf("       continuing to measurement...\r\n");
        } else {
            printf("  (abnormal: 0x10=uncalibrated, 0x9X=busy)\r\n");
        }

        /* 连续测量 5 次（间隔 1s），验证传感器数据是否持续刷新。
           与主工程驱动的差别：本测试无临界区包裹、80ms 直接用 delay_ms 等待。 */
        for (int m = 0; m < 5; m++) {
            if (m > 0) {
                delay_ms(1000);   /* 与主工程 send 任务节奏一致 */
            }
            ret = aht21_test_measure(&bus, &temp, &humi);
            if (0 != ret) {
                printf("[measure#%d] failed (ret=%d): responds but no valid data\r\n", m, ret);
                printf("========== TEST FAILED (measure) ==========\r\n");
                return 3;
            }
            printf("[measure#%d] temp = %.2f C, humi = %.2f %%RH\r\n", m, temp, humi);
        }

        if (temp < -40.0f || temp > 125.0f || humi < 0.0f || humi > 100.0f) {
            printf("    values out of range -> data invalid!\r\n");
            printf("========== TEST FAILED (data invalid) ==========\r\n");
            return 3;
        }

        printf("========== AHT21 TEST PASSED on %s ==========\r\n",
               s_test_pin_sets[i].desc);
        return 0;
    }

    printf("\r\nNo AHT21-compatible device on either pin set:\r\n");
    printf("  - PB13/PB14 and PB8/PB9 both probed (0x70+0x71 must both ACK)\r\n");
    printf("  check: module VCC=3.3V + GND common, which pins are actually used,\r\n");
    printf("         module really AHT21/AHT20 (not AHT30/SHT30), pull-ups if bare chip\r\n");
    printf("========== TEST FAILED (no device) ==========\r\n");
    return 1;
}

uint8_t AHT_Read_Status(iic_bus_t AHT_bus)
{
	uint8_t Byte_first;	
	IICStart(&AHT_bus);
	IICSendByte(&AHT_bus,0x71);
	IICWaitAck(&AHT_bus);
	IICReceiveByte(&AHT_bus,&Byte_first);
	IICSendNotAck(&AHT_bus);
	IICStop(&AHT_bus);	
	return Byte_first;
}

uint8_t aht21_test2(iic_bus_t AHT_bus) {
	uint8_t cnt=5;
	uint8_t  Byte_1th=0;
	uint8_t  Byte_2th=0;
	uint8_t  Byte_3th=0;
	uint8_t  Byte_4th=0;
	uint8_t  Byte_5th=0;
	uint8_t  Byte_6th=0;
	uint32_t RetuData = 0;

    float humi = 0.0, temp = 0.0;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    IICInit(&AHT_bus);

    delay_ms(100);

    if((AHT_Read_Status(AHT_bus)&0x08)!=0x08)
	{
		IICStart(&AHT_bus);
		IICSendByte(&AHT_bus,0x70);
		IICWaitAck(&AHT_bus);
		IICSendByte(&AHT_bus,0xBE);
		IICWaitAck(&AHT_bus);
		IICSendByte(&AHT_bus,0x08);
		IICWaitAck(&AHT_bus);
		IICSendByte(&AHT_bus,0x00);
		IICWaitAck(&AHT_bus);
		IICStop(&AHT_bus);
		delay_ms(10);
	}
	
	IICStart(&AHT_bus);
	IICSendByte(&AHT_bus,0x70);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0xAC);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0x33);
	IICWaitAck(&AHT_bus);
	IICSendByte(&AHT_bus,0x00);
	IICWaitAck(&AHT_bus);
	IICStop(&AHT_bus);	
	
	delay_ms(80);
	while(((AHT_Read_Status(AHT_bus)&0x80)==0x80) && cnt)
	{
		delay_ms(5);
		cnt--;
		AHT_Read_Status(AHT_bus);
	}
	if(!cnt)
	{
        printf("cnt = %d\r\n", cnt);
        // return 1;
    }
	
	IICStart(&AHT_bus);
	IICSendByte(&AHT_bus,0x71);
	IICWaitAck(&AHT_bus);
	IICReceiveByte(&AHT_bus,&Byte_1th);
	IICSendAck(&AHT_bus);
	IICReceiveByte(&AHT_bus,&Byte_2th);
	IICSendAck(&AHT_bus);
	IICReceiveByte(&AHT_bus,&Byte_3th);
	IICSendAck(&AHT_bus);
	IICReceiveByte(&AHT_bus,&Byte_4th);
	IICSendAck(&AHT_bus);
	IICReceiveByte(&AHT_bus,&Byte_5th);
	IICSendAck(&AHT_bus);
	IICReceiveByte(&AHT_bus,&Byte_6th);
	IICSendNotAck(&AHT_bus);
	IICStop(&AHT_bus);

	RetuData = (RetuData|Byte_2th)<<8;
	RetuData = (RetuData|Byte_3th)<<8;
	RetuData = (RetuData|Byte_4th);
	RetuData =RetuData >>4;
	humi = (RetuData * 1000 >> 20);
	humi /= 10;
	
	RetuData = 0;
	RetuData = (RetuData|(Byte_4th&0x0f))<<8;
	RetuData = (RetuData|Byte_5th)<<8;
	RetuData = (RetuData|Byte_6th);
	RetuData = RetuData&0xfffff;
	temp = ((RetuData * 2000 >> 20)- 500);
	temp /= 10;

    printf("[measure] temp = %.2f C, humi = %.2f %%RH\r\n", temp, humi);
	
	return 0;
}
