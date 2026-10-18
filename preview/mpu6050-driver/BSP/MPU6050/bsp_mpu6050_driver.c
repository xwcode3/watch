/**
 * @file    bsp_mpu6050_driver.c
 * @brief   MPU6050 设备逻辑层实现（纯器件协议，无硬件细节）
 *
 * 初始化序列（每步的寄存器/值/理由见设计文档 §5）：
 *   1. 上电稳定等待 100ms（经 OSAL delay，不忙等——规避 AHT21 的 TIM2 忙等问题）
 *   2. pf_iic_init 初始化总线引擎（硬件 I2C 外设由 hw_iic.c 完成）
 *   3. 读 WHO_AM_I，必须等于 0x68，否则返回 BAD_ID（链路/在位双重验证）
 *   4. 写 PWR_MGMT_1 = 0x80 器件复位，等待 100ms 振荡器稳定
 *   5. 写 SIGNAL_PATH_RESET = 0x07 复位加速度/陀螺/温度数字信号路径
 *   6. 写 PWR_MGMT_1 = 0x01 解除睡眠 + 选择 PLL(X gyro) 时钟
 *   7. 写 CONFIG = DLPF_CFG(默认 3, 44Hz)
 *   8. 写 SMPLRT_DIV = 默认 9（Fs = 1kHz/(1+9) = 100Hz）
 *   9. 写 GYRO_CONFIG = FS_SEL(默认 0, ±250°/s)
 *  10. 写 ACCEL_CONFIG = AFS_SEL(默认 0, ±2g)
 *
 * 读取序列（一次突发读 14 字节，0x3B 起）：
 *   START → 0xD0(写) → 0x3B → Sr(重复起始) → 0xD1(读) → 14 字节 → NACK → STOP
 *   [ax_H ax_L ay_H ay_L az_H az_L t_H t_L gx_H gx_L gy_H gy_L gz_H gz_L]
 *
 * 错误恢复：任何总线原语失败 → 计 err_count → 上层可调 pf_reset /
 * pf_iic_bus_reset 做器件复位与总线复位。
 */
#include "bsp_mpu6050_driver.h"

/* ------------------------------------------------------------------ */
/* 私有：总线事务封装（细粒度原语 -> 器件级读写）                        */
/* ------------------------------------------------------------------ */

#define MPU6050_I2C_WRITE_ADDR   (uint8_t)(MPU6050_I2C_ADDR_7BIT << 1)
#define MPU6050_I2C_READ_ADDR    (uint8_t)((MPU6050_I2C_ADDR_7BIT << 1) | 0x01u)

/** 写 1 个寄存器 */
static mpu6050_status_t mpu6050_write_reg(bsp_mpu6050_driver_t * const self,
                                          uint8_t reg, uint8_t val)
{
    iic_driver_interface_t *bus = self->p_iic_instance;

    if (bus == NULL || bus->pf_iic_start == NULL) {
        return MPU6050_STATUS_PARAM;
    }
    if (!bus->pf_iic_start(bus->iic_bus)) {
        return MPU6050_STATUS_ERRORBUS;
    }
    if (!bus->pf_iic_send_byte(bus->iic_bus, MPU6050_I2C_WRITE_ADDR) ||
        !bus->pf_iic_wait_ack(bus->iic_bus)) {
        bus->pf_iic_stop(bus->iic_bus);
        return MPU6050_STATUS_ERRORBUS;
    }
    if (!bus->pf_iic_send_byte(bus->iic_bus, reg) ||
        !bus->pf_iic_wait_ack(bus->iic_bus)) {
        bus->pf_iic_stop(bus->iic_bus);
        return MPU6050_STATUS_ERRORBUS;
    }
    if (!bus->pf_iic_send_byte(bus->iic_bus, val) ||
        !bus->pf_iic_wait_ack(bus->iic_bus)) {
        bus->pf_iic_stop(bus->iic_bus);
        return MPU6050_STATUS_ERRORBUS;
    }
    if (!bus->pf_iic_stop(bus->iic_bus)) {
        return MPU6050_STATUS_ERRORTIMEOUT;
    }
    return MPU6050_STATUS_OK;
}

