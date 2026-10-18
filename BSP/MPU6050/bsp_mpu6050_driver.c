#include "bsp_mpu6050_driver.h"

#define MPU_USE_FIFO 0

#define IIC_MEMADD_SIZE_8BIT        0x00000001U
#define TIME_OUT_MS                 1000
#define MPUXXX_NOT_INITED           0
#define MPUXXX_INITED               1
#define MPUXXX_DATA_PACKET_SIZE     14

static uint8_t g_is_inited = MPUXXX_NOT_INITED;
static uint32_t g_is_dma_readed = 0;

static double accel_scale = 16384.0;  // 加速度量程±2g时的灵敏度缩放因子（LSB/g）
static double gyro_scale = 16.4;  // 陀螺仪量程±2000°/s时的灵敏度缩放因子（LSB/°/s）

#define MPUXXX_WRITE_REG(p_mpu_driver, reg, p_data, len)\
                        p_mpu_driver->p_iic_driver_interface->pf_iic_mem_write( \
                        p_mpu_driver->p_iic_driver_interface->p_iic, \
                        (MPU_ADDR << 1) | 0, \
                        reg, \
                        IIC_MEMADD_SIZE_8BIT,\
                        p_data, \
                        len, \
                        TIME_OUT_MS)

#define MPUXXX_READ_REG(p_mpu_driver, reg, p_data, len) \
                        p_mpu_driver->p_iic_driver_interface->pf_iic_mem_read( \
                        p_mpu_driver->p_iic_driver_interface->p_iic, \
                        (MPU_ADDR << 1) | 1, \
                        reg, \
                        IIC_MEMADD_SIZE_8BIT, \
                        p_data, \
                        len, \
                        TIME_OUT_MS)

inline uint32_t mpuxxx_flag_read(void) {
    return g_is_dma_readed;
}

inline void mpuxxx_flag_set(uint8_t flag) {
    g_is_dma_readed = flag;
}


/**
 * @brief mpuxxx sleep
 *        让 MPU6050 进入睡眠模式
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_sleep(bsp_mpuxxx_driver_t *p_mpu_driver) {
    uint8_t ret = 1;
    uint8_t data = 0;
    data = SLEEP_BIT(1);

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_PWR_MGMT_1_REG, &data, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_sleep: write MPU_PWR_MGMT_1_REG error\r\n");
#endif
        return ret;
    }

    log_i("mpuxxx_sleep");

    return 1;
}

/**
 * @brief mpuxxx wake up
 *        唤醒 MPU6050, 即清除电源管理 1 寄存器的睡眠位
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_wakeup(bsp_mpuxxx_driver_t *p_mpu_driver) {
    uint8_t ret = 1;
    uint8_t data = 0;
    data = SLEEP_BIT(0);

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_PWR_MGMT_1_REG, &data, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_wakeup: write MPU_PWR_MGMT_1_REG error\r\n");
#endif
        return ret;
    }

    log_i("mpuxxx_wakeup");

    return ret;
}

/**
 * @brief set gyro full scale range
 *        设置陀螺仪满量程范围
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] fsr: gyro fsr, 0: ±2000°/s, 1: ±1000°/s, 2: ±500°/s, 3: ±250°/s
 *                 陀螺仪量程, 0: ±2000°/s, 1: ±1000°/s, 2: ±500°/s, 3: ±250°/s
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 * 
 * @note 写入成功后同步更新内部灵敏度缩放因子 gyro_scale(LSB/°/s)
*/
static uint8_t mpuxxx_set_gyro_fsr(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t fsr
) {
    uint8_t ret = 1;
    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_GYRO_CFG_REG, &fsr, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_gyro_fsr: write MPU_GYRO_CFG_REG error\r\n");
#endif
        return ret;
    }

    /* fsr 传进来的是已移位的寄存器值(FS_SEL_BIT(x) = x<<3)，
     * 必须右移 3 位还原成 FS_SEL 编码再比较，否则永远落到 default */
    switch ((fsr >> 3) & 0x03U) {
        case 0x00:
            gyro_scale = 131.0;
            break;
        case 0x01:
            gyro_scale = 65.5;
            break;
        case 0x02:
            gyro_scale = 32.8;
            break;
        case 0x03:
            gyro_scale = 16.4;
            break;
        default:
            gyro_scale = 131.0;
            break;
    }

    log_i("mpuxxx_set_gyro_fsr");

    return ret;
}


/**
 * @brief set accel full scale range
 *        设置加速度计满量程范围
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] fsr: accel fsr, 0: ±2g, 1: ±4g, 2: ±8g, 3: ±16g
 *                 加速度计量程, 0: ±2g, 1: ±4g, 2: ±8g, 3: ±16g
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 * 
 * @note 写入成功后同步更新内部灵敏度缩放因子 accel_scale(LSB/g)
 */
static uint8_t mpuxxx_set_accel_fsr(
    bsp_mpuxxx_driver_t * p_mpu_driver, uint8_t fsr
) {
    uint8_t ret = 1;
    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_ACCEL_CFG_REG, &fsr, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_accel_fsr: write MPU_ACCEL_CFG_REG error\r\n");
#endif
        return ret;
    } 

    /* 同 set_gyro_fsr: fsr 是移位后的寄存器值，先还原成 AFS_SEL 编码 */
    switch ((fsr >> 3) & 0x03U) {
        case 0x00:
            accel_scale = 16384.0;
            break;
        case 0x01:
            accel_scale = 8192.0;
            break;
        case 0x02:
            accel_scale = 4096.0;
            break;
        case 0x03:
            accel_scale = 2048.0;
            break;
        default:
            accel_scale = 16384.0;
            break;
    }

    log_i("mpuxxx_set_accel_fsr");

    return ret;
}


