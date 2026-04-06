//
// Created by 文武 on 2026/3/22.
//

#ifndef WATCH_USART_H
#define WATCH_USART_H

#include "stm32f4xx.h"

// 函数声明
void USART1_Config(void);           // 串口配置函数
void USART1_SendByte(uint8_t data); // 发送单个字节
void USART1_SendString(char* str);  // 发送字符串
void USART1_IRQHandler(void);       // 中断处理函数


#endif //WATCH_USART_H
