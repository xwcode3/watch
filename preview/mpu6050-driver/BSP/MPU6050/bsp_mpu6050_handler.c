/**
 * @file    bsp_mpu6050_handler.c
 * @brief   MPU6050 Handler 层实现（队列 + lifetime 节流 + 回调 + 互斥 + 重试）
 *
 * 线程模型与 AHT21 的 temp_humi_handler_thread 完全同构：
 *
 *   生产者 send_imu_event (prio 1)        消费者 imu_handler_thread (prio 3)
 *     每 100ms → bsp_imu_read()             for(;;) queue_get(阻塞)
 *              → queue_put(event)                 → mutex_lock
 *                                                → lifetime 节流判断
 *                                                → ops.pf_read_data (带重试)
 *                                                → mutex_unlock
 *                                                → event.pf_callback(data)
 *
 * 规避了 AHT21 已知的几处债务（对应剖析文档 §7 编号）：
 *   - #6 忙等：等待全部走 OSAL delay（vTaskDelay 包装）；
 *   - #7 临界区：以互斥锁保护整条 I2C 事务（比临界区更精确，
 *     且不会因长时间关中断影响系统实时性）；
 *   - #10 回调：默认回调非空——示例任务里直接把数据送 elog/printf；
 *   - #4 全局单例：驱动状态 per-instance，统计计数也在实例内。
 */
#include "bsp_mpu6050_handler.h"

/* 日志宏：工程有 elog 时定义 MPU6050_USE_ELOG，否则静默 */
#ifdef MPU6050_USE_ELOG
#include "elog.h"
#define MPU6050_LOGI(...)   log_i(__VA_ARGS__)
#define MPU6050_LOGW(...)   log_w(__VA_ARGS__)
#define MPU6050_LOGE(...)   log_e(__VA_ARGS__)
#else
#define MPU6050_LOGI(...)   ((void)0)
#define MPU6050_LOGW(...)   ((void)0)
#define MPU6050_LOGE(...)   ((void)0)
#endif

#define IMU_QUEUE_DEPTH       10u
#define IMU_MUTEX_TIMEOUT_MS  50u     /*!< I2C 事务互斥等待上限 */
#define IMU_READ_RETRY_DEF    2u      /*!< 读失败重试次数默认值 */

bsp_imu_handler_t *gp_imu_instance = NULL;

/* ------------------------------------------------------------------ */
/* 内部工具                                                             */
/* ------------------------------------------------------------------ */

/* ops 适配器：typed self -> void* 上下文（桥接的可读替代直接强转） */
static mpu6050_status_t ops_init_adapter(void *dev, const mpu6050_config_t *cfg)
{
    return ((bsp_mpu6050_driver_t *)dev)->pf_init((bsp_mpu6050_driver_t *)dev, cfg);
}
static mpu6050_status_t ops_deinit_adapter(void *dev)
{
    return ((bsp_mpu6050_driver_t *)dev)->pf_deinit((bsp_mpu6050_driver_t *)dev);
}
static mpu6050_status_t ops_read_data_adapter(void *dev, mpu6050_data_t *out)
{
    return ((bsp_mpu6050_driver_t *)dev)->pf_read_data((bsp_mpu6050_driver_t *)dev, out);
}
static mpu6050_status_t ops_sleep_adapter(void *dev)
{
    return ((bsp_mpu6050_driver_t *)dev)->pf_sleep((bsp_mpu6050_driver_t *)dev);
}
static mpu6050_status_t ops_wakeup_adapter(void *dev)
{
    return ((bsp_mpu6050_driver_t *)dev)->pf_wakeup((bsp_mpu6050_driver_t *)dev);
}
static mpu6050_status_t ops_reset_adapter(void *dev)
{
    return ((bsp_mpu6050_driver_t *)dev)->pf_reset((bsp_mpu6050_driver_t *)dev);
}

static void __mount_handler(bsp_imu_handler_t *inst)
{
    gp_imu_instance = inst;
}

static int32_t __mount_callback(imu_event_t *event,
                                void (*cb)(const mpu6050_data_t *))
{
    if (event == NULL) {
        return 0;
    }
    event->pf_callback = cb;
    return (cb != NULL) ? 1 : 2;   /* 2 = 未挂载（仅告警），对齐 AHT21 风格 */
}

/* ------------------------------------------------------------------ */
/* 初始化                                                               */
/* ------------------------------------------------------------------ */

