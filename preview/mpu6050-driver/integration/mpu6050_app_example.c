/**
 * @file    mpu6050_app_example.c
 * @brief   集成层示例：把 MPU6050 驱动接进 FreeRTOS（App 层接线演示）
 *
 * 对应 AHT21 工程的 Core/Src/FreeRTOS_tasks.c 角色：
 *   - 定义具体总线（I2C1, PB6/PB7, 400kHz 硬件外设）
 *   - 填 iic_driver_interface_t（硬件引擎 hw_iic_*）
 *   - 定义 OSAL 实例（FreeRTOS 包装）
 *   - 创建生产者 + 消费者两个任务
 *
 * 用法：把本文件并入工程，在 main() 里调用 mpu6050_app_task_create()。
 * 工程若有现成 osal_freertos.c，用现成的，删掉本文件里的 OSAL 包装段。
 *
 * 硬件接线（GY-521 模块）：
 *   VCC -> 3V3    GND -> GND    SCL -> PB6    SDA -> PB7    AD0 -> GND
 *   INT -> 悬空（本驱动轮询，不用中断）
 */
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "iic_interface.h"
#include "hw_iic.h"
#include "osal_interface.h"
#include "bsp_mpu6050_driver.h"
#include "bsp_mpu6050_handler.h"

/* ================================================================== */
/* 1. 总线与引擎接线                                                    */
/* ================================================================== */

/** MPU6050 总线描述：硬件 I2C1，PB6=SCL / PB7=SDA，快速模式 400kHz */
static iic_bus_t iic_bus_mpu6050 = {
    .scl_port     = GPIOB,
    .scl_pin      = GPIO_Pin_6,
    .sda_port     = GPIOB,
    .sda_pin      = GPIO_Pin_7,
    .i2c_periph   = I2C1,
    .i2c_clk_speed= 400000u,
    .hw_enabled   = 1u,
};

/** I2C 引擎接口实例：挂硬件 I2C 实现（桥接的"实现"注册） */
static iic_driver_interface_t iic_driver_interface_mpu = { 0 };

/* ================================================================== */
/* 2. OSAL FreeRTOS 包装（工程已有 osal_freertos.c 时删除本段）          */
/* ================================================================== */

static uint32_t osal_freertos_get_tick_ms_impl(void)
{
    return (uint32_t)xTaskGetTickCount();
}

static osal_status_t osal_freertos_delay_ms_impl(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
    return OSAL_STATUS_OK;
}

static osal_status_t osal_freertos_queue_create_impl(uint32_t depth, uint32_t item_size,
                                                     void **handle)
{
    *handle = (void *)xQueueCreate((UBaseType_t)depth, (UBaseType_t)item_size);
    return (*handle != NULL) ? OSAL_STATUS_OK : OSAL_STATUS_ERROR;
}

static osal_status_t osal_freertos_queue_put_impl(void *handle, const void *item,
                                                  uint32_t timeout_ms)
{
    return (xQueueSend((QueueHandle_t)handle, item,
                       pdMS_TO_TICKS(timeout_ms)) == pdTRUE)
           ? OSAL_STATUS_OK : OSAL_STATUS_TIMEOUT;
}

static osal_status_t osal_freertos_queue_get_impl(void *handle, void *item,
                                                  uint32_t timeout_ms)
{
    TickType_t t = (timeout_ms == 0xFFFFFFFFu) ? portMAX_DELAY
                                               : pdMS_TO_TICKS(timeout_ms);
    return (xQueueReceive((QueueHandle_t)handle, item, t) == pdTRUE)
           ? OSAL_STATUS_OK : OSAL_STATUS_TIMEOUT;
}

static osal_status_t osal_freertos_mutex_create_impl(void **handle)
{
    *handle = (void *)xSemaphoreCreateMutex();
    return (*handle != NULL) ? OSAL_STATUS_OK : OSAL_STATUS_ERROR;
}

static osal_status_t osal_freertos_mutex_lock_impl(void *handle, uint32_t timeout_ms)
{
    return (xSemaphoreTake((SemaphoreHandle_t)handle,
                           pdMS_TO_TICKS(timeout_ms)) == pdTRUE)
           ? OSAL_STATUS_OK : OSAL_STATUS_TIMEOUT;
}