/** 读 n 个寄存器（连续突发） */
static mpu6050_status_t mpu6050_read_regs(bsp_mpu6050_driver_t * const self,
                                          uint8_t start_reg,
                                          uint8_t *buf, uint8_t n)
{
    uint8_t i;
    iic_driver_interface_t *bus = self->p_iic_instance;

    if (bus == NULL || buf == NULL || n == 0u) {
        return MPU6050_STATUS_PARAM;
    }

    /* 相位 1：写寄存器地址 */
    if (!bus->pf_iic_start(bus->iic_bus)) {
        return MPU6050_STATUS_ERRORBUS;
    }
    if (!bus->pf_iic_send_byte(bus->iic_bus, MPU6050_I2C_WRITE_ADDR) ||
        !bus->pf_iic_wait_ack(bus->iic_bus)) {
        bus->pf_iic_stop(bus->iic_bus);
        return MPU6050_STATUS_ERRORBUS;
    }
    if (!bus->pf_iic_send_byte(bus->iic_bus, start_reg) ||
        !bus->pf_iic_wait_ack(bus->iic_bus)) {
        bus->pf_iic_stop(bus->iic_bus);
        return MPU6050_STATUS_ERRORBUS;
    }

    /* 相位 2：重复起始 + 读 */
    if (!bus->pf_iic_start(bus->iic_bus)) {          /* Repeated START */
        return MPU6050_STATUS_ERRORBUS;
    }
    if (!bus->pf_iic_send_byte(bus->iic_bus, MPU6050_I2C_READ_ADDR) ||
        !bus->pf_iic_wait_ack(bus->iic_bus)) {
        bus->pf_iic_stop(bus->iic_bus);
        return MPU6050_STATUS_ERRORBUS;
    }

    for (i = 0; i < n; ++i) {
        if (i == (uint8_t)(n - 1u)) {
            (void)bus->pf_iic_send_not_ack(bus->iic_bus);   /* 最后一字节前 NACK */
        }
        if (!bus->pf_iic_receive_byte(bus->iic_bus, &buf[i])) {
            bus->pf_iic_stop(bus->iic_bus);
            return MPU6050_STATUS_ERRORTIMEOUT;
        }
    }
    if (!bus->pf_iic_stop(bus->iic_bus)) {
        return MPU6050_STATUS_ERRORTIMEOUT;
    }
    return MPU6050_STATUS_OK;
}

/** OSAL 延时（无实例时退化为不等待并返回错误，由上层保证注入） */
static mpu6050_status_t mpu6050_delay_ms(bsp_mpu6050_driver_t * const self, uint32_t ms)
{
    if (self->p_delay_instance == NULL || self->p_delay_instance->delay_ms == NULL) {
        return MPU6050_STATUS_PARAM;
    }
    return (self->p_delay_instance->delay_ms(ms) == OSAL_STATUS_OK)
           ? MPU6050_STATUS_OK : MPU6050_STATUS_ERROR;
}

/* ------------------------------------------------------------------ */
/* ops 实现                                                             */
/* ------------------------------------------------------------------ */

static mpu6050_status_t mpu6050_read_id_impl(bsp_mpu6050_driver_t * const self,
                                             uint8_t * const p_id_out)
{
    mpu6050_status_t st;

    if (self == NULL || p_id_out == NULL) {
        return MPU6050_STATUS_PARAM;
    }
    if (!self->inited) {
        return MPU6050_STATUS_NOT_INIT;
    }
    st = mpu6050_read_regs(self, MPU6050_REG_WHO_AM_I, p_id_out, 1u);
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
        return st;
    }
    self->device_id = *p_id_out;
    return MPU6050_STATUS_OK;
}

static mpu6050_status_t mpu6050_reset_impl(bsp_mpu6050_driver_t * const self)
{
    mpu6050_status_t st;

    if (self == NULL) {
        return MPU6050_STATUS_PARAM;
    }

    /* 器件复位：PWR_MGMT_1.DEVICE_RESET = 1（硬件自动清零该位） */
    st = mpu6050_write_reg(self, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_DEVICE_RESET);
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
        return st;
    }
    (void)mpu6050_delay_ms(self, MPU6050_POWERUP_STABLE_MS);   /* 100ms 稳定 */

    /* 复位信号路径：加速度/陀螺/温度数字路径 */
    st = mpu6050_write_reg(self, MPU6050_REG_SIGNAL_PATH_RESET, 0x07u);
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
    }
    return st;
}

