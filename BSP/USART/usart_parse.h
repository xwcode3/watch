//
// Created by 文武 on 2026/6/16.
//

#ifndef WATCH_USART_PARSE_H
#define WATCH_USART_PARSE_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "elog.h"

#include "bsp_usart_driver.h"
#include "mid_circular_buffer.h"

#define FRAME_NOT_DETECTED  (0x01)  // 未检测到帧
#define FRAME_HEAD          (0x02)  // 帧头
#define FRAME_TAIL          (0x03)  // 帧尾

#define FRAME_HEAD_FLAG (0xFE)    // 帧头标志
#define FRAME_TAIL_FLAG (0xFF)    // 帧尾标志

void usart1_parse_task(void * arg);

#endif //WATCH_USART_PARSE_H