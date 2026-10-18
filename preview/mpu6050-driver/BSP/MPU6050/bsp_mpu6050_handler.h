/**
 * @file    bsp_mpu6050_handler.h
 * @brief   MPU6050 Handler 层（业务编排：队列 / 时基 / lifetime / 回调 / 互斥）
 *
 * 分层组织与 AHT21 的 BSP/AHT21/bsp_aht21_handler.c 完全一致：
 *   - 设备无关的"业务编排"：队列、时基、lifetime 缓存、回调；
 *   - 差异点（吸收 AHT21 剖析文档 §6.5 的改进建议）：
 *     Handler 持有的是设备无关的 imu_ops_t ops 表 + 不透明 dev 上下文，
 *     而非具体 bsp_mpu6050_driver_t*，实现"换芯不改 Handler"。
 *     mpu6050 驱动在 instantiate 时已把 pf_* 挂好，这里只需透传。
 *
 * 互斥锁：硬件 I2C 外设为共享资源。默认 MPU6050 独占 I2C1（AHT21 走
 * 软件 I2C PB13/PB14），锁仍默认启用，防止未来多任务访问；
 * 若未来 AHT21 迁移到同一硬件 I2C，两 Handler 共用同一把锁即可。
 */
#ifndef __BSP_MPU6050_HANDLER_H
#define __BSP_MPU6050_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "osal_interface.h"
#include "bsp_mpu6050_driver.h"

/* ------------------------------------------------------------------ */
/* 事件与配置                                                           */
/* ------------------------------------------------------------------ */

typedef enum {
    IMU_EVENT_ALL = 0,          /*!< 读全部六轴+温度（当前唯一模式） */
    IMU_EVENT_RESERVED = 0x7FFFFFFF
} imu_event_type_t;

/** 采样事件（生产者投递） */
typedef struct {
    imu_event_type_t type;
    uint32_t         lifetime_ms;   /*!< 节流窗口，0 = 每次都读 */
    void (*pf_callback)(const mpu6050_data_t *data);  /*!< 本次事件的消费者 */
} imu_event_t;

/** Handler 输入参数（集成层填充，对齐 temp_humi_input_arg 风格） */
typedef struct {
    iic_driver_interface_t *iic_driver_interface;
    osal_time_base_t       *timebase_interface;
    osal_delay_t           *os_delay;
    osal_queue_t           *os_queue;
    osal_mutex_t           *os_mutex;              /*!< I2C 事务互斥，NULL=内部自建 */
    const mpu6050_config_t *cfg;                   /*!< NULL=驱动默认配置 */
    void (*pf_callback)(const mpu6050_data_t *data); /*!< 默认回调，可空 */
    uint32_t                retry_max;             /*!< 读失败重试次数，默认 2 */
} bsp_mpu6050_input_arg_t;

/* ------------------------------------------------------------------ */
/* 设备无关 ops 契约（吸收 AHT21 §6.5 建议：换芯不改 Handler）           */
/* ------------------------------------------------------------------ */
typedef struct imu_ops {
    mpu6050_status_t (*pf_init)(void *dev, const mpu6050_config_t *cfg);
    mpu6050_status_t (*pf_deinit)(void *dev);
    mpu6050_status_t (*pf_read_data)(void *dev, mpu6050_data_t *out);
    mpu6050_status_t (*pf_sleep)(void *dev);
    mpu6050_status_t (*pf_wakeup)(void *dev);
    mpu6050_status_t (*pf_reset)(void *dev);
} imu_ops_t;

/* ------------------------------------------------------------------ */
/* Handler 实例                                                         */
/* ------------------------------------------------------------------ */
typedef enum {
    IMU_HANDLER_NOT_INIT = 0,
    IMU_HANDLER_INITIATED = 1
} imu_handler_state_t;

typedef struct bsp_imu_handler {
    imu_ops_t               ops;            /*!< 设备操作表（不透明） */
    void                   *dev;            /*!< 设备上下文(bsp_mpu6050_driver_t*) */

    osal_queue_t           *queue_interface;
    void                   *queue_handle;
    osal_time_base_t       *timebase_interface;
    osal_delay_t           *delay_interface;
    osal_mutex_t           *mutex_interface;
    void                   *mutex_handle;

    imu_event_t             event;
    void                  (*default_callback)(const mpu6050_data_t *data);
    uint32_t                last_tick;
    uint32_t                retry_max;
    uint8_t                 is_initated;
} bsp_imu_handler_t;

/* 全局访问点（对齐 gp_temp_humi_instance 约定） */
extern bsp_imu_handler_t *gp_imu_instance;

/* ------------------------------------------------------------------ */
/* 对外 API                                                             */
/* ------------------------------------------------------------------ */

/** 线程主体（消费者）：vTaskCreate 的入口，argument = bsp_mpu6050_input_arg_t* */
void imu_handler_thread(void *argument);

/** 实例化 Handler（建队列 + 实例化驱动 + 挂全局指针） */
int32_t bsp_mpu6050_handler_inst(bsp_imu_handler_t *h, bsp_mpu6050_input_arg_t *arg);

/** 对外投递采样事件（生产者用；ev=NULL 使用默认参数；返回 0=失败） */
int32_t bsp_imu_read(const imu_event_t *ev);

/** 读取累计错误统计（供测试台账） */
void bsp_imu_get_stats(uint32_t *err_count, uint32_t *read_count);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_MPU6050_HANDLER_H */
