#ifndef __BSP_KEY_H__
#define __BSP_KEY_H__

#include "stm32f4xx.h"

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"


#define SHORT_PRESS_TIME   (500)    // Specify the short press time

extern TaskHandle_t key_TaskHandle;

typedef enum {
    KEY_OK             = 0,         /* Operation completed successfully     */
    KEY_ERROR          = 1,         /* Run-time error without case matched  */
    KEY_ERRORTIMEOUT   = 2,         /* Operation failed with timeout        */
    KEY_ERRORRESOURCE  = 3,         /* Resource not available               */
    KEY_ERRORPARAMETER = 4,         /* Parameter error                      */
    KEY_ERRORNOMEMORY  = 5,         /* Out of memory                        */
    KEY_ERRORISR       = 6,         /* Not allowed in ISR context           */
    KEY_RESERVED       = 0x7FFFFFFF /* Reserved                             */
} key_status_t;

typedef enum {
    KEY_PRESSED        = 0,         /* Operation completed successfully     */
    KEY_NOT_PRESSED    = 1,         /* Run-time error without case matched  */
    KEY_SHORT_PRESSED  = 2,         /* short press  */
    KEY_LONG_PRESSED   = 3,         /* long press  */
} key_press_status_t;

typedef enum {
  RASING = 0,     /* rising edge */
  FALLING = 1,    /* falling edge */
} key_trigger_edge_t;

typedef struct {
  key_trigger_edge_t  edge_type;      /* edge type */
  uint32_t            trigger_tick;   /* edge trigger time */
} key_press_event_t;

/**
  * @brief  key thread function
  * @param  pvParameters: task parameters (unused)
  * @retval void
  */
void key_task(void *pvParameters);

/**
  * @brief  scan the status of the key
  * @param key_value : key status
  * @retval key_status_t
  */
key_status_t key_scan(key_press_status_t * key_value);

/**
  * @brief  scan whether the key status is long press or short press 
  * @param  uint32_t * key_value
  * @retval key_status_t:
  *             KEY_SHORT_PRESSED: short press
  *             KEY_LONG_PRESSED: long press
  */
key_status_t key_polling_scan_short_long_press(key_press_status_t * key_value,
                                               uint32_t short_press_time);


/**
 * @brief Get the key status
 *
 * @param none
 *
 * @return 1 : key is pressed
 * @return 0 : key is not pressed
 */
uint8_t key_scan_bootloader(void);

#endif  // end of __BSP_KEY_H__