/**
 * @brief set mpu6050 digital low-pass filter -- lpf
 *        设置 MPU6050 数字低通滤波器(DLPF)
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] lpf: digital low-pass filter, 0: 256Hz, 1: 188Hz, 
 *                  2: 98Hz, 3: 42Hz, 4: 20Hz, 5: 10Hz, 6: 5Hz
 *                  数字低通滤波配置, 0: 256Hz, 1: 188Hz, 
 *                  2: 98Hz, 3: 42Hz, 4: 20Hz, 5: 10Hz, 6: 5Hz
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_set_lpf(
    bsp_mpuxxx_driver_t * p_mpu_driver, uint8_t lpf
) {
    uint8_t ret = 1;
    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_CFG_REG, &lpf, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_lpf: write MPU_CFG_REG error\r\n");
#endif
        return ret;
    }

    log_i("mpuxxx_set_lpf");

    return ret;
}


/**
 * @brief set rate, Sample Rate = Gyroscope Output Rate / (1 + SMPLRT_DIV)
 *        设置采样率分频, 采样率 = 陀螺仪输出速率 / (1 + SMPLRT_DIV)
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] rate: rate, 0: 8kHz, 1: 1kHz, 2: 2kHz, 3: 3kHz, 
 *                        4: 4kHz, 5: 5kHz, 6: 6kHz
 *                  采样率分频值, 0: 8kHz, 1: 1kHz, 2: 2kHz, 3: 3kHz, 
 *                        4: 4kHz, 5: 5kHz, 6: 6kHz
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_set_rate(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t rate
) {
    uint8_t ret = 1;

    // Configure sample rate to 50Hz
    // Sample Rate = Gyroscope Output Rate / (1 + SMPLRT_DIV)
    // where Gyroscope Output Rate = 8kHz, when the DLPF is disabled
    // (DLPF_CEG = 0 or 7), and 1kHz when the DLPF is enabled.

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_SAMPLE_RATE_REG, &rate, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_rate: write MPU_SAMPLE_RATE_REG error\r\n");
#endif
        return ret;
    }
#ifdef DEBUG
    log_i("mpuxxx_set_rate");
#endif
    
    return ret;
}

/**
 * @brief set interrupt enable
 *        设置中断使能寄存器
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] enable: interrupt enable, 0: disable, 1: enable
 *                    中断使能位, 0: 关闭, 1: 使能
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_set_interrupt_enable(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t enable
) {
    int8_t ret = 1;

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_INT_EN_REG, &enable, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_interrupt_enable: write MPU_INT_EN_REG error, ret = %d\r\n", ret);
#endif
        return ret;
    }

#ifdef DEBUG
    log_i("mpuxxx_set_interrupt_enable");
#endif

    return ret;
}


/**
 * @brief set motion threshold
 *        设置运动检测阈值
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] threshold: motion threshold, 0: 128, 1: 127, 2: 126, 3: 125, 4: 124, 5: 123, 6: 122, 7: 121
 *                       运动检测阈值, 取值 0: 128, 1: 127, 2: 126, 3: 125, 4: 124, 5: 123, 6: 122, 7: 121
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_set_motion_threshold(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t threshold
) {
    uint8_t ret = 1;

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_MOTION_DET_REG, &threshold, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_motion_threshold: write MPU_MOTION_DET_REG error\r\n");
#endif
        return ret;
    }

#ifdef DEBUG
    log_i("mpuxxx_set_motion_threshold");
#endif

    return ret;
}


/**
 * @brief set INT level
 *        设置 INT 引脚电平极性及中断锁存方式
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] level: INT level, 0: active low, 1: active high
 *                    INT 电平配置, 0: 低电平有效, 1: 高电平有效
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_set_INT_level(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t level
) {
    uint8_t ret = 1;

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_INTBP_CFG_REG, &level, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_int_level: write MPU_INTBP_CFG_REG error\r\n");
#endif
        return ret;
    }

#ifdef DEBUG
    log_i("mpuxxx_set_INT_level");
#endif

    return ret;
}


/**
 * @brief set user ctrl. 
 *        MPU_USER_CTRL_REG: This register allow the user to enable and disable
 *        the FIFO buffer、 IIC master mode and primary IIC interface.
 *        设置用户控制寄存器。
 *        MPU_USER_CTRL_REG: 该寄存器用于使能/关闭 FIFO 缓冲区、IIC 主模式
 *        以及主 IIC 接口
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] data: user ctrl data
 *                  用户控制寄存器待写入的数据
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_set_user_ctrl(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t data
) {
    uint8_t ret = 1;

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_USER_CTRL_REG, &data, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_user_ctrl: write MPU_USER_CTRL_REG error\r\n");
#endif
        return ret;
    }

#ifdef DEBUG
    log_i("mpuxxx_set_user_ctrl");
#endif

    return ret;
}


/**
 * @brief set power management 1 register. 
 *        This register allows the user to configure the power mode
 *        and clock source. It also provides a bit fo resetting the entire device,
 *        and a bit for disabling the temperature sensor.
 *        设置电源管理 1 寄存器。
 *        该寄存器用于配置电源模式与时钟源, 另含一个整机复位位
 *        和一个温度传感器关闭位
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] data: power management 1 data
 *                  电源管理 1 寄存器待写入的数据
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_set_pwr_mgmt1_reg(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t data
) {
    uint8_t ret = 1;

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_PWR_MGMT_1_REG, &data, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_pwr_mgmt1_reg: write MPU_PWR_MGMT_1_REG error\r\n");
#endif
        return ret;
    }

    log_i("mpuxxx_set_pwr_mgmt1_reg");

    return ret;
}

/**
 * @brief set power management 2 register
 *        设置电源管理 2 寄存器(各轴待机控制与低功耗唤醒频率)
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] data: power management 2 data
 *                  电源管理 2 寄存器待写入的数据
 * 
 * @return uint8_t: 1 if success
 *                  1 表示写入成功, 其他值表示写寄存器失败
 */
static uint8_t mpuxxx_set_pwr_mgmt2_reg(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t data
) {
    uint8_t ret = 1;

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_PWR_MGMT_2_REG, &data, 1);
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_pwr_mgmt2_reg: write MPU_PWR_MGMT_2_REG error\r\n");
#endif
        return ret;
    }

    log_i("mpuxxx_set_pwr_mgmt2_reg");

    return ret;
}


/**
 * @brief set fifo en reg
 *        设置 FIFO 使能寄存器, 决定哪些数据被写入 FIFO
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[in] data: fifo en reg data
 *                  FIFO 使能寄存器待写入的数据
 * 
 * @return uint8_t
 *         1 表示写入成功, 其他值表示写寄存器失败
*/
static uint8_t mpuxxx_set_fifo_en_reg(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t data
) {
    uint8_t ret = 1;

    ret = MPUXXX_WRITE_REG(p_mpu_driver, MPU_FIFO_EN_REG, &data, 1);
    if(ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_set_fifo_en_reg write MPU_FIFO_EN_REG error\r\n");
#endif
        return ret;
    }

    log_i("mpuxxx_set_fifo_en_reg");

    return ret;
}


/**
 * @brief get interrupt status reg
 *        读取中断状态寄存器
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[out] p_data: pointer to a uint8_t data
 *                     用于接收中断状态寄存器值的缓冲区
 * 
 * @return uint8_t
 *         1 表示读取成功, 其他值表示读寄存器失败
*/
static uint8_t mpuxxx_get_interrupt_status_reg(
    bsp_mpuxxx_driver_t *p_mpu_driver, uint8_t *p_data
) {
    uint8_t ret = 1;

    ret = MPUXXX_READ_REG(p_mpu_driver, MPU_INT_STA_REG, p_data, 1);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_get_interrupt_status_reg read interrupt status reg error\r\n");
#endif
        return ret;
    }
#ifdef DEBUG
    DEBUG_OUT("get_interrupt_status_reg: %#x\r\n", *p_data);
#endif

#ifdef DEBUG
    log_i("mpuxxx_get_interrupt_status_reg");
#endif

    return ret;
}


/**
 * @brief get temperature
 *        读取温度值
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[out] p_data: pointer to a mpu6050 data structure
 *                     指向 MPU6050 数据结构的指针, 结果写入 temperature 字段(单位 ℃)
 * 
 * @return uint8_t
 *         1 表示读取成功, 其他值表示读寄存器失败
*/
static uint8_t mpuxxx_get_temperature(
    bsp_mpuxxx_driver_t *p_mpu_driver, mpuxxx_data_t *p_data
) {
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_get_temperature\r\n");
#endif
    uint8_t ret = 1;
    uint8_t data[2] = {0};
    int16_t temp = 0;

    ret = MPUXXX_READ_REG(p_mpu_driver, MPU_TEMP_OUTH_REG, data, 2);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_get_temperature read temperature error\r\n");
#endif
        return ret;
    }
    temp = (int16_t)(data[0] << 8 | data[1]);
    p_data->temperature = (float)(temp / 340.0 + 36.53);

    log_i("mpuxxx_get_temperature");

    return ret;
}