static osal_status_t osal_freertos_mutex_unlock_impl(void *handle)
{
    return (xSemaphoreGive((SemaphoreHandle_t)handle) == pdTRUE)
           ? OSAL_STATUS_OK : OSAL_STATUS_ERROR;
}

static osal_time_base_t osal_timebase_freertos = {
    .get_tick_ms = osal_freertos_get_tick_ms_impl,
};
static osal_delay_t osal_delay_freertos = {
    .delay_ms = osal_freertos_delay_ms_impl,
};
static osal_queue_t osal_queue_freertos = {
    .queue_create = osal_freertos_queue_create_impl,
    .queue_put    = osal_freertos_queue_put_impl,
    .queue_get    = osal_freertos_queue_get_impl,
};
static osal_mutex_t osal_mutex_freertos = {
    .mutex_create = osal_freertos_mutex_create_impl,
    .mutex_lock   = osal_freertos_mutex_lock_impl,
    .mutex_unlock = osal_freertos_mutex_unlock_impl,
};

/* ================================================================== */
/* 3. 数据消费者回调（把数据真正用起来——规避 AHT21 的回调空置问题）       */
/* ================================================================== */

static void imu_data_consumer(const mpu6050_data_t *data)
{
    /* 这里接 LCD / 控制逻辑 / 上位机协议；示例先打印 */
    printf("[IMU] ax=%.3f ay=%.3f az=%.3f g | gx=%.1f gy=%.1f gz=%.1f dps | T=%.2f C\r\n",
           data->accel_x_g, data->accel_y_g, data->accel_z_g,
           data->gyro_x_dps, data->gyro_y_dps, data->gyro_z_dps,
           data->temp_c);
}

/* ================================================================== */
/* 4. 任务定义                                                          */
/* ================================================================== */

/** Handler 输入参数（对齐 temp_humi_input_arg 的组织方式） */
static bsp_mpu6050_input_arg_t imu_input_arg = { 0 };

/**
 * 生产者任务：每 100ms 投递一次采样事件
 * （对齐 send_temp_humi_event：先等队列建好，再周期投递）
 */
static void send_imu_event(void *argument)
{
    static const imu_event_t ev = {
        .type        = IMU_EVENT_ALL,
        .lifetime_ms = 100u,      /* 节流窗口 = 采样周期；消费者按 >= 判断 */
        .pf_callback = NULL,      /* NULL = 走集成层默认回调 */
    };
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(200));       /* 等消费者初始化完成 */

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));   /* 采样周期 100ms，可调 */
        (void)bsp_imu_read(&ev);
    }
}

/** 创建 MPU6050 两个任务（在 main() 的任务创建段调用） */
void mpu6050_app_task_create(void)
{
    /* 1) 挂硬件 I2C 引擎 */
    hw_iic_mount_interface(&iic_driver_interface_mpu, &iic_bus_mpu6050);

    /* 2) 填 Handler 输入 */
    imu_input_arg.iic_driver_interface = &iic_driver_interface_mpu;
    imu_input_arg.timebase_interface   = &osal_timebase_freertos;
    imu_input_arg.os_delay             = &osal_delay_freertos;
    imu_input_arg.os_queue             = &osal_queue_freertos;
    imu_input_arg.os_mutex             = &osal_mutex_freertos;  /* NULL=不加锁 */
    imu_input_arg.cfg                  = NULL;   /* 默认 ±2g/±250°/s/44Hz/100Hz */
    imu_input_arg.pf_callback          = imu_data_consumer;
    imu_input_arg.retry_max            = 2u;

    /* 3) 创建消费者(优先级 3) + 生产者(优先级 1)，栈 512 字 */
    (void)xTaskCreate(imu_handler_thread,  "imu_handler",  512,
                      &imu_input_arg, 3, NULL);
    (void)xTaskCreate(send_imu_event,      "imu_sender",   256,
                      NULL, 1, NULL);
}