static mpu6050_status_t mpu6050_init_impl(bsp_mpu6050_driver_t * const self,
                                          const mpu6050_config_t * const cfg)
{
    mpu6050_status_t st;
    uint8_t id = 0u;
    mpu6050_config_t c;

    if (self == NULL || self->p_iic_instance == NULL) {
        return MPU6050_STATUS_PARAM;
    }

    /* 0) 默认配置 */
    if (cfg != NULL) {
        c = *cfg;
    } else {
        c.smplrt_div    = 9u;    /* 100Hz */
        c.dlpf_cfg      = 3u;    /* 44Hz  */
        c.gyro_fs_sel   = 0u;    /* ±250°/s */
        c.accel_afs_sel = 0u;    /* ±2g     */
    }
    self->cfg = c;

    /* 1) 上电/总线初始化 */
    (void)mpu6050_delay_ms(self, MPU6050_POWERUP_STABLE_MS);
    if (!self->p_iic_instance->pf_iic_init(self->p_iic_instance->iic_bus)) {
        return MPU6050_STATUS_ERRORBUS;
    }

    /* 2) 器件身份验证（WHO_AM_I 必须 = 0x68） */
    st = mpu6050_read_regs(self, MPU6050_REG_WHO_AM_I, &id, 1u);
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
        return st;                       /* 总线不通 */
    }
    if (id != MPU6050_WHO_AM_I_VALUE) {
        self->device_id = id;
        return MPU6050_STATUS_BAD_ID;    /* 器件不符/不在位 */
    }
    self->device_id = id;

    /* 3) 器件复位 + 信号路径复位 + 稳定等待 */
    st = mpu6050_reset_impl(self);
    if (st != MPU6050_STATUS_OK) {
        return st;
    }

    /* 4) 解除睡眠 + 时钟源 = PLL with X axis gyroscope */
    st = mpu6050_write_reg(self, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_PLL_XGYRO);
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
        return st;
    }
    (void)mpu6050_delay_ms(self, MPU6050_WAKEUP_STABLE_MS);    /* 30ms */

    /* 5) DLPF 数字低通滤波 */
    st = mpu6050_write_reg(self, MPU6050_REG_CONFIG, c.dlpf_cfg & 0x07u);
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
        return st;
    }

    /* 6) 采样率分频 */
    st = mpu6050_write_reg(self, MPU6050_REG_SMPLRT_DIV, c.smplrt_div);
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
        return st;
    }

    /* 7) 陀螺仪量程 */
    st = mpu6050_write_reg(self, MPU6050_REG_GYRO_CONFIG,
                           (uint8_t)((c.gyro_fs_sel & 0x03u) << 3));
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
        return st;
    }

    /* 8) 加速度计量程 */
    st = mpu6050_write_reg(self, MPU6050_REG_ACCEL_CONFIG,
                           (uint8_t)((c.accel_afs_sel & 0x03u) << 3));
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
        return st;
    }

    self->inited = 1u;
    return MPU6050_STATUS_OK;
}

static mpu6050_status_t mpu6050_deinit_impl(bsp_mpu6050_driver_t * const self)
{
    if (self == NULL) {
        return MPU6050_STATUS_PARAM;
    }
    if (!self->inited) {
        return MPU6050_STATUS_NOT_INIT;
    }
    /* 进入睡眠：PWR_MGMT_1.SLEEP = 1 */
    (void)mpu6050_write_reg(self, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_SLEEP);
    (void)self->p_iic_instance->pf_iic_deinit(self->p_iic_instance->iic_bus);
    self->inited = 0u;
    return MPU6050_STATUS_OK;
}

static mpu6050_status_t mpu6050_sleep_impl(bsp_mpu6050_driver_t * const self)
{
    if (self == NULL) {
        return MPU6050_STATUS_PARAM;
    }
    if (!self->inited) {
        return MPU6050_STATUS_NOT_INIT;
    }
    return mpu6050_write_reg(self, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_SLEEP);
}

static mpu6050_status_t mpu6050_wakeup_impl(bsp_mpu6050_driver_t * const self)
{
    mpu6050_status_t st;

    if (self == NULL) {
        return MPU6050_STATUS_PARAM;
    }
    if (!self->inited) {
        return MPU6050_STATUS_NOT_INIT;
    }
    st = mpu6050_write_reg(self, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_PLL_XGYRO);
    if (st == MPU6050_STATUS_OK) {
        (void)mpu6050_delay_ms(self, MPU6050_WAKEUP_STABLE_MS);
    }
    return st;
}