/**
 * @brief get gyro
 *        读取三轴陀螺仪原始值并换算为角速度
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[out] p_data: pointer to a mpu6050 data structure
 *                     指向 MPU6050 数据结构的指针, 结果写入
 *                     gyro_x/y/z_raw 与 gx/gy/gz(单位 °/s)
 * 
 * @return uint8_t
 *         1 表示读取成功, 其他值表示读寄存器失败
*/
static uint8_t mpuxxx_get_gyro(
    bsp_mpuxxx_driver_t *p_mpu_driver, mpuxxx_data_t *p_data
) {
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_get_gyro\r\n");
#endif
    uint8_t ret = 1;
	uint8_t data[6] = {0};
	
    ret = MPUXXX_READ_REG(p_mpu_driver, MPU_GYRO_XOUTH_REG, data, 6);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_get_gyro read gyro error\r\n");
#endif
        return ret;
    }
    p_data->gyro_x_raw = (int16_t)(data[0] << 8 | data[1]);
    p_data->gyro_y_raw = (int16_t)(data[2] << 8 | data[3]);
    p_data->gyro_z_raw = (int16_t)(data[4] << 8 | data[5]);

    p_data->gx = (double)(p_data->gyro_x_raw / gyro_scale);
    p_data->gy = (double)(p_data->gyro_y_raw / gyro_scale);
    p_data->gz = (double)(p_data->gyro_z_raw / gyro_scale);

    log_i("mpuxxx_get_gyro");

    return ret;
}



/**
 * @brief get accel
 *        读取三轴加速度原始值并换算为重力加速度
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[out] p_data: pointer to a mpu6050 data structure
 *                     指向 MPU6050 数据结构的指针, 结果写入
 *                     accel_x/y/z_raw 与 ax/ay/az(单位 g)
 * 
 * @return uint8_t
 *         1 表示读取成功, 其他值表示读寄存器失败
*/
static uint8_t mpuxxx_get_accel(
    bsp_mpuxxx_driver_t *p_mpu_driver, mpuxxx_data_t *p_data
) {
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_get_accel\r\n");
#endif
    uint8_t ret = 1;
    uint8_t data[6] = {0};

    ret = MPUXXX_READ_REG(p_mpu_driver, MPU_ACCEL_XOUTH_REG, data, 6);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_get_accel read error\r\n");
#endif
        return ret;
    }
    p_data->accel_x_raw = (int16_t)(data[0] << 8 | data[1]);
    p_data->accel_y_raw = (int16_t)(data[2] << 8 | data[3]);
    p_data->accel_z_raw = (int16_t)(data[4] << 8 | data[5]);

    p_data->ax = (double)(p_data->accel_x_raw / accel_scale);
    p_data->ay = (double)(p_data->accel_y_raw / accel_scale);
    p_data->az = (double)(p_data->accel_z_raw / accel_scale);

    log_i("mpuxxx_get_accel");

    return ret;
}



/**
 * @brief get all data
 *        一次性读取加速度、温度与陀螺仪共 14 字节数据
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[out] p_data: pointer to a mpu6050 data structure
 *                     指向 MPU6050 数据结构的指针, 原始值与换算结果一并返回
 * 
 * @return uint8_t
 *         1 表示读取成功, 其他值表示读寄存器失败
 * 
 * @note 供非 OS 支持场景(轮询)使用; OS 场景请走中断 + FIFO 通路
*/
static uint8_t mpuxxx_get_all_data(
    bsp_mpuxxx_driver_t *p_mpu_driver, mpuxxx_data_t *p_data
) {
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_get_all_data\r\n");
#endif
    uint8_t ret = 1;
    uint8_t data[14] = {0};
    int16_t temp = 0;

    ret = MPUXXX_READ_REG(p_mpu_driver, MPU_ACCEL_XOUTH_REG, data, 14);
	if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_get_all_data read error\r\n");
#endif
        return ret;
    }
	
    p_data->accel_x_raw = (int16_t)(data[0] << 8 | data[1]);
    p_data->accel_y_raw = (int16_t)(data[2] << 8 | data[3]);
    p_data->accel_z_raw = (int16_t)(data[4] << 8 | data[5]);
    
    p_data->ax = (double)(p_data->accel_x_raw / accel_scale);
    p_data->ay = (double)(p_data->accel_y_raw / accel_scale);
    p_data->az = (double)(p_data->accel_z_raw / accel_scale);

	temp = (int16_t)(data[6] << 8 | data[7]);
    p_data->temperature = (float)(temp / 340.0 + 36.53);
									
    p_data->gyro_x_raw = (int16_t)(data[8] << 8 | data[9]);
    p_data->gyro_y_raw = (int16_t)(data[10] << 8 | data[11]);
    p_data->gyro_z_raw = (int16_t)(data[12] << 8 | data[13]);

    p_data->gx = (double)(p_data->gyro_x_raw / gyro_scale);
    p_data->gy = (double)(p_data->gyro_y_raw / gyro_scale);
    p_data->gz = (double)(p_data->gyro_z_raw / gyro_scale);

    log_i("mpuxxx_get_all_data");
    
    return ret;
}

/**
 * @brief fifo init
 *        初始化 FIFO: 复位 FIFO、配置使能项、使能溢出中断
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * 
 * @return uint8_t
 *         1 表示各步骤全部成功, 其他值表示中途某步写寄存器失败
*/
static uint8_t mpuxxx_fifo_init(bsp_mpuxxx_driver_t *p_mpu_driver)
{
    uint8_t ret = 1;

#if 0
    // reset fifo
    ret = mpuxxx_set_user_ctrl(p_mpu_driver, FIFO_RESET_BIT(1));
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_fifo_init reset fifo error\r\n");
#endif
        return ret;
    }

    // Wait for reset to complete
#ifdef OS_SUPPORTING
    p_mpu_driver->p_delay_interface->delay_ms(10);
#else
    p_mpu_driver->p_delay_interface->pf_delay_ms(10);
#endif

    // Configure FIFO enable register, note the configuration order
    ret = mpuxxx_set_fifo_en_reg(p_mpu_driver,
                                 ACCEL_FIFO_EN_BIT(1) |  // Enable accelerometer first
                                 XG_FIFO_EN_BIT(1)    |  // Then gyroscope
                                 YG_FIFO_EN_BIT(1)    |
                                 ZG_FIFO_EN_BIT(1));
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_fifo_init configure fifo enable register error\r\n");
#endif
        return ret;
    }

    // Enable FIFO last
    ret = mpuxxx_set_user_ctrl(p_mpu_driver, FIFO_EN_BIT(1));
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_fifo_init enable fifo error\r\n");
#endif
        return ret;
    }

#endif

    // Configure interrupt
    ret = mpuxxx_set_INT_level(
        p_mpu_driver, INT_RD_CLEAR_BIT(1) | INT_LEVEL_BIT(1)
    );
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_fifo_init configure interrupt error\r\n");
#endif
        return ret;
    }

    // Enable FIFO overflow interrupt
    // ret = mpuxxx_set_interrupt_enable(p_mpu_driver, FIFO_OVERFLOW_EN_BIT(1));
    ret = mpuxxx_set_interrupt_enable(p_mpu_driver, DATA_RDY_EN_BIT(1));
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_fifo_init enable fifo overflow interrupt error\r\n");
#endif
        return ret;
    }

    log_i("mpuxxx_fifo_init");

    return ret;
}


