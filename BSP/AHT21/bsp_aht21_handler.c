#include "bsp_aht21_handler.h"
#include "elog.h"

#define TEMP_HUMI_NOT_INITIATED     0
#define TEMP_HUMI_INITIATED         1

#define MY_MAX_DELAY 0xffffffffUL


// Flag to indicate if the handler has been initialize
static bsp_temp_humi_handler_t * gp_temp_humi_instance = NULL;

// Private data structure to manage handler state
typedef struct temp_humi_handler_private_data {
    uint8_t is_initated;   /* Initialization flag */
} temp_humi_handler_private_data_t;


/**
 * @brief Mount the handler instance globally for use in other functions.
 * 
 * @param[in] instance: Pointer to the handler instance to mount.
 */
void __mount_hanlder(bsp_temp_humi_handler_t *instance) {
    gp_temp_humi_instance = instance;
}

QueueHandle_t temp_humi_queue = NULL;


/**
 * @brief Initialize the temperature and humidity handler.
 * 初始化温湿度 handler，包括创建温湿度事件队列、
 * 
 * @param[in] handler_instance: Pointer to the handler instance.
 * 
 * @retval TEMP_HUMI_OK: Initialization successful.
 * @retval TEMP_HUMI_ERRORRESOURCE: Resource allocation or queue creation failed.
 */