static int32_t bsp_mpu6050_handler_init(bsp_imu_handler_t *h,
                                        bsp_mpu6050_input_arg_t *arg)
{
    static bsp_mpu6050_driver_t driver_obj;      /* 静态实例：生命周期=线程全程 */

    /* 依赖注入检查（对齐 bsp_temp_humi_handler_inst 的 NULL 校验风格） */
    if (arg->iic_driver_interface == NULL ||
        arg->timebase_interface   == NULL ||
        arg->os_queue             == NULL ||
        arg->os_delay             == NULL) {
        return 0;
    }

    /* 队列 */
    if (arg->os_queue->queue_create(IMU_QUEUE_DEPTH, sizeof(imu_event_t),
                                    &h->queue_handle) != OSAL_STATUS_OK) {
        return 0;
    }
    h->queue_interface     = arg->os_queue;
    h->timebase_interface  = arg->timebase_interface;
    h->delay_interface     = arg->os_delay;

    /* 互斥锁：外部未提供则内部自建 */
    h->mutex_interface = arg->os_mutex;
    if (h->mutex_interface != NULL) {
        if (h->mutex_interface->mutex_create(&h->mutex_handle) != OSAL_STATUS_OK) {
            return 0;
        }
    } else {
        h->mutex_handle = NULL;   /* 无互斥接口时降级为不加锁（单任务访问场景） */
    }

    h->retry_max = (arg->retry_max != 0u) ? arg->retry_max : IMU_READ_RETRY_DEF;
    h->default_callback = arg->pf_callback;

    /* 实例化驱动（桥接：ops 表 + dev 上下文） */
    if (mpu6050_instantiate(&driver_obj,
                            arg->iic_driver_interface,
                            arg->timebase_interface,
                            arg->os_delay,
                            arg->cfg) != MPU6050_STATUS_OK) {
        MPU6050_LOGE("mpu6050 instantiate failed");
        return 0;
    }
    h->dev = (void *)&driver_obj;

    /* 设备无关 ops 表：具名适配函数替代直接强转（无 -Wcast-function-type 告警） */
    {
        h->ops.pf_init       = ops_init_adapter;
        h->ops.pf_deinit     = ops_deinit_adapter;
        h->ops.pf_read_data  = ops_read_data_adapter;
        h->ops.pf_sleep      = ops_sleep_adapter;
        h->ops.pf_wakeup     = ops_wakeup_adapter;
        h->ops.pf_reset      = ops_reset_adapter;
    }

    h->last_tick  = 0u;
    h->is_initated = IMU_HANDLER_INITIATED;
    return 1;
}