static mpu6050_status_t mpu6050_read_raw_impl(bsp_mpu6050_driver_t * const self,
                                              mpu6050_raw_data_t * const out)
{
    mpu6050_status_t st;
    uint8_t buf[14];

    if (self == NULL || out == NULL) {
        return MPU6050_STATUS_PARAM;
    }
    if (!self->inited) {
        return MPU6050_STATUS_NOT_INIT;
    }

    st = mpu6050_read_regs(self, MPU6050_REG_ACCEL_XOUT_H, buf, 14u);
    if (st != MPU6050_STATUS_OK) {
        self->err_count++;
        return st;
    }

    /* 高字节在前（big-endian）拼接为有符号 16-bit */
    out->accel_x = (int16_t)(((uint16_t)buf[0]  << 8) | buf[1]);
    out->accel_y = (int16_t)(((uint16_t)buf[2]  << 8) | buf[3]);
    out->accel_z = (int16_t)(((uint16_t)buf[4]  << 8) | buf[5]);
    out->temp_raw = (int16_t)(((uint16_t)buf[6]  << 8) | buf[7]);
    out->gyro_x  = (int16_t)(((uint16_t)buf[8]  << 8) | buf[9]);
    out->gyro_y  = (int16_t)(((uint16_t)buf[10] << 8) | buf[11]);
    out->gyro_z  = (int16_t)(((uint16_t)buf[12] << 8) | buf[13]);

    self->read_count++;
    return MPU6050_STATUS_OK;
}

static mpu6050_status_t mpu6050_read_data_impl(bsp_mpu6050_driver_t * const self,
                                               mpu6050_data_t * const out)
{
    mpu6050_status_t st;
    mpu6050_raw_data_t raw;

    if (self == NULL || out == NULL) {
        return MPU6050_STATUS_PARAM;
    }

    st = mpu6050_read_raw_impl(self, &raw);
    if (st != MPU6050_STATUS_OK) {
        return st;
    }
    mpu6050_raw_to_phy(&raw, out);
    return MPU6050_STATUS_OK;
}

/* ------------------------------------------------------------------ */
/* 换算与实例化                                                         */
/* ------------------------------------------------------------------ */

void mpu6050_raw_to_phy(const mpu6050_raw_data_t * const raw,
                        mpu6050_data_t           * const out)
{
    if (raw == NULL || out == NULL) {
        return;
    }
    /* 量程换算：SPL 移位风格参考 AHT21，这里用直接除法保证可读性 */
    out->accel_x_g  = (float)raw->accel_x / MPU6050_ACCEL_LSB_PER_G;
    out->accel_y_g  = (float)raw->accel_y / MPU6050_ACCEL_LSB_PER_G;
    out->accel_z_g  = (float)raw->accel_z / MPU6050_ACCEL_LSB_PER_G;
    out->gyro_x_dps = (float)raw->gyro_x / MPU6050_GYRO_LSB_PER_DPS;
    out->gyro_y_dps = (float)raw->gyro_y / MPU6050_GYRO_LSB_PER_DPS;
    out->gyro_z_dps = (float)raw->gyro_z / MPU6050_GYRO_LSB_PER_DPS;
    /* 温度：Temp(℃) = raw/340 + 36.53（Register Map §4.4，适用 PLL 时钟） */
    out->temp_c     = (float)raw->temp_raw / 340.0f + 36.53f;
}

mpu6050_status_t mpu6050_instantiate(bsp_mpu6050_driver_t   * const self,
                                     iic_driver_interface_t * const p_iic,
                                     osal_time_base_t       * const p_timebase,
                                     osal_delay_t           * const p_delay,
                                     const mpu6050_config_t * const cfg)
{
    if (self == NULL || p_iic == NULL) {
        return MPU6050_STATUS_PARAM;
    }
    if (self->inited) {
        return MPU6050_STATUS_OK;   /* 已初始化：幂等返回（对齐 AHT21 IS_INITED 风格） */
    }

    self->p_iic_instance    = p_iic;
    self->p_timebase_instance = p_timebase;
    self->p_delay_instance  = p_delay;
    self->err_count         = 0u;
    self->read_count        = 0u;
    self->device_id         = 0u;

    /* 挂载 ops 表（桥接模式的"实现"注册动作） */
    self->pf_init       = mpu6050_init_impl;
    self->pf_deinit     = mpu6050_deinit_impl;
    self->pf_read_id    = mpu6050_read_id_impl;
    self->pf_read_data  = mpu6050_read_data_impl;
    self->pf_read_raw   = mpu6050_read_raw_impl;
    self->pf_sleep      = mpu6050_sleep_impl;
    self->pf_wakeup     = mpu6050_wakeup_impl;
    self->pf_reset      = mpu6050_reset_impl;

    /* 走初始化流程 */
    return self->pf_init(self, cfg);
}