/**
 * @brief read fifo when fifo overflow interrupt occur
 *        FIFO 溢出中断触发时, 按包循环读出 FIFO 中的全部数据
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[out] p_data: pointer to a mpu6050 data structure array
 *                     指向 MPU6050 数据结构数组的指针, 每包占用一个元素
 * 
 * @return uint8_t 
 *         1 表示读取成功, 其他值表示读寄存器失败
 */
static uint8_t mpuxxx_read_fifo_isr_occur(
    bsp_mpuxxx_driver_t *p_mpu_driver, mpuxxx_data_t *p_data
) {
    uint8_t ret = 1;

#if 0  // can not be used, because the fifo data is not correct
uint16_t fifo_count = 0;
    uint16_t fifo_packet_count = 0;
    uint8_t fifo_buffer[12] = {0};

    // Get FIFO count
    ret = MPUXXX_READ_REG(p_mpu_driver, MPU_FIFO_CNTH_REG, fifo_buffer, 2);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_read_fifo_isr_occur read FIFO count error\r\n");
#endif
        return ret;
    }

    fifo_count = (fifo_buffer[0] << 8) | fifo_buffer[1];
    
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_read_fifo_isr_occur fifo_count: %u\r\n", fifo_count);
#endif
    
    // Read FIFO data in chunks of 12 bytes (6 bytes accel + 6 bytes gyro)
    if (fifo_count >= 12)
    {
        fifo_packet_count = fifo_count / 12;

        for (uint16_t i = 0; i < fifo_packet_count; i++)
        {
            ret = MPUXXX_READ_REG(p_mpu_driver, MPU_FIFO_RW_REG, fifo_buffer, 12);
            if (ret != 1)
            {
#ifdef DEBUG
                DEBUG_OUT("mpuxxx_read_fifo read FIFO data error\r\n");
#endif
                return ret;
            }

            // Process accel data
            p_data[i].accel_x_raw = (int16_t)(fifo_buffer[0] << 8 | fifo_buffer[1]);
            p_data[i].accel_y_raw = (int16_t)(fifo_buffer[2] << 8 | fifo_buffer[3]);
            p_data[i].accel_z_raw = (int16_t)(fifo_buffer[4] << 8 | fifo_buffer[5]);

            // Process gyro data
            p_data[i].gyro_x_raw = (int16_t)(fifo_buffer[6]  << 8 | fifo_buffer[7]);
            p_data[i].gyro_y_raw = (int16_t)(fifo_buffer[8]  << 8 | fifo_buffer[9]);
            p_data[i].gyro_z_raw = (int16_t)(fifo_buffer[10] << 8 | fifo_buffer[11]);
            
            // Convert raw data to physical units
            p_data[i].ax = (double)(p_data[i].accel_x_raw / accel_scale);
            p_data[i].ay = (double)(p_data[i].accel_y_raw / accel_scale);
            p_data[i].az = (double)(p_data[i].accel_z_raw / accel_scale);
            
            p_data[i].gx = (double)(p_data[i].gyro_x_raw / gyro_scale);
            p_data[i].gy = (double)(p_data[i].gyro_y_raw / gyro_scale);
            p_data[i].gz = (double)(p_data[i].gyro_z_raw / gyro_scale);
			            
#ifdef DEBUG
		DEBUG_OUT("fifo data:\r\n");
        DEBUG_OUT("ax: %lf, ay: %lf, az: %lf, gx: %lf, gy: %lf, gz: %lf\r\n",\
                  p_data[i].ax,
                  p_data[i].ay,
                  p_data[i].az,
                  p_data[i].gx,
                  p_data[i].gy,
                  p_data[i].gz );
#endif

#endif

    return ret;
}

/**
 * @brief read fifo by reading one packet only can be used when
 *        ACCEL_FIFO_EN_BIT \ XG_FIFO_EN_BIT \ YG_FIFO_EN_BIT \ ZG_FIFO_EN_BIT
 *        are sets
 *        按包读取 FIFO 数据, 仅在 ACCEL_FIFO_EN_BIT \ XG_FIFO_EN_BIT \
 *        YG_FIFO_EN_BIT \ ZG_FIFO_EN_BIT 均置位时可用
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[out] p_data: pointer to a mpu6050 data structure
 *                     指向 MPU6050 数据结构的指针
 * 
 * @return uint8_t 
 *         1 表示读取成功, 其他值表示读寄存器失败
 * 
 */
#if 0
static uint8_t mpuxxx_read_fifo_packet(bsp_mpuxxx_driver_t *p_mpu_driver, mpuxxx_data_t *p_data)
{
    uint8_t ret = 1;
    uint16_t fifo_count = 0;
    uint8_t fifo_buffer[12] = {0};

    // Get FIFO count
    ret = MPUXXX_READ_REG(p_mpu_driver, MPU_FIFO_CNTH_REG, fifo_buffer, 2);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_read_fifo read FIFO count error\r\n");
#endif
        return ret;
    }

    fifo_count = (fifo_buffer[0] << 8) | fifo_buffer[1];
    
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_read_fifo fifo_count: %u\r\n", fifo_count);
#endif
    
    // Read FIFO data in chunks of 12 bytes (6 bytes accel + 6 bytes gyro)

    if (fifo_count >= 12)
    {
        ret = MPUXXX_READ_REG(p_mpu_driver, MPU_FIFO_RW_REG, fifo_buffer, 12);
        if (ret != 1)
        {
#ifdef DEBUG
            DEBUG_OUT("mpuxxx_read_fifo read FIFO data error\r\n");
#endif
            return ret;
        }

        // Process accel data
        p_data->accel_x_raw = (int16_t)(fifo_buffer[0] << 8 | fifo_buffer[1]);
        p_data->accel_y_raw = (int16_t)(fifo_buffer[2] << 8 | fifo_buffer[3]);
        p_data->accel_z_raw = (int16_t)(fifo_buffer[4] << 8 | fifo_buffer[5]);

        // Process gyro data
        p_data->gyro_x_raw = (int16_t)(fifo_buffer[6]  << 8 | fifo_buffer[7]);
        p_data->gyro_y_raw = (int16_t)(fifo_buffer[8]  << 8 | fifo_buffer[9]);
        p_data->gyro_z_raw = (int16_t)(fifo_buffer[10] << 8 | fifo_buffer[11]);
        
        // Convert raw data to physical units
        p_data->ax = (double)(p_data->accel_x_raw / accel_scale);
        p_data->ay = (double)(p_data->accel_y_raw / accel_scale);
        p_data->az = (double)(p_data->accel_z_raw / accel_scale);
        
        p_data->gx = (double)(p_data->gyro_x_raw / gyro_scale);
        p_data->gy = (double)(p_data->gyro_y_raw / gyro_scale);
        p_data->gz = (double)(p_data->gyro_z_raw / gyro_scale);

#ifdef DEBUG
		DEBUG_OUT("fifo data:");
        DEBUG_OUT("ax: %lf, ay: %lf, az: %lf, gx: %lf, gy: %lf, gz: %lf\r\n",\
                  p_data->ax,
                  p_data->ay,
                  p_data->az,
                  p_data->gx,
                  p_data->gy,
                  p_data->gz );
#endif
    }
    
    return ret;
}
#endif



