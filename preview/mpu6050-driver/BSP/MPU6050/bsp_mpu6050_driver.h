/**
 * @file    bsp_mpu6050_driver.h
 * @brief   MPU6050 六轴传感器驱动 · 设备逻辑层（桥接模式 · 抽象侧）
 *
 * 分层与 AHT21 驱动（BSP/AHT21/）完全一致：
 *   - 本层只实现 MPU6050 器件协议（寄存器、初始化序列、数据解码），
 *     所有总线访问经 iic_driver_interface_t 抽象契约完成，不直接触碰硬件；
 *   - 上层 Handler（bsp_mpu6050_handler）负责队列/时基/回调编排；
 *   - 换总线（软件 I2C -> 硬件 I2C）或换平台时，本层零改动。
 *
 * 错误码风格对齐 aht21_status_t。
 *
 * 器件：InvenSense MPU-6050（GY-521 模块，AD0 接地，7 位地址 0x68）
 * 参考手册：MPU-6000/MPU-6050 Register Map and Descriptions (v4.2)
 */
#ifndef __BSP_MPU6050_DRIVER_H
#define __BSP_MPU6050_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "iic_interface.h"
#include "osal_interface.h"

/* ------------------------------------------------------------------ */
/* 器件常量（Register Map v4.2）                                        */
/* ------------------------------------------------------------------ */
#define MPU6050_I2C_ADDR_7BIT         0x68u   /*!< AD0 = GND 默认地址 */
#define MPU6050_WHO_AM_I_VALUE        0x68u   /*!< WHO_AM_I 应答值    */

/* 寄存器地址 */
#define MPU6050_REG_SMPLRT_DIV        0x19u   /*!< 采样率分频     */
#define MPU6050_REG_CONFIG            0x1Au   /*!< DLPF 配置      */
#define MPU6050_REG_GYRO_CONFIG       0x1Bu   /*!< 陀螺仪量程     */
#define MPU6050_REG_ACCEL_CONFIG      0x1Cu   /*!< 加速度计量程   */
#define MPU6050_REG_FIFO_EN           0x23u   /*!< FIFO 使能      */
#define MPU6050_REG_INT_PIN_CFG       0x37u   /*!< 中断引脚配置   */
#define MPU6050_REG_INT_ENABLE        0x38u   /*!< 中断使能       */
#define MPU6050_REG_ACCEL_XOUT_H      0x3Bu   /*!< 数据区起始(14字节) */
#define MPU6050_REG_PWR_MGMT_1        0x6Bu   /*!< 电源管理 1     */
#define MPU6050_REG_PWR_MGMT_2        0x6Cu   /*!< 电源管理 2     */
#define MPU6050_REG_WHO_AM_I          0x75u   /*!< 器件身份       */
#define MPU6050_REG_SIGNAL_PATH_RESET 0x68u   /*!< 信号路径复位   */

/* PWR_MGMT_1 位域 */
#define MPU6050_PWR1_DEVICE_RESET     0x80u
#define MPU6050_PWR1_SLEEP            0x40u
#define MPU6050_PWR1_CLKSEL_PLL_XGYRO 0x01u

/* 量程换算（默认 ±2g / ±250°/s） */
#define MPU6050_ACCEL_LSB_PER_G       16384.0f   /*!< ±2g  量程 */
#define MPU6050_GYRO_LSB_PER_DPS      131.0f     /*!< ±250°/s 量程 */

/* 时序参数（毫秒） */
#define MPU6050_POWERUP_STABLE_MS     100u   /*!< 复位后振荡器稳定等待 */
#define MPU6050_WAKEUP_STABLE_MS      30u    /*!< 解除睡眠后等待       */

/* ------------------------------------------------------------------ */
/* 错误码（风格对齐 aht21_status_t）                                     */
/* ------------------------------------------------------------------ */
typedef enum {
    MPU6050_STATUS_OK            = 0x00,
    MPU6050_STATUS_ERROR         = 0x01,   /*!< 一般错误               */
    MPU6050_STATUS_PARAM         = 0x02,   /*!< 参数错误(NULL 指针等)  */
    MPU6050_STATUS_NOT_INIT      = 0x03,   /*!< 尚未初始化             */
    MPU6050_STATUS_ERRORTIMEOUT  = 0x04,   /*!< 通信/等待超时          */
    MPU6050_STATUS_ERRORBUS      = 0x05,   /*!< 总线 NACK/卡死         */
    MPU6050_STATUS_BAD_ID        = 0x06,   /*!< WHO_AM_I 校验失败      */
    MPU6050_STATUS_RESERVED      = 0x7FFFFFFF
} mpu6050_status_t;

