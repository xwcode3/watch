#pragma once
#ifndef BSP_AHT21_HANDLER_H
#define BSP_AHT21_HANDLER_H

#include <stdint.h>
#include "bsp_aht21_driver.h"
#include "iic.h"
#include "FreeRTOS.h"
#include "queue.h"


typedef enum {
    TEMP_HUMI_OK                = 0,            /* Successful */
    TEMP_HUMI_ERROR             = 1,
    TEMP_HUMI_ERRORTIMEOUT      = 2,
    TEMP_HUMI_ERRORRESOURCE     = 3,
    TEMP_HUMI_ERRORPARAMETER     = 4,
    TEMP_HUMI_ERRORNOMEMORY     = 5,
    TEMP_HUMI_ERRORISR          = 6,
    TEMP_HUMI_RESERVED          = 0x7FFFFFFF    /* Reserved */
} temp_humi_status_t;

typedef enum {
    TEMP_HUMI_EVENT_TEMP = 0,
    TEMP_HUMI_EVENT_HUMI,
    TEMP_HUMI_EVENT_BOTH
} temp_humi_event_data_type_t;

/* Class of Event */
typedef struct {
    float *                 temperature;
    float *                 humidity;
    uint32_t                lifetime;           /* Lifetime of data, ms */
    uint32_t                timestamp;          /* Timestamp of data, ms */
    temp_humi_event_data_type_t type;       /* The data mode */
    void (*pf_callback)(float *, float *);      /* Callback function */
} temp_humi_event_t;

/* Input interface when instantiated */
typedef struct {
    /* IIC driver instance */
    iic_driver_interface_t              * iic_driver_interface; 

    /* Timebase interface */
    osal_time_base_t                *timebase_interface;
    osal_queue_t                    *os_queue_interface;
    osal_delay_t                    *os_delay_interface;

    void (*pf_callback)(float *, float *);      /* Callback function */
} temp_humi_handler_all_input_arg_t;


typedef struct temp_humi_handler_private_data temp_humi_handler_private_data_t;

/* Handleer class bsp handler */
typedef struct bsp_temp_humi_handler {
    /* Interface passed to driver layer */
    osal_time_base_t                *timebase_interface;
    iic_driver_interface_t              *iic_driver_interface;

    /* Interface from OS layer */
    osal_queue_t                   *os_queue_interface;
    osal_delay_t                   *os_delay_interface;

    /* Interface from driver instance */
    bsp_aht21_driver_t                  *p_aht21_instance;

    /* Handler of event queue */
    void                                *event_queue_handle;

    /* Private data */
    temp_humi_handler_private_data_t    *p_private_data;

    /* Timestamp of last temperature reading */
    uint32_t                            last_temperature_tick;

    /* Timestamp of last humidity reading */
    uint32_t                            last_humidity_tick;
} bsp_temp_humi_handler_t;

/**
 * @brief A thread function that handles the temperature and humidity events.
 * 
 * This function continuously checks for tempearture and humidity updates
 * and sends them to the event queue. It also updates the last temperature
 * and humidity.
 */
void temp_humi_handler_thread(void *argument);


/**
 * @brief Initializes the temperature and humidity handler instance.
 * 
 * This fuction takes in a handler instance and input arguments to 
 * initialize the temperature and humidity handler. It sets up the ncessary
 * interfaces, creates a queue for handling events, and initializes
 * the last temperature and humidity timestamps.
 * 
 * @param[in] handler_instance: A pointer to the temperature and humidity
 *                                handler instance. 
 * @param[in] input_arg: A pointer to the input arguments containing the
 *                          the required interfaces.
 * 
 * @return 8-bit unsigned integer representing the status of the initialization.
 *         - 0: Successful initialization.
 *         - Non-zero: Inittalization failed.
 */
temp_humi_status_t bsp_temp_humi_handler_inst(
    bsp_temp_humi_handler_t *handler_instance,
    temp_humi_handler_all_input_arg_t *input_arg
);

/**
 * @brief Reads the temperature and humidity from the AHT21 sensor.
 * 
 * This function sends an event to the AHT21 to read the temperature and 
 * humidity. When the temperature and humidity data is ready the callback function
 * or provided pointers for temp. & humi. will be updated.
 * 
 * @param[in] event: A pointer to the event made by the caller
 * 
 * @return An 8-bit signed integer representing the status of the event passed.
 *         - 0: Successful queued in the bsp handler.
 *         - Non-zero: Failed to pass.
 */
temp_humi_status_t bsp_temp_humi_read(temp_humi_event_t *event);

#endif /* BSP_AHT21_HANDLER_H */