/**
 * @brief read fifo by reading one packet only can be used when
 *        ACCEL_FIFO_EN_BIT \ XG_FIFO_EN_BIT \ YG_FIFO_EN_BIT \ ZG_FIFO_EN_BIT
 *        are sets
 *        按包读取 FIFO 数据, 仅在 ACCEL_FIFO_EN_BIT \ XG_FIFO_EN_BIT \
 *        YG_FIFO_EN_BIT \ ZG_FIFO_EN_BIT 均置位时可用
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * @param[out] p_data: pointer to a mpu6050 data structure
 *                     指向 MPU6050 数据结构数组的指针, 每包占用一个元素
 * 
 * @return uint8_t
 *         1 表示读取成功, 其他值表示读寄存器失败
*/
static uint8_t mpuxxx_read_fifo_packet(
    bsp_mpuxxx_driver_t *p_mpu_driver, mpuxxx_data_t *p_data
) {
    uint8_t ret = 1;
    uint16_t fifo_count = 0;
    uint16_t fifo_packet_count = 0;
    uint8_t fifo_buffer[12] = {0};

    // Get FIFO count 
    ret = MPUXXX_READ_REG(p_mpu_driver, MPU_FIFO_CNTH_REG, fifo_buffer, 2);
    if (ret != 1) {
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_read_fifo_packet read FIFO count error");
#endif

        return ret;
    }

    fifo_count = (fifo_buffer[0] << 8 | fifo_buffer[1]);

#ifdef DEBUG
    DEBUG_OUT("mpuxxx_read_fifo_packet fifo_count = %u", fifo_count);
#endif

    // Read FIFO data in chunks of 12 bytes (6 bytes accel + 6 bytes gyro)
    if (fifo_count >= 12) {
        fifo_packet_count = fifo_count / 12;
    
        for (uint16_t i = 0; i < fifo_packet_count; i++) {
            ret = MPUXXX_READ_REG(p_mpu_driver, MPU_FIFO_RW_REG, fifo_buffer, 12);
            if (ret != 1) {
#ifdef DEBUG
                DEBUG_OUT("mpuxxx_read_fifo_packet read fifo data error");
#endif

                return ret;
            }

            // Process accel data 
            p_data[i].accel_x_raw = (int16_t)(fifo_buffer[0] << 8 | fifo_buffer[1]);
            p_data[i].accel_x_raw = (int16_t)(fifo_buffer[2] << 8 | fifo_buffer[3]);
            p_data[i].accel_x_raw = (int16_t)(fifo_buffer[4] << 8 | fifo_buffer[5]);

            // Process gyro data
            p_data[i].gyro_x_raw = (int16_t)(fifo_buffer[6] << 8 | fifo_buffer[7]);
            p_data[i].gyro_x_raw = (int16_t)(fifo_buffer[8] << 8 | fifo_buffer[9]);
            p_data[i].gyro_x_raw = (int16_t)(fifo_buffer[10] << 8 | fifo_buffer[11]);

            // Convert raw data to physical units
            p_data[i].ax = (double)(p_data[i].accel_x_raw / accel_scale);
            p_data[i].ay = (double)(p_data[i].accel_y_raw / accel_scale);
            p_data[i].az = (double)(p_data[i].accel_z_raw / accel_scale);

            p_data[i].gx = (double)(p_data[i].gyro_x_raw / gyro_scale);
            p_data[i].gy = (double)(p_data[i].gyro_y_raw / gyro_scale);
            p_data[i].gz = (double)(p_data[i].gyro_z_raw / gyro_scale);
        
#ifdef DEBUG
            DEBUG_OUT("fifo data:");
            DEBUG_OUT("ax: %lf, ay: %lf, az: %lf, gx: %lf, gy: %lf, gz: %lf",
                       p_data[i].ax, p_data[i].ay, p_data[i].az,
                       p_data[i].gx, p_data[i].gy, p_data[i].gz );
#endif
        }
    }

    return ret;
}


/**
 * @brief mpu6050 motioin init
 *        初始化 MPU6050 运动检测功能
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * 
 * @return uint8_t
 *         1 表示各步骤全部成功, 其他值表示中途某步写寄存器失败
 */
static uint8_t mpuxxx_motion_init(bsp_mpuxxx_driver_t *p_mpu_driver) {
    uint8_t ret = 1;

    // motion detect threshold
    ret = mpuxxx_set_motion_threshold(p_mpu_driver, MOT_THR_BIT(0x10));
    if (ret != 1) {

#ifdef DEBUG
    DEBUG_OUT("mpuxxx_motion_init write (MPU_MOTION_DET_REG) \
                                   motion detect reg error");
#endif
    return ret;
    }

    // motion detect interrupt active low
    ret = mpuxxx_set_INT_level(p_mpu_driver, 
                                (INT_RD_CLEAR_BIT(1) | INT_LEVEL_BIT(1)) );
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_motion_init write (MPU_INTBP_CFG_REG) \
                                            motion detect reg error");
#endif
        return ret;
    }

    // Enable motion detect interrupt and enable data ready interrupt(0x41)
    // data = 0x41;

    // enable motion detect interrupt(0x40)
    ret = mpuxxx_set_interrupt_enable(p_mpu_driver, MOT_EN_BIT(1));
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_motion_init write (MPU_INTBP_CFG_REG) \
                                        motion detect reg error");
#endif
        return ret;
    }

    return ret;
}


/**
 * @brief mpuxxx init
 *        MPU6050 芯片初始化: 复位、唤醒、配置量程/采样率/DLPF/中断并校验器件 ID
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * 
 * @return mpuxxx_satus_t
 *         1 表示初始化成功, 其他值表示中途某步失败
 */
static uint8_t mpuxxx_init(bsp_mpuxxx_driver_t *p_mpu_driver) {
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_init start");
#endif

    uint8_t ret = 1;
    uint8_t id = 0;

    // Initialize I2C
    p_mpu_driver->p_iic_driver_interface->pf_iic_init();

    // Reset MPU6050
    // Reset bit
    ret = mpuxxx_set_pwr_mgmt1_reg(p_mpu_driver, DEVICE_RESET_BIT(1));
    if (ret != 1) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init RESET error");
#endif

        return ret;
    }

    log_i("mpuxxx_init RESET success");

#ifdef OS_SUPPORTING
    p_mpu_driver->p_delay_interface->delay_ms(100);
