/**
 * @file    osal_interface.h
 * @brief   OSAL 抽象契约（队列 / 时基 / 延时 / 互斥锁）
 *
 * 与现有工程 System/OSAL/ 的接口约定保持一致（字段名与调用方式对齐
 * AHT21 剖析文档：timebase->get_tick_ms、queue->create/put/get、delay->delay_ms）。
 * 新增 osal_mutex_t：MPU6050 驱动用于硬件 I2C 事务级互斥。
 *
 * 集成说明：若工程已有等价 OSAL 契约头，删除本文件并让
 * bsp_mpu6050_*.c 的 include 指向既有头；仅需保证字段名一致
 * （不一致时的调整点见 docs/MPU6050_设计文档.md 移植指南 §8.3）。
 */
#ifndef __OSAL_INTERFACE_H
#define __OSAL_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    OSAL_STATUS_OK       = 0x00,
    OSAL_STATUS_ERROR    = 0x01,
    OSAL_STATUS_PARAM    = 0x02,
    OSAL_STATUS_TIMEOUT  = 0x03,
    OSAL_STATUS_RESERVED = 0x7FFFFFFF
} osal_status_t;

/** 时基契约 */
typedef struct osal_time_base {
    uint32_t (*get_tick_ms)(void);
} osal_time_base_t;

/** 延时契约（RTOS 友好，任务态阻塞延时） */
typedef struct osal_delay {
    osal_status_t (*delay_ms)(uint32_t ms);
} osal_delay_t;

/** 队列契约（handle 为不透明指针，由实现层解释） */
typedef struct osal_queue {
    osal_status_t (*queue_create)(uint32_t depth, uint32_t item_size, void **handle);
    osal_status_t (*queue_put)(void *handle, const void *item, uint32_t timeout_ms);
    osal_status_t (*queue_get)(void *handle, void *item, uint32_t timeout_ms);
} osal_queue_t;

/** 互斥锁契约（新增：硬件 I2C 事务级互斥） */
typedef struct osal_mutex {
    osal_status_t (*mutex_create)(void **handle);
    osal_status_t (*mutex_lock)(void *handle, uint32_t timeout_ms);
    osal_status_t (*mutex_unlock)(void *handle);
} osal_mutex_t;

/** 临界区契约（沿用现有约定） */
typedef struct osal_critical {
    void (*enter)(void);
    void (*exit)(void);
} osal_critical_t;

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_INTERFACE_H */
