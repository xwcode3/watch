//
// Created by 文武 on 2026/3/22.
//

#ifndef WATCH_USART_H
#define WATCH_USART_H

#include <stdint.h>
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "elog.h"

#include "mid_circular_buffer.h"
#include "bsp_usart_driver.h"

/**
 * @brief  initialize USART1, reuse GPIOA Pin9 and Pin10 as UART1 TX/RX pins
 *         initialize DMA2 Stream2 for USART1 RX
 * @param  uint8_t* buffer_address: DMA buffer address;
 * @param  uint32_t buffer_size: DMA buffer size;
 * @retval void
 */
void USART1_DMA2_Stream2_Init(uint8_t* buffer_address, uint32_t buffer_size);

/**
 * @brief 注册回调函数并初始化 DMA 缓冲区
 * @param usart: 串口外设（如 USART1）
 * @param buf: 接收缓冲区
 * @param size: 缓冲区大小
 * @retval void
 */
void UART_RegisterRxEventCallback(USART_TypeDef* usart,uint8_t* buf, uint16_t size);

/**
 * @brief  dma半满传输完成中断回调函数
 * @param  uint32_t number_of_data: 半传输完成时的数据数量
 * @retval void
 */
void dma_half_irq_callback(uint32_t number_of_data);

/**
 * @brief  dma全满传输完成中断回调函数
 * @param  uint32_t number_of_data: 全传输完成时的数据数量
 * @retval void
 */
void dma_comp_irq_callback(uint32_t number_of_data);

/**
 * @brief  usart空闲中断回调函数
 * @param  uint32_t number_of_data: 空闲中断传输完成时的数据数量
 * @retval void
 */
void uart_idle_irq_callback(uint32_t number_of_data);


void USART1_SendByte(uint8_t data);


/**
 * @brief USART1 串口配置函数
 * @param 无
 * @retval 无
 */
void USART1_Config(void);

#if 0
// 函数声明
void USART1_Config(void);           // 串口配置函数
void USART1_SendByte(uint8_t data); // 发送单个字节
void USART1_SendString(char* str);  // 发送字符串
void USART1_IRQHandler(void);       // 中断处理函数
#endif


#endif //WATCH_USART_H
