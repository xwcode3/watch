#include "osal_freertos.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "timer.h"
static TickType_t osal_freertos_to_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == OSAL_WAIT_FOREVER)
    {
        return portMAX_DELAY;
    }

    return pdMS_TO_TICKS(timeout_ms);
}

static osal_status_t osal_freertos_delay_ms(uint32_t ms)
{
    TickType_t ticks;

    if (ms == 0U)
    {
        return OSAL_ERRORPARAMETER;
    }

    ticks = pdMS_TO_TICKS(ms);
    if (ticks == 0U)
    {
        ticks = 1U;
    }
    vTaskDelay(ticks);

    return OSAL_OK;
}

static osal_status_t osal_freertos_get_tick_ms(uint32_t *ms)
{
    if (ms == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    *ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());

    return OSAL_OK;
}

static void loop_delay_us(uint32_t us) {
    delay_us(us);
}

static void loop_delay_ms(uint32_t ms) {
    delay_ms(ms);
}

static osal_status_t osal_freertos_queue_create(uint32_t item_num,
                                                uint32_t item_size,
                                                void **handle)
{
    QueueHandle_t queue;

    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    queue = xQueueCreate((UBaseType_t)item_num, (UBaseType_t)item_size);
    if (queue == NULL)
    {
        return OSAL_ERRORNOMEMORY;
    }

    *handle = (void *)queue;

    return OSAL_OK;
}

static osal_status_t osal_freertos_queue_put(void *handle,
                                             const void *item,
                                             uint32_t timeout_ms)
{
    BaseType_t result;

    if ((handle == NULL) || (item == NULL))
    {
        return OSAL_ERRORPARAMETER;
    }

    result = xQueueSend((QueueHandle_t)handle,
                        item,
                        osal_freertos_to_ticks(timeout_ms));

    return (result == pdPASS) ? OSAL_OK : OSAL_ERRORTIMEOUT;
}

static osal_status_t osal_freertos_queue_get(void *handle,
                                             void *item,
                                             uint32_t timeout_ms)
{
    BaseType_t result;

    if ((handle == NULL) || (item == NULL))
    {
        return OSAL_ERRORPARAMETER;
    }

    result = xQueueReceive((QueueHandle_t)handle,
                           item,
                           osal_freertos_to_ticks(timeout_ms));

    return (result == pdPASS) ? OSAL_OK : OSAL_ERRORTIMEOUT;
}

static osal_status_t osal_freertos_queue_delete(void *handle)
{
    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    vQueueDelete((QueueHandle_t)handle);

    return OSAL_OK;
}

static osal_status_t osal_freertos_queue_put_from_isr(void *handle,
                                                      const void *item,
                                                      long * const pxHigherPriorityTaskWoken)
{
    BaseType_t higher_prio_task_woken = pdFALSE;

    if ((handle == NULL) || (item == NULL) || (pxHigherPriorityTaskWoken == NULL))
    {
        return OSAL_ERRORPARAMETER;
    }

    /* 中断中不允许阻塞: 队列满时直接失败返回, 不会覆盖已有数据 */
    if (xQueueSendFromISR((QueueHandle_t)handle,
                          item,
                          &higher_prio_task_woken) != pdPASS)
    {
        return OSAL_ERRORRESOURCE;
    }

    *pxHigherPriorityTaskWoken = (higher_prio_task_woken != pdFALSE) ? 1L : 0L;

    return OSAL_OK;
}