#endif

    // Wake up MPU6050
    // Clear sleep bit
    ret = mpuxxx_wakeup(p_mpu_driver);
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init WAKE UP error");
#endif
        return ret;
    }
    log_i("mpuxxx_init WAKE UP success");

    // Configure Gyroscope for ±2000 dps (0x03 << 3)
    ret = mpuxxx_set_gyro_fsr(p_mpu_driver, FS_SEL_BIT(0x03));
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init gyro config error");
#endif
        return ret;
    }
    log_i("mpuxxx_init gyro config success: ±2000 dps");


    // Configure accelerometer for ±2g (0x00 << 3)
    ret = mpuxxx_set_accel_fsr(p_mpu_driver, AFS_SEL_BIT(0x00));
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init accel config error");
#endif
        return ret;
    }
    log_i("mpuxxx_init accel config success: ±2g");

    // Configure sample rate to 50Hz
    // Sample rate = Gyroscope output rate / (1 + SMPLRT_DIV)
    // where Gyroscope output rate = 8kHz when the DLPF is disable (DLPF_CFG = 0 or 7),
    // and 1kHz when the DLPF is enabled
    // 50Hz = 1000 / (1 + 19), so SMPLRT_DIV = 19
    // 0x04 : 200Hz
    ret = mpuxxx_set_rate(p_mpu_driver, SMPLRT_DIV_BIT(0x19));
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init sample rate config error");
#endif
        return ret;
    }
    log_i("mpuxxx_init sample rate config success: 50Hz");

    // Configure DLPF, This value should be half of the sampling rate
    // when the sample rate is 50Hz, the DLPF should be 25Hz
    ret = mpuxxx_set_lpf(p_mpu_driver, DLPF_CFG_BIT(0x04));
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init DLPF config error");
#endif
        return ret;
    }
    log_i("mpuxxx_init DLPF config success: 25Hz");

#if 0
    // Disable all interrupts
    ret = mpuxxx_set_interrupt_enable(p_mpu_driver, COLOSE_ALL);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init interrupt disable error");
#endif
        return ret;
    }
#endif

#if 1
    // Open data ready interrupt
    ret = mpuxxx_set_interrupt_enable(p_mpu_driver, DATA_RDY_EN_BIT(1));
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init interrupt disable error");
#endif
        return ret;
    }
#endif
    log_i("mpuxxx_init interrupt disable success");


    // Verify device ID
    ret = MPUXXX_READ_REG(p_mpu_driver, MPU_DEVICE_ID_REG, &id, 1);
    if (1 != ret || id != MPU_ID) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init device ID verification failed, ID = %#x", id);
#endif
        return 0;      
    }
    // log_i("mpuxxx_init device ID verification success, ID = %#x", id);

#ifdef DEBUG
    DEBUG_OUT("mpuxxx_init completed successfully, device ID = %#x\r\n", id);
#endif

    // Set clock source to PLL with X axis gyroscope reference
    ret = mpuxxx_set_pwr_mgmt1_reg(p_mpu_driver, CLKSEL_BIT(0x01));
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init CLKSEL error");
#endif
        return ret;     
    }
    log_i("mpuxxx_init CLKSEL success");


    // Disable gyroscope standby
    ret = mpuxxx_set_pwr_mgmt2_reg(p_mpu_driver,
                                    LP_WAKE_CTRL_BIT(0) |
                                    STBY_XA_BIT(0)      |
                                    STBY_YA_BIT(0)      |
                                    STBY_ZA_BIT(0)      |
                                    STBY_XG_BIT(0)      |
                                    STBY_YG_BIT(0)      |
                                    STBY_ZG_BIT(0) );
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_init gyro standby error");
#endif
        return ret;           
    }

    return 1;
}


/**
 * @brief mpu6050 deinit
 *        MPU6050 去初始化, 清零已初始化标志
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * 
 * @return uint8_t
 *         1 表示成功, 0 表示此前并未初始化
 */
static uint8_t mpuxxx_deinit(bsp_mpuxxx_driver_t *p_mpu_driver) {
    if (MPUXXX_NOT_INITED == g_is_inited) {
#ifdef DEBUG
        DEBUG_OUT("bsp_mpuxxx_driver_deinit not inited");
#endif
        return 0;   
    }

    g_is_inited = MPUXXX_NOT_INITED;
    return 1;
}



/**
 * @brief mpuxxx INT interrupt callback
 *        MPU6050 INT 引脚中断回调: 非 OS 场景直接读取全部数据;
 *        OS 场景关闭中断、读中断状态寄存器并启动 DMA 读取一个数据包
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * 
 * @return void
 */
void int_interrupt_callback(void *mpu_driver, void *mpu_data) {
#ifdef DEBUG
    DEBUG_OUT("====init_interrupt_callback start=====\r\n");
#endif

    uint8_t ret = 1;
    bsp_mpuxxx_driver_t *p_mpu_driver = NULL;

    if (NULL == mpu_driver) {
#ifdef DEBUG
    DEBUG_OUT("init_interrupt_callback parameter error\r\n");
#endif       
    }

    p_mpu_driver = (bsp_mpuxxx_driver_t *)mpu_driver;

    // if not os supporting, get all data
#ifndef OS_SUPPORTING
    ret = mpuxxx_get_all_data(p_mpu_driver, (mpuxxx_data_t *)mpu_data);
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("int_interrupt_callback mpuxxx_get_all_data error\r\n");
        DEBUG_OUT("ret = %d\r\n", ret);
#endif
    }
#else
    // 1. get buffer address
    uint8_t *wbuff = NULL;
    uint8_t data = 0;
    
    wbuff = circular_buf.pf_get_wbuffer_addr(&circular_buf);
#ifdef DEBUG
    DEBUG_OUT("int_interrupt_callback wbuffer = %p\r\n", wbuff);
#endif

    // 2. close interrupt of mpuxxx
    ret = mpuxxx_set_interrupt_enable(p_mpu_driver, COLOSE_ALL);
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("int_interrupt_callback write interrupt enable reg error\r\n");
        DEBUG_OUT("ret = %d\r\n", ret);
#endif
    }

    // 3. Read the status of interrupt register
    ret = mpuxxx_get_interrupt_status_reg(p_mpu_driver, &data);
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("int_interrupt_callback read inter reg data 11 error\r\n");
        DEBUG_OUT("ret = %u\r\n", ret);
#endif
    }

#ifdef DEBUG
    DEBUG_OUT("int_interrupt_callback read inter reg data 22 = %#x\r\n", data);
#endif
    // get timestamp
    uint32_t timestamp_start = 0;
    p_mpu_driver->p_time_base_interface->get_tick_ms(&timestamp_start);

#ifdef DEBUG
    DEBUG_OUT("get timestamp start : %lu\r\n", timestamp_start);
#endif

	// read data by dma
    ret = p_mpu_driver->p_iic_driver_interface->pf_iic_mem_read_dma(
                                    p_mpu_driver->p_iic_driver_interface->p_iic, 
                                    (MPU_ADDR << 1) | 1, 
                                    MPU_ACCEL_XOUTH_REG, 
                                    IIC_MEMADD_SIZE_8BIT, 
                                    wbuff, 
                                    MPU6050_DATA_PACKET_SIZE);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("int_interrupt_callback read accel data error\r\n");
        DEBUG_OUT("ret = %u\r\n", ret);
#endif
    }
#endif /* End of OS_SUPPORTING */

#ifdef DEBUG
    DEBUG_OUT("=====int_interrupt_callback end=====\r\n");
#endif
}


