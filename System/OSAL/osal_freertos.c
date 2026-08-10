#include "osal_freertos.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

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
    osal_freertos_queue_create,
    osal_freertos_queue_put,
    osal_freertos_queue_get,
    osal_freertos_queue_delete
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
