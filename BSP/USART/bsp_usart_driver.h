//
// Created by 文武 on 2026/6/14.
//

#ifndef WATCH_BSP_USART_DRIVER_H
#define WATCH_BSP_USART_DRIVER_H

#include "stm32f4xx.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"

// middleware
#include "elog.h"

#include "usart.h"

void uart_driver_func(void * arg);

circular_buffer_t * get_ring_buffer_point(void);

#endif //WATCH_BSP_USART_DRIVER_H