/**
 * @brief mpu6050 dma interrupt callback
 *        MPU6050 DMA 传输完成中断回调: 记录时间戳、重新打开数据就绪中断、
 *        推进循环缓冲区写指针并置位 DMA 接收完成标志
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                         指向 MPU6050 驱动实例
 * 
 * @return void
*/
void dma_interrupt_callback(void *mpu_driver, void *mpu_data) {
#ifdef DEBUG
    DEBUG_OUT("----------dma_interrupt_callback start----------\r\n");
#endif // DEBUG

    int8_t ret = 1;
    bsp_mpuxxx_driver_t *p_mpu_driver = NULL;
    
    if (NULL == mpu_driver)
    {
#ifdef DEBUG
    DEBUG_OUT("int_interrupt_callback parameter error\r\n");
#endif // DEBUG
    }

    p_mpu_driver = (bsp_mpuxxx_driver_t *)mpu_driver;

    uint32_t timestamp_end = 0;

    p_mpu_driver->p_time_base_interface->get_tick_ms(&timestamp_end);

#ifdef DEBUG
    DEBUG_OUT("get timestamp end: %lu\r\n", timestamp_end);
#endif // End of DEBUG

    // Open data ready interrupt
    ret = mpuxxx_set_interrupt_enable(p_mpu_driver, DATA_RDY_EN_BIT(1));
    if (1 != ret) {
#ifdef DEBUG
        DEBUG_OUT("dma_interrupt_callback open interrupt error\r\n");
        DEBUG_OUT("ret = %d\r\n", ret);
#endif // DEBUG       
    }

#ifdef OS_SUPPORTING
    // change the buffer address
    circular_buf.pf_data_writed(&circular_buf);

/*********************************************************/

#if 0 // queue test
    // notify the handler
    if (p_mpu_driver->queue_handle == NULL)
    {
#ifdef DEBUG
        DEBUG_OUT("queue_handle is NULL\r\n");
#endif
    }
    uint8_t tx_data = 1;
    ret = p_mpu_driver->p_os_interface->os_queue_put_isr(
                                    p_mpu_driver->queue_handle,
                                    &tx_data,
                                    NULL
                                    );
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("dma_interrupt_callback put queue error\r\n");
        DEBUG_OUT("ret = %d\r\n", ret);
#endif // DEBUG
    }
#endif /* End of OS_SUPPORTING */

#endif // end of queue test

/*********************************************************/

#if 0 // binary test
    ret = p_mpu_driver->p_os_interface->os_semaphore_signal_binary_isr(p_mpu_driver->semaphore_binary_handle, NULL);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("dma_interrupt_callback give semaphore error\r\n");
        DEBUG_OUT("ret = %d\r\n", ret);
#endif // DEBUG
    }
#endif // end of  binary test

/*********************************************************/

#if 0 // notify test

    ret = p_mpu_driver->p_os_interface->os_semaphore_signal_notify_isr(
                                notify_handle,
                                1,
                                eSetValueWithOverwrite,
                                NULL);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("dma_interrupt_callback give semaphore error\r\n");
        DEBUG_OUT("ret = %d\r\n", ret);
#endif
    }
#endif // End of notify test


/*********************************************************/
#if 1 // global variable test
    g_is_dma_readed = 1;

#endif // End of notify test

#ifdef DEBUG
    DEBUG_OUT("-----dma_interrupt_callback end-----\r\n");
#endif

}


/**
 * @brief mpu6050 driver init
 *        MPU6050 驱动初始化: 调用芯片初始化, 随后配置 FIFO
 * 
 * @param[in] p_mpu_driver: pointer to a mpu6050 driver structure
 *                        指向 MPU6050 驱动实例
 * 
 * @return uint8_t
 *         1 表示成功, 0 表示已初始化过, 其他值表示初始化失败
 */
static uint8_t bsp_mpuxxx_driver_init(bsp_mpuxxx_driver_t *p_mpu_driver) {
    uint8_t ret = 1;

    if (g_is_inited == MPUXXX_INITED)
    {
#ifdef DEBUG
        DEBUG_OUT("bsp_mpuxxx_driver_init already inited\r\n");
#endif
        return 0;
    }

#ifdef DEBUG
    DEBUG_OUT("bsp_mpuxxx_driver_init start\r\n");
#endif

    ret = mpuxxx_init(p_mpu_driver);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("bsp_mpuxxx_driver_init mpuxxx_init error\r\n");
#endif
        return ret;
    }

#if 0
    ret = mpuxxx_motion_init(p_mpu_driver);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("bsp_mpuxxx_driver_init mpuxxx_init error");
#endif
        return ret;
    }
#endif

   ret = mpuxxx_fifo_init(p_mpu_driver);
   if (ret != 1)
   {
#ifdef DEBUG
       DEBUG_OUT("bsp_mpuxxx_driver_init mpuxxx_fifo_init error\r\n");
#endif
       return ret;
   }

#ifdef DEBUG
    DEBUG_OUT("bsp_mpuxxx_driver_init end\r\n");
#endif
    return 1;   
}


