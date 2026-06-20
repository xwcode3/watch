#ifndef __BSP_LED_H__
#define __BSP_LED_H__

#include "stm32f4xx.h"
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

extern TaskHandle_t led_TaskHandle;    // led线程句柄

// Function return status
typedef enum {
  LED_OK                  = 0,                /* Operation completed successfully     */
  LED_ERROR               = 1,                /* Run-time error without case matched  */
  LED_ERRORTIMEOUT        = 2,                /* Operation failed with timeout        */
  LED_ERRORRESOURCE       = 3,                /* Resource not available               */
  LED_ERRORPARAMETER      = 4,                /* Parameter error                      */
  LED_ERRORNOMEMORY       = 5,                /* Out of memory                        */
  LED_ERRORISR            = 6,                /* Not allowed in ISR context           */
  LED_RESERVED            = 0x7FFFFFFF        /* Reserved                             */
} led_status_t;

typedef enum {
  LED_OFF             = 0,
  LED_ON              = 1,
  LED_TOGGLE          = 2,
  LED_BLINK_1_TIMES   = 3,
  LED_BLINK_3_TIMES   = 4,
  LED_BLINK_10_TIMES  = 5,
  LED_INITED_VALUE    = 0xFF,
} led_operation_t;


/**
  * @brief  led thread function
  * @param  pvParameters: task parameters (unused)
  * @retval void
  */
void led_task(void *pvParameters);

/**
 * @brief  控制LED的亮灭，或翻转状态
 * @param  void
 * @retval void
 */
led_status_t led_control(led_operation_t operation);

/**
 * @brief  led control by irq
 * @param  void
 * @retval void
 */
led_status_t led_off_on_irq(void);

// // 点亮LED，输出低电平
// static void led_on(void);

// // 熄灭LED，输出高电平
// static void led_off(void);

// 0: 灭; 1: 点亮
led_operation_t get_led_status(void);

// // 切换LED状态
// static void led_toggle(void);


#endif // __BSP_LED_H__
