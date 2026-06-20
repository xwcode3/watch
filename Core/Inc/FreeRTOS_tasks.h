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

#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_adc.h"
#include "bsp_usart_driver.h"
#include "usart_parse.h"

void app_task_create(void);

#endif //WATCH_FREERTOS_TASKS_H