/**
 * @brief mpu6050 driver instance
 *        MPU6050 驱动实例化: 挂载各 OSAL 接口与回调, 校验接口完整性后完成初始化
 * 
 * @param[in] p_mpuxxx_driver: pointer to a mpu6050 driver structure
 *                             指向 MPU6050 驱动实例
 * @param[in] p_iic_driver_interface: pointer to a iic driver interface
 *                                    指向 IIC 驱动接口
 * @param[in] p_yield_interface: pointer to a yield interface
 * @param[in] p_os_interfece: pointer to a os interface
 * @param[in] p_timebase_interface: pointer to a timebase interface
 * @param[in] callback_register: pointer to a callback function
 *                              注册 INT 中断回调
 * @param[in] callback_register_dma: pointer to a callback function
 *                                  注册 DMA 传输完成回调
 * 
 * @return uint8_t
 *         1 表示成功, 2 表示入参为空, 3 表示某个接口未挂载完整
 * 
 * @note 原注释列的 p_yield_interface / p_os_interfece / p_timebase_interface
 *       已不是本函数的形参, 实际接口以函数定义为准
*/
uint8_t bsp_mpuxxx_driver_inst (
        bsp_mpuxxx_driver_t                 *p_mpuxxx_driver,
        mpu6050_iic_driver_interface_t      *p_iic_driver_interface
#ifdef OS_SUPPORTING
,
        osal_delay_t * p_delay_interface,
        osal_loop_delay_t * p_loop_delay_interface,
        osal_thread_t * p_thread_interface,
        osal_queue_t * p_queue_interface,
        osal_mutex_t * p_mutex_interface,
        osal_sem_t * p_semaphore_interface,
        osal_notify_t * p_notify_interface,
        osal_critical_t * p_critical_interface,
        osal_time_base_t * p_time_base_interface,
        void (*callback_register)       (void (*callback)(void *, void *)),
        void (*callback_register_dma)   (void (*callback)(void *, void *)),
        void * queue_handle,
        void * mutex_handle,
        void * semaphore_handle,
        void * notify_handle
#endif /* End of OS_SUPPORTING */
) {
#ifdef DEBUG
    DEBUG_OUT("bsp_mpuxxx_driver_inst start\r\n");
#endif

    uint8_t ret = 1;

    if (NULL == p_mpuxxx_driver        || 
        NULL == p_iic_driver_interface
#ifdef OS_SUPPORTING
        || NULL == p_delay_interface
        || NULL == p_loop_delay_interface
        || NULL == p_thread_interface
        || NULL == p_queue_interface
        || NULL == p_mutex_interface
        || NULL == p_semaphore_interface
        || NULL == p_notify_interface
        || NULL == p_critical_interface
        || NULL == p_time_base_interface 
#endif /* End of OS_SUPPORTING */
        || NULL == callback_register 
        || NULL == callback_register_dma)
    {
#ifdef DEBUG
        DEBUG_OUT("bsp_mpuxxx_driver_inst parameter error\r\n");
#endif
        ret = 2;
    }

    p_mpuxxx_driver->p_iic_driver_interface = p_iic_driver_interface;

    if (NULL == p_iic_driver_interface->pf_iic_init      || 
        NULL == p_iic_driver_interface->pf_iic_deinit    ||
        NULL == p_iic_driver_interface->pf_iic_mem_write ||
        NULL == p_iic_driver_interface->pf_iic_mem_read  ||
        NULL == p_iic_driver_interface->pf_iic_mem_read_dma)
    {
#ifdef DEBUG
        DEBUG_OUT("bsp_mpuxxx_driver_inst iic driver interface error\r\n");
#endif
        ret = 3;
    }

#ifdef OS_SUPPORTING
    p_mpuxxx_driver->p_delay_interface = p_delay_interface;

    if (NULL == p_mpuxxx_driver->p_delay_interface->delay_ms)
    {
#ifdef DEBUG
        DEBUG_OUT("bsp_mpuxxx_driver_inst yield interface or os interface error\r\n");
#endif
        ret = 3;
    }

	p_mpuxxx_driver->p_loop_delay_interface = p_loop_delay_interface;
    p_mpuxxx_driver->p_thread_interface = p_thread_interface;
    p_mpuxxx_driver->p_queue_interface = p_queue_interface;
    p_mpuxxx_driver->p_mutex_interface = p_mutex_interface;
    p_mpuxxx_driver->p_semaphore_interface = p_semaphore_interface;
    p_mpuxxx_driver->p_notify_interface = p_notify_interface;
    p_mpuxxx_driver->p_critical_interface = p_critical_interface;
    p_mpuxxx_driver->p_time_base_interface = p_time_base_interface;


#if 0
    if (NULL == p_mpuxxx_driver->p_os_interface->os_queue_create            ||
        NULL == p_mpuxxx_driver->p_os_interface->os_queue_delete            ||
        NULL == p_mpuxxx_driver->p_os_interface->os_queue_put               ||
		NULL == p_mpuxxx_driver->p_os_interface->os_queue_put_isr           ||
        NULL == p_mpuxxx_driver->p_os_interface->os_queue_get               ||
        NULL == p_mpuxxx_driver->p_os_interface->os_semaphore_create_mutex  ||
        NULL == p_mpuxxx_driver->p_os_interface->os_semaphore_delete_mutex  ||
        NULL == p_mpuxxx_driver->p_os_interface->os_semaphore_lock_mutex    ||
        NULL == p_mpuxxx_driver->p_os_interface->os_semaphore_unlock_mutex  ||
        NULL == p_mpuxxx_driver->p_os_interface->os_semaphore_create_binary ||
        NULL == p_mpuxxx_driver->p_os_interface->os_semaphore_delete_binary ||
        NULL == p_mpuxxx_driver->p_os_interface->os_semaphore_wait_binary   ||
        NULL == p_mpuxxx_driver->p_os_interface->os_semaphore_signal_binary   )
    {
#ifdef DEBUG
        DEBUG_OUT("bsp_mpuxxx_driver_inst os interface error");
#endif
        ret = 3;
    }
#endif /* end of 0 */

#endif /* End of OS_SUPPORTING */

//     p_mpuxxx_driver->p_loop_delay_interface = p_loop_delay_interface;

//     if (
//         NULL == p_mpuxxx_driver->p_loop_delay_interface->loop_delay_us   ||
//         NULL == p_mpuxxx_driver->p_loop_delay_interface->loop_delay_ms)
//     {
// #ifdef DEBUG
//         DEBUG_OUT("bsp_mpuxxx_driver_inst delay interface error");
// #endif
//         ret = 3;
//     }

//     p_mpuxxx_driver->p_time_base_interface = p_time_base_interface;

//     if (NULL == p_mpuxxx_driver->p_time_base_interface->get_tick_ms)
//     {
// #ifdef DEBUG
//         DEBUG_OUT("bsp_mpuxxx_driver_inst timebase interface error");
// #endif
//         ret = 3;
//     }

    p_mpuxxx_driver->pf_sleep                    = mpuxxx_sleep;
    p_mpuxxx_driver->pf_wakeup                   = mpuxxx_wakeup;
    p_mpuxxx_driver->pf_deinit                   = mpuxxx_deinit;
    p_mpuxxx_driver->pf_set_gyro_fsr             = mpuxxx_set_gyro_fsr;
    p_mpuxxx_driver->pf_set_accel_fsr            = mpuxxx_set_accel_fsr;
    p_mpuxxx_driver->pf_set_lpf                  = mpuxxx_set_lpf;
    p_mpuxxx_driver->pf_set_rate                 = mpuxxx_set_rate;
    p_mpuxxx_driver->pf_set_interrupt_enable     = mpuxxx_set_interrupt_enable;
    p_mpuxxx_driver->pf_set_motion_threshold     = mpuxxx_set_motion_threshold;
    p_mpuxxx_driver->pf_set_INT_level            = mpuxxx_set_INT_level;
    p_mpuxxx_driver->pf_set_user_ctrl            = mpuxxx_set_user_ctrl;
    p_mpuxxx_driver->pf_set_pwr_mgmt1_reg        = mpuxxx_set_pwr_mgmt1_reg;
    p_mpuxxx_driver->pf_set_pwr_mgmt2_reg        = mpuxxx_set_pwr_mgmt2_reg;
    p_mpuxxx_driver->pf_set_fifo_en_reg          = mpuxxx_set_fifo_en_reg;
    p_mpuxxx_driver->pf_get_temperature          = mpuxxx_get_temperature;
    p_mpuxxx_driver->pf_get_accel                = mpuxxx_get_accel;
    p_mpuxxx_driver->pf_get_gyro                 = mpuxxx_get_gyro;
    p_mpuxxx_driver->pf_get_all_data             = mpuxxx_get_all_data;
    p_mpuxxx_driver->pf_get_interrupt_status_reg = mpuxxx_get_interrupt_status_reg;
    p_mpuxxx_driver->pf_read_fifo_packet         = mpuxxx_read_fifo_packet;
    p_mpuxxx_driver->pf_read_fifo_isr_occur      = mpuxxx_read_fifo_isr_occur;
    
    p_mpuxxx_driver->queue_handle = queue_handle;
    p_mpuxxx_driver->semaphore_mutex_handle = mutex_handle;
    p_mpuxxx_driver->semaphore_binary_handle = semaphore_handle;
    p_mpuxxx_driver->notify_handle = notify_handle;

    ret = bsp_mpuxxx_driver_init(p_mpuxxx_driver);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("bsp_mpuxxx_driver_inst bsp_mpuxxx_driver_init error\r\n");
#endif
        return ret;
    }
    
    callback_register(int_interrupt_callback);
    callback_register_dma(dma_interrupt_callback);
        
    g_is_inited = MPUXXX_INITED;

#ifdef DEBUG
    DEBUG_OUT("bsp_mpuxxx_driver_inst end\r\n");
#endif

    return ret;
}