/* ------------------------------------------------------------------ */
/* 数据结构                                                             */
/* ------------------------------------------------------------------ */

/** 原始 16-bit 读数（寄存器原始值，大端拼接后） */
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp_raw;
} mpu6050_raw_data_t;

/** 换算后的物理量 */
typedef struct {
    float accel_x_g;    /*!< 单位 g               */
    float accel_y_g;
    float accel_z_g;
    float gyro_x_dps;   /*!< 单位 °/s             */
    float gyro_y_dps;
    float gyro_z_dps;
    float temp_c;       /*!< 单位 ℃              */
} mpu6050_data_t;

/** 配置参数（初始化时传入，全部带默认值） */
typedef struct {
    uint8_t smplrt_div;     /*!< 采样率分频，Fs = 1kHz/(1+div)，默认 9(100Hz) */
    uint8_t dlpf_cfg;       /*!< DLPF 配置 0-6，默认 3(44Hz)                  */
    uint8_t gyro_fs_sel;    /*!< 陀螺量程 0-3，默认 0(±250°/s)                */
    uint8_t accel_afs_sel;  /*!< 加速度量程 0-3，默认 0(±2g)                  */
} mpu6050_config_t;

/** 驱动实例（对齐 bsp_aht21_driver_t 的组织方式） */
typedef struct bsp_mpu6050_driver {
    /* ops 表：instantiate 时挂载 */
    mpu6050_status_t (*pf_init)(struct bsp_mpu6050_driver * const self,
                                const mpu6050_config_t * const cfg);
    mpu6050_status_t (*pf_deinit)(struct bsp_mpu6050_driver * const self);
    mpu6050_status_t (*pf_read_id)(struct bsp_mpu6050_driver * const self,
                                   uint8_t * const p_id_out);
    mpu6050_status_t (*pf_read_data)(struct bsp_mpu6050_driver * const self,
                                     mpu6050_data_t * const out);
    mpu6050_status_t (*pf_read_raw)(struct bsp_mpu6050_driver * const self,
                                    mpu6050_raw_data_t * const out);
    mpu6050_status_t (*pf_sleep)(struct bsp_mpu6050_driver * const self);
    mpu6050_status_t (*pf_wakeup)(struct bsp_mpu6050_driver * const self);
    mpu6050_status_t (*pf_reset)(struct bsp_mpu6050_driver * const self);

    /* 依赖注入：总线契约 + OSAL 时基/延时 */
    iic_driver_interface_t *p_iic_instance;
    osal_time_base_t       *p_timebase_instance;
    osal_delay_t           *p_delay_instance;

    /* 实例状态（per-instance，规避 AHT21 的全局单例问题） */
    uint8_t          inited;
    uint8_t          device_id;
    mpu6050_config_t cfg;

    /* 运行统计（供测试台账读取） */
    uint32_t err_count;       /*!< 累计通信错误数   */
    uint32_t read_count;      /*!< 累计成功读取次数 */
} bsp_mpu6050_driver_t;

/* ------------------------------------------------------------------ */
/* 对外 API                                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief  实例化并初始化驱动（桥接核心：挂 ops 表 + 注入依赖 + 走 init）
 * @note   cfg 传 NULL 时使用默认配置(±2g/±250°/s/DLPF 44Hz/100Hz 采样)。
 */
mpu6050_status_t mpu6050_instantiate(bsp_mpu6050_driver_t       * const self,
                                     iic_driver_interface_t     * const p_iic,
                                     osal_time_base_t           * const p_timebase,
                                     osal_delay_t               * const p_delay,
                                     const mpu6050_config_t     * const cfg);

/** 换算辅助：raw -> 物理量（可独立用于上位机比对） */
void mpu6050_raw_to_phy(const mpu6050_raw_data_t * const raw,
                        mpu6050_data_t           * const out);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_MPU6050_DRIVER_H */