static osal_status_t osal_freertos_queue_get_from_isr(void *handle,
                                                      void *item,
                                                      long * const pxHigherPriorityTaskWoken)
{
    BaseType_t higher_prio_task_woken = pdFALSE;

    if ((handle == NULL) || (item == NULL) || (pxHigherPriorityTaskWoken == NULL))
    {
        return OSAL_ERRORPARAMETER;
    }

    /* 中断中不允许阻塞: 队列为空时直接失败返回 */
    if (xQueueReceiveFromISR((QueueHandle_t)handle,
                             item,
                             &higher_prio_task_woken) != pdPASS)
    {
        return OSAL_ERRORRESOURCE;
    }

    *pxHigherPriorityTaskWoken = (higher_prio_task_woken != pdFALSE) ? 1L : 0L;

    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/*                              任务通知                                      */
/*---------------------------------------------------------------------------*/

static eNotifyAction osal_freertos_to_notify_action(osal_notify_action_t action)
{
    switch (action)
    {
        case OSAL_NOTIFY_ACTION_SET_BITS:
            return eSetBits;

        case OSAL_NOTIFY_ACTION_INCREMENT:
            return eIncrement;

        case OSAL_NOTIFY_ACTION_SET_VALUE:
            return eSetValueWithOverwrite;

        case OSAL_NOTIFY_ACTION_SET_VALUE_IF_CLEAR:
            return eSetValueWithoutOverwrite;

        case OSAL_NOTIFY_ACTION_NONE:
        default:
            return eNoAction;
    }
}

static osal_status_t osal_freertos_notify_send(void *handle)
{
    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    (void)xTaskNotifyGive((TaskHandle_t)handle);

    return OSAL_OK;
}

static osal_status_t osal_freertos_notify_send_from_isr(void *handle,
                                                        uint32_t value,
                                                        osal_notify_action_t action,
                                                        long * const pxHigherPriorityTaskWoken)
{
    BaseType_t higher_prio_task_woken = pdFALSE;

    if ((handle == NULL) || (pxHigherPriorityTaskWoken == NULL))
    {
        return OSAL_ERRORPARAMETER;
    }

    if (xTaskNotifyFromISR((TaskHandle_t)handle,
                           value,
                           osal_freertos_to_notify_action(action),
                           &higher_prio_task_woken) != pdPASS)
    {
        return OSAL_ERRORRESOURCE;
    }

    *pxHigherPriorityTaskWoken = (higher_prio_task_woken != pdFALSE) ? 1L : 0L;

    return OSAL_OK;
}

static osal_status_t osal_freertos_notify_wait(uint32_t *value,
                                               uint32_t timeout_ms)
{
    uint32_t notified_value = 0U;

    if (value == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    /* 进入/退出都不清位: 通知值原样读出, 仅清除"未读"状态 */
    if (xTaskNotifyWait(0U, 0U, &notified_value,
                        osal_freertos_to_ticks(timeout_ms)) != pdPASS)
    {
        return OSAL_ERRORTIMEOUT;
    }

    *value = notified_value;

    return OSAL_OK;
}

static osal_status_t osal_freertos_notify_take(uint32_t *count,
                                               uint32_t timeout_ms)
{
    uint32_t taken;

    if (count == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    /* xClearCountOnExit 传 pdFALSE: 每次只取走 1 个计数, 剩余计数继续累积 */
    taken = ulTaskNotifyTake(pdFALSE, osal_freertos_to_ticks(timeout_ms));
    if (taken == 0U)
    {
        return OSAL_ERRORTIMEOUT;
    }

    *count = taken;

    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/*                              互斥量                                        */
/*---------------------------------------------------------------------------*/

static osal_status_t osal_freertos_mutex_create(void **handle)
{
    SemaphoreHandle_t mutex;

    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    mutex = xSemaphoreCreateMutex();
    if (mutex == NULL)
    {
        return OSAL_ERRORNOMEMORY;
    }

    *handle = (void *)mutex;

    return OSAL_OK;
}

static osal_status_t osal_freertos_mutex_delete(void *handle)
{
    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    vSemaphoreDelete((SemaphoreHandle_t)handle);

    return OSAL_OK;
}

static osal_status_t osal_freertos_mutex_lock(void *handle, uint32_t timeout_ms)
{
    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    return (xSemaphoreTake((SemaphoreHandle_t)handle,
                           osal_freertos_to_ticks(timeout_ms)) == pdTRUE)
           ? OSAL_OK
           : OSAL_ERRORTIMEOUT;
}

static osal_status_t osal_freertos_mutex_unlock(void *handle)
{
    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    /* 只有持有者能释放: 由非持有者调用会返回 pdFALSE */
    return (xSemaphoreGive((SemaphoreHandle_t)handle) == pdTRUE)
           ? OSAL_OK
           : OSAL_ERROR;
}

/*---------------------------------------------------------------------------*/
/*                              信号量                                        */
/*---------------------------------------------------------------------------*/

/*
 * 二值信号量与计数信号量在 FreeRTOS 中同为 SemaphoreHandle_t, take/give/delete
 * 的用法完全一致, 仅创建函数不同, 因此下面共用同一份实现。
 */
static osal_status_t osal_freertos_sem_create_binary(void **handle)
{
    SemaphoreHandle_t sem;

    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    /* 注意: 创建后处于"空"状态, 必须先 give 一次才可能 take 成功 */
    sem = xSemaphoreCreateBinary();
    if (sem == NULL)
    {
        return OSAL_ERRORNOMEMORY;
    }

    *handle = (void *)sem;

    return OSAL_OK;
}

static osal_status_t osal_freertos_sem_create_counting(void **handle,
                                                       uint32_t max_count,
                                                       uint32_t initial_count)
{
    SemaphoreHandle_t sem;

    if ((handle == NULL) || (max_count == 0U) || (initial_count > max_count))
    {
        return OSAL_ERRORPARAMETER;
    }

    sem = xSemaphoreCreateCounting((UBaseType_t)max_count,
                                   (UBaseType_t)initial_count);
    if (sem == NULL)
    {
        return OSAL_ERRORNOMEMORY;
    }

    *handle = (void *)sem;

    return OSAL_OK;
}

static osal_status_t osal_freertos_sem_delete(void *handle)
{
    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    vSemaphoreDelete((SemaphoreHandle_t)handle);

    return OSAL_OK;
}

static osal_status_t osal_freertos_sem_take(void *handle, uint32_t timeout_ms)
{
    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    return (xSemaphoreTake((SemaphoreHandle_t)handle,
                           osal_freertos_to_ticks(timeout_ms)) == pdTRUE)
           ? OSAL_OK
           : OSAL_ERRORTIMEOUT;
}

static osal_status_t osal_freertos_sem_give(void *handle)
{
    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    /* 计数信号量已达上限, 或二值信号量已为非空时, 返回 OSAL_ERRORRESOURCE */
    return (xSemaphoreGive((SemaphoreHandle_t)handle) == pdTRUE)
           ? OSAL_OK
           : OSAL_ERRORRESOURCE;
}

static osal_status_t osal_freertos_sem_take_from_isr(void *handle,
                                                     long * const pxHigherPriorityTaskWoken)
{
    BaseType_t higher_prio_task_woken = pdFALSE;

    if ((handle == NULL) || (pxHigherPriorityTaskWoken == NULL))
    {
        return OSAL_ERRORPARAMETER;
    }

    /*
     * xSemaphoreTakeFromISR 只能用于二值/计数信号量, 不可用于互斥量;
     * 中断中不允许阻塞, 信号量为空时直接失败返回。
     */
    if (xSemaphoreTakeFromISR((SemaphoreHandle_t)handle,
                              &higher_prio_task_woken) != pdTRUE)
    {
        return OSAL_ERRORRESOURCE;
    }

    *pxHigherPriorityTaskWoken = (higher_prio_task_woken != pdFALSE) ? 1L : 0L;

    return OSAL_OK;
}

static osal_status_t osal_freertos_sem_give_from_isr(void *handle,
                                                     long * const pxHigherPriorityTaskWoken)
{
    BaseType_t higher_prio_task_woken = pdFALSE;

    if ((handle == NULL) || (pxHigherPriorityTaskWoken == NULL))
    {
        return OSAL_ERRORPARAMETER;
    }

    if (xSemaphoreGiveFromISR((SemaphoreHandle_t)handle,
                              &higher_prio_task_woken) != pdTRUE)
    {
        return OSAL_ERRORRESOURCE;
    }

    *pxHigherPriorityTaskWoken = (higher_prio_task_woken != pdFALSE) ? 1L : 0L;

    return OSAL_OK;
}

static osal_status_t osal_freertos_critical_enter(void)
{
    taskENTER_CRITICAL();

    return OSAL_OK;
}

static osal_status_t osal_freertos_critical_exit(void)
{
    taskEXIT_CRITICAL();

    return OSAL_OK;
}

static osal_status_t osal_freertos_yield(void)
{
    taskYIELD();

    return OSAL_OK;
}

static osal_status_t osal_freertos_thread_create(osal_thread_entry_t entry,
                                                 const char *name,
                                                 uint32_t stack_depth,
                                                 void *param,
                                                 uint32_t priority,
                                                 void **handle)
{
    BaseType_t result;
    TaskHandle_t task = NULL;

    if ((entry == NULL) ||
        (handle == NULL) ||
        (priority >= configMAX_PRIORITIES))
    {
        return OSAL_ERRORPARAMETER;
    }

    result = xTaskCreate(entry,
                         name,
                         (configSTACK_DEPTH_TYPE)stack_depth,
                         param,
                         (UBaseType_t)priority,
                         &task);
    if (result != pdPASS)
    {
        return OSAL_ERRORNOMEMORY;
    }

    *handle = (void *)task;

    return OSAL_OK;
}

static osal_status_t osal_freertos_thread_delete(void *handle)
{
    if (handle == NULL)
    {
        return OSAL_ERRORPARAMETER;
    }

    vTaskDelete((TaskHandle_t)handle);

    return OSAL_OK;
}

const osal_delay_t osal_delay_freertos = {
    osal_freertos_delay_ms
};

const osal_time_base_t osal_time_base_freertos = {
    osal_freertos_get_tick_ms
};

const osal_queue_t osal_queue_freertos = {
    .queue_create = osal_freertos_queue_create,
    .queue_delete = osal_freertos_queue_delete,
    .queue_put = osal_freertos_queue_put,
    .queue_get = osal_freertos_queue_get,
    .queue_put_from_isr = osal_freertos_queue_put_from_isr,
    .queue_get_from_isr = osal_freertos_queue_get_from_isr
};

const osal_mutex_t osal_mutex_freertos = {
    .mutex_create = osal_freertos_mutex_create,
    .mutex_delete = osal_freertos_mutex_delete,
    .mutex_lock = osal_freertos_mutex_lock,
    .mutex_unlock = osal_freertos_mutex_unlock
};

const osal_sem_t osal_sem_freertos = {
    .semaphore_create = osal_freertos_sem_create_binary,
    .semaphore_delete = osal_freertos_sem_delete,
    .semaphore_take = osal_freertos_sem_take,
    .semaphore_give = osal_freertos_sem_give,
    .semaphore_take_from_isr = osal_freertos_sem_take_from_isr,
    .semaphore_give_from_isr = osal_freertos_sem_give_from_isr
};

/* 与二值信号量共用 take/give/delete: FreeRTOS 中两者是同一种对象 */
const osal_sem_counting_t osal_sem_counting_freertos = {
    .semaphore_counting_create = osal_freertos_sem_create_counting,
    .semaphore_counting_delete = osal_freertos_sem_delete,
    .semaphore_counting_take = osal_freertos_sem_take,
    .semaphore_counting_give = osal_freertos_sem_give,
    .semaphore_counting_take_from_isr = osal_freertos_sem_take_from_isr,
    .semaphore_counting_give_from_isr = osal_freertos_sem_give_from_isr
};

const osal_notify_t osal_notify_freertos = {
    .notify_send = osal_freertos_notify_send,
    .notify_send_from_isr = osal_freertos_notify_send_from_isr,
    .notify_wait = osal_freertos_notify_wait,
    .notify_take = osal_freertos_notify_take
};

const osal_critical_t osal_critical_freertos = {
    osal_freertos_critical_enter,
    osal_freertos_critical_exit
};

const osal_thread_t osal_thread_freertos = {
    osal_freertos_thread_create,
    osal_freertos_thread_delete
};

const osal_yield_t osal_yield_freertos = {
    osal_freertos_yield
};

const osal_loop_delay_t osal_loop_delay_freertos = {
    loop_delay_us,
    loop_delay_ms
};