int32_t bsp_mpu6050_handler_inst(bsp_imu_handler_t *h, bsp_mpu6050_input_arg_t *arg)
{
    if (h == NULL || arg == NULL) {
        return 0;
    }
    if (bsp_mpu6050_handler_init(h, arg) != 1) {
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* 消费者线程                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief 一次带互斥、重试、恢复的读取
 * @note  返回 mpu6050_status_t；失败时按 retry_max 重试，
 *        重试仍失败则尝试总线复位 + 器件重新初始化。
 */
static mpu6050_status_t imu_do_read(bsp_imu_handler_t *h, mpu6050_data_t *out)
{
    uint32_t try;
    mpu6050_status_t st = MPU6050_STATUS_ERROR;

    for (try = 0; try <= h->retry_max; ++try) {
        /* 互斥进入：整条 I2C 事务加锁 */
        if (h->mutex_interface != NULL && h->mutex_handle != NULL) {
            if (h->mutex_interface->mutex_lock(h->mutex_handle, IMU_MUTEX_TIMEOUT_MS)
                    != OSAL_STATUS_OK) {
                MPU6050_LOGW("imu mutex timeout");
                st = MPU6050_STATUS_ERRORTIMEOUT;
                continue;
            }
        }

        st = h->ops.pf_read_data(h->dev, out);

        if (h->mutex_interface != NULL && h->mutex_handle != NULL) {
            (void)h->mutex_interface->mutex_unlock(h->mutex_handle);
        }

        if (st == MPU6050_STATUS_OK) {
            return st;
        }

        /* 失败：短暂退避后重试（RTOS 友好，非忙等） */
        if (try < h->retry_max) {
            (void)h->delay_interface->delay_ms(5u);
        }
    }

    /* 全部重试失败：总线复位 + 器件恢复 */
    MPU6050_LOGE("imu read failed st=%d, run recovery", (int)st);
    {
        iic_driver_interface_t *bus = ((bsp_mpu6050_driver_t *)h->dev)->p_iic_instance;
        if (bus != NULL && bus->pf_iic_bus_reset != NULL) {
            if (h->mutex_interface != NULL && h->mutex_handle != NULL) {
                (void)h->mutex_interface->mutex_lock(h->mutex_handle, IMU_MUTEX_TIMEOUT_MS);
            }
            (void)bus->pf_iic_bus_reset(bus->iic_bus);
            (void)h->ops.pf_reset(h->dev);
            (void)h->ops.pf_init(h->dev, NULL);
            if (h->mutex_interface != NULL && h->mutex_handle != NULL) {
                (void)h->mutex_interface->mutex_unlock(h->mutex_handle);
            }
        }
    }
    return st;
}

void imu_handler_thread(void *argument)
{
    bsp_mpu6050_input_arg_t *input_arg = (bsp_mpu6050_input_arg_t *)argument;
    bsp_imu_handler_t        instance;
    mpu6050_data_t           data;
    uint32_t                 now;

    if (input_arg == NULL) {
        MPU6050_LOGE("imu thread arg NULL");
        return;
    }

    if (bsp_mpu6050_handler_inst(&instance, input_arg) != 1) {
        MPU6050_LOGE("imu handler inst failed");
        return;   /* 初始化失败：线程退出（对齐 AHT21 行为） */
    }
    __mount_handler(&instance);
    (void)__mount_callback(&instance.event, input_arg->pf_callback);

    MPU6050_LOGI("imu handler ready");

    for (;;) {
        /* 阻塞等待采样事件（对齐 queue_get + MY_MAX_DELAY 风格） */
        if (instance.queue_interface->queue_get(instance.queue_handle,
                                                &instance.event,
                                                0xFFFFFFFFu) != OSAL_STATUS_OK) {
            continue;
        }

        now = instance.timebase_interface->get_tick_ms();

        /* lifetime 节流：>= 判断（对齐 AHT21 修正后的语义）；
           lifetime=0 表示每次事件都读 */
        if (instance.event.lifetime_ms == 0u ||
            (now - instance.last_tick) >= instance.event.lifetime_ms) {

            if (imu_do_read(&instance, &data) == MPU6050_STATUS_OK) {
                instance.last_tick = now;   /* 成功才刷新时间戳，失败立即重试 */

                if (instance.event.pf_callback != NULL) {
                    instance.event.pf_callback(&data);
                } else if (instance.default_callback != NULL) {
                    instance.default_callback(&data);
                } else {
                    MPU6050_LOGI("imu: ax=%.3f ay=%.3f az=%.3f g | "
                                 "gx=%.1f gy=%.1f gz=%.1f dps | t=%.2fC",
                                 data.accel_x_g, data.accel_y_g, data.accel_z_g,
                                 data.gyro_x_dps, data.gyro_y_dps, data.gyro_z_dps,
                                 data.temp_c);
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* 生产者 API                                                           */
/* ------------------------------------------------------------------ */

int32_t bsp_imu_read(const imu_event_t *ev)
{
    imu_event_t ev_default;

    if (gp_imu_instance == NULL || !gp_imu_instance->is_initated) {
        return 0;
    }
    if (ev == NULL) {
        ev_default.type        = IMU_EVENT_ALL;
        ev_default.lifetime_ms = 0u;
        ev_default.pf_callback = NULL;
        ev = &ev_default;
    }

    return (gp_imu_instance->queue_interface->queue_put(gp_imu_instance->queue_handle,
                                                        ev, 0u) == OSAL_STATUS_OK)
           ? 1 : 0;
}

void bsp_imu_get_stats(uint32_t *err_count, uint32_t *read_count)
{
    if (gp_imu_instance == NULL || gp_imu_instance->dev == NULL) {
        if (err_count)   { *err_count = 0u; }
        if (read_count)  { *read_count = 0u; }
        return;
    }
    {
        bsp_mpu6050_driver_t *d = (bsp_mpu6050_driver_t *)gp_imu_instance->dev;
        if (err_count)   { *err_count = d->err_count; }
        if (read_count)  { *read_count = d->read_count; }
    }
}
