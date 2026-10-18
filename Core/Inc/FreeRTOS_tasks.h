//
// Created by 文武 on 2026/4/22.
//

#ifndef WATCH_FREERTOS_TASKS_H
#define WATCH_FREERTOS_TASKS_H

#include "stm32f4xx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "bsp_aht21_driver.h"
#include "bsp_aht21_handler.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_led_driver.h"
#include "bsp_led_handler.h"
#include "bsp_adc.h"
#include "bsp_usart_driver.h"
#include "usart_parse.h"

#include "bsp_mpu6050_driver.h"
#include "bsp_mpu6050_handler.h"
#include "bsp_circular_buffer.h"

#include "ota.h"
#include "iic.h"
#include "osal.h"
#include "osal_freertos.h"

void app_task_create(void);

#endif //WATCH_FREERTOS_TASKS_H