static temp_humi_status_t bsp_temp_humi_handler_init(
    bsp_temp_humi_handler_t * handler_instance
) {

    log_i("bsp_temp_humi_handler_init start");
    int8_t ret = 0;

    // Create event queue
    if (NULL == handler_instance->os_queue_interface->queue_create) {
        log_e("os queue interface is NULL");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    ret = handler_instance->os_queue_interface->queue_create(
        10, sizeof(temp_humi_event_t),
        &(handler_instance->event_queue_handle)
    );
    log_i("OS_QUEUE_CREATE ret = %d", ret);
    if (ret) {
        log_e("error: create queue failed!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    temp_humi_queue = (QueueHandle_t)handler_instance->event_queue_handle;

    // Initialize AHT21 instance
    ret = aht21_instantiate(handler_instance->p_aht21_instance,
                      handler_instance->iic_driver_interface,
#ifdef OS_SUPPORTING
                      handler_instance->os_delay_interface,
#endif
                      handler_instance->timebase_interface
    );

#ifdef AHT21_HANDLER_DEBUG
    log_d("aht21_inst ret = %d", ret);
#endif  /* End of AHT21_HANDLER_DEBUG */

    if (ret) {
        log_e("error: aht21_inst failed!");
        return TEMP_HUMI_ERRORRESOURCE;
    }
    log_i("aht21_inst success");

    return TEMP_HUMI_OK;
}


/**
 * @brief De-initialize the temperature and humidity handler.
 * 
 * @return TEMP_HUMI_OK: De-initialization successful.
 */
static temp_humi_status_t bsp_temp_humi_handler_deinit(void) {
    gp_temp_humi_instance->p_private_data->is_initated = TEMP_HUMI_NOT_INITIATED;

    return TEMP_HUMI_OK;
}

/**
 * @brief Instantiate the temperature and humidity handler.
 * 
 * @param[in] handler_instance: Pointer to the handler instance.
 * @param[in] input_arg: Pointer to the input arguments containing required
 *                       interfaces.
 * 
 * @retval TEMP_HUMI_OK: Instantiation successful.
 * @retval TEMP_HUMI_ERRORRESOURCE: Null pointer or resource error.
 */
temp_humi_status_t bsp_temp_humi_handler_inst(
    bsp_temp_humi_handler_t * handler_instance,
    temp_humi_handler_all_input_arg_t * input_arg
) {
    temp_humi_status_t ret = TEMP_HUMI_OK;

    log_i("bsp_temp_humi_handler_inst start");

    if (NULL == handler_instance) {
        log_e("error: handler_instance is NULL!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    if (NULL == input_arg) {
        log_e("error: input_arg is NULL!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    if (NULL == input_arg->timebase_interface) {
        log_e("error: timebase_interface is NULL!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    if (NULL == input_arg->iic_driver_interface) {
        log_e("error: iic_driver_interface is NULL!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    if (NULL == input_arg->os_queue_interface) {
        log_e("error: os_queue_interface is NULL!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    if (NULL == input_arg->os_delay_interface) {
        log_e("error: os_delay_interface is NULL!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    // Assign input interfaces to handler instance
    // Better: Void * variable passed to low level driver
    handler_instance->timebase_interface = input_arg->timebase_interface;
    handler_instance->iic_driver_interface = input_arg->iic_driver_interface;
#ifdef OS_SUPPORTING
    handler_instance->os_queue_interface = input_arg->os_queue_interface;
    handler_instance->os_delay_interface = input_arg->os_delay_interface;
#endif /* End of OS_SUPPORTING */

    // Initialize the handler
    ret = bsp_temp_humi_handler_init(handler_instance);
    if (TEMP_HUMI_OK != ret) {
        log_e("error: bsp_temp_humi_handler_init failed!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    // handler_instance->p_private_data->is_initated = TEMP_HUMI_INITIATED;
    handler_instance->p_private_data->is_initated = TEMP_HUMI_INITIATED;

    log_i("is_initated = %d", handler_instance->p_private_data->is_initated);

    log_i("bsp_temp_humi_handler_inst end");
    return TEMP_HUMI_OK;
}


/**
 * @brief Fetch temperature and humidity values.
 * 
 * @param[in] handler_instance: Pointer to the handler instance.
 * @param[in] event: Event data specifying the type of data to fetch
 *                   and its lifetime.
 * @param[in] temp: Pointer to store the fetched temperature value.
 * @param[in] humi: Pointer to store the fetched humidity value.
 * 
 * @retval TEMP_HUMI_OK: Data fetch successfully.
 * @retval TEMP_HUMI_ERROR: Error in fetching data.
 */
temp_humi_status_t get_temp_humi(
    bsp_temp_humi_handler_t * handler_instance,
    temp_humi_event_t event,
    float * temp,
    float * humi
) {
    temp_humi_status_t ret = TEMP_HUMI_OK;
    uint32_t tim = 0;
    handler_instance->timebase_interface->get_tick_ms(&tim);

    switch (event.type) {
        case TEMP_HUMI_EVENT_TEMP:
            // Re-fetch data if the current time exceeds the data's lifetime
            if ((tim - handler_instance->last_temperature_tick) > event.lifetime) {
                handler_instance->p_aht21_instance->pf_read_temperature(
                    handler_instance->p_aht21_instance, temp
                );

                // Update last temperature tick
                handler_instance->last_temperature_tick = tim;
            }

            break;

        case TEMP_HUMI_EVENT_HUMI:
            if ((tim - handler_instance->last_humidity_tick) > event.lifetime) {
                handler_instance->p_aht21_instance->pf_read_humidity(
                    handler_instance->p_aht21_instance, humi
                );

                // Update last humidity tick
                handler_instance->last_humidity_tick = tim;
            }

            break;

        case TEMP_HUMI_EVENT_BOTH:
            /* NOTE: 用 >= 而不是 >。发送间隔(1000ms)与 lifetime(1000ms)相等时，
               "1000 > 1000" 恒为假，导致每两次事件才读一次（读频率减半）。 */
            if (((tim - handler_instance->last_temperature_tick) >= event.lifetime)
                || ((tim - handler_instance->last_humidity_tick) >= event.lifetime)) {
                /* 检查读取返回值：失败时不更新 last tick（下次事件立即重试），
                   也不更新数据，避免把上一次的旧值当"新数据"上报。 */
                if (AHT21_OK == handler_instance->p_aht21_instance->pf_read_data(
                        handler_instance->p_aht21_instance, temp, humi)) {
                    // Update last temperature tick
                    handler_instance->last_temperature_tick = tim;
                    handler_instance->last_humidity_tick = tim;

                    log_i("new data, temp = %f, humi = %f", *temp, *humi);
                } else {
                    log_e("aht21 read failed, keep old data");
                }
            } else {
                log_i("old data, temp = %f, humi = %f", *temp, *humi);
            }

            break;
        
        default:
            // Error in configuration
            *temp = 0;
            *humi = 0;
            ret = TEMP_HUMI_ERROR;
            break;
    }

    return ret;
}

/**
 * @brief Mount a callback function for temperature and humidity events.
 * 
 * @param[in] event: Pointer to the event data.
 * @param[in] pf_callback: Pointer to the callback function.
 * 
 * @retval 2: Null pointer.
 * @retval 1: Callback function mounted successfully.
 * @retval 0: Null pointer or error in configuration.
 */
uint8_t __mount_callback(temp_humi_event_t *event, void (*pf_callback)(float *, float *)) {
    if (NULL == event) {
        log_e("error: event is NULL!");
        return 0;
    }

    if (NULL == pf_callback) {
        log_i("warning: pf_callback is NULL!");
        event->pf_callback = NULL;
        return 2;
    }
    
    event->pf_callback = pf_callback;
    return 1;
}



/**
 * @brief Handler thread for processing temperature and humidity events.
 * 
 * @param[in] argument: Pointer to input arguments (input_arg).
 */
void temp_humi_handler_thread(void *argument) {
    log_i("temp_humi_handler_thread start");
    
    float temp = 0;
    float humi = 0;
    temp_humi_handler_all_input_arg_t * input_arg = NULL;
    temp_humi_event_t event;
    temp_humi_status_t ret = TEMP_HUMI_OK;

    // AHT21 instance
    bsp_aht21_driver_t bsp_aht21_driver;
    // Handler instance structure
    bsp_temp_humi_handler_t temp_humi_handler_instance = {0};
    temp_humi_handler_private_data_t temp_humi_private_data = {0};

    if (NULL == argument) {
        log_e("temp_humi_handler_thread argument is NULL!");
        return;
    }

    input_arg = (temp_humi_handler_all_input_arg_t *)argument;

    temp_humi_handler_instance.p_aht21_instance = &bsp_aht21_driver;
    temp_humi_handler_instance.p_private_data = &temp_humi_private_data;


    ret = bsp_temp_humi_handler_inst(
        &temp_humi_handler_instance,
        input_arg
    );

    if (TEMP_HUMI_OK == ret) {
        __mount_hanlder(&temp_humi_handler_instance);
    }

    __mount_callback(&event, input_arg->pf_callback);

    for (;;) {
        // 1. Read event from the queue
        ret = temp_humi_handler_instance.os_queue_interface->queue_get(
            temp_humi_handler_instance.event_queue_handle,
            &event,
            MY_MAX_DELAY
        );
        log_i("temp_humi_queue get ret = %d", ret);

        // 2. Fetch temperature and humidity
        if (TEMP_HUMI_OK == get_temp_humi(&temp_humi_handler_instance, 
            event, &temp, &humi)) {

            // Data fetched successfully
        } else {
            // Data fetch error
        }

        // 3. Execute the callback function
        if (NULL != event.pf_callback) {
            event.pf_callback(&temp, &humi);
        }
    }
}


/**
 * @brief API to read temperature and humidity
 * 
 * @param[in] event: Event containing temperature and humidity request details.
 * 
 * @retval TEMP_HUMI_OK: Event successfully queued.
 * @retval TEMP_HUMI_ERRORRESOURCE: Resource allocation or initialization error.
 */
temp_humi_status_t bsp_temp_humi_read(temp_humi_event_t *event) {
    log_i("bsp_temp_humi_read");

    temp_humi_status_t ret = TEMP_HUMI_OK;

    // 1.Check if the handler instance exists (init may have failed)
    if (NULL == gp_temp_humi_instance) {
        log_e("error: temp_humi_handler instance is NULL!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    // 2.Check if the handler is initialized
    if (TEMP_HUMI_NOT_INITIATED == \
        gp_temp_humi_instance->p_private_data->is_initated) {
        
        log_e("error: temp_humi_handler is not initialized!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    // 2. Queue the event for processing
    ret = gp_temp_humi_instance->os_queue_interface->queue_put(
        gp_temp_humi_instance->event_queue_handle, event, MY_MAX_DELAY
    );
    log_i("put ret = %d", ret);
    if (ret) {
        log_e("error: temp_humi_handler queue put error!");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    return TEMP_HUMI_OK;
}