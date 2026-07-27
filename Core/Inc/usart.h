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

/**
  * @brief  串口发送字节函数，使用USART2或USART1，使用阻塞方式
  * @param  USARTx USART2, USART1
  * @param  data 数据字节
  * @retval None
  */
void USART_SendChar(USART_TypeDef* USARTx, uint8_t data);

/**
  * @brief  串口接收字节函数，使用USART2或USART1，使用阻塞方式
  * @param  USARTx USART2, USART1
  * @retval 返回数据
  */
uint8_t USART_ReceiveChar(USART_TypeDef* USARTx);


/**
 * @brief  串口接收字节函数，使用USART2或USART1，使用DMA方式
 * @param  usartx: USART外设
 * @param  buffer: 接收缓冲区
 * @param  length: 缓冲区长度
 * @retval void
 */
void uartx_receive2idle_dma( USART_TypeDef* usartx, uint8_t* buffer, uint16_t length);

/**
 * @brief  用于 OTA 任务的 USART1 DMA配置函数
 * @param  void
 * @retval void
 */
void ota_usart1_dma_config(void);

/**
 * @brief  用于 ota 任务的 USART1 配置函数
 * @param  void
 * @retval void
 */
void ota_usart1_config(void);


/**
 * @brief  串口发送函数，使用USART1，使用阻塞方式
 * @param  buffer: 要发送的数据缓冲区
 * @param  length: 要发送的数据长度
 * @retval void
 */
void usart1_transmit(uint8_t *buffer, uint16_t length);


/**
 * @brief  进入 Ymodem 接收模式：关闭 USART1 的 DMA 转运，改为轮询 DR 接收
 * @note   在调用 Ymodem_Receive_2Buffer() 之前调用。
 *         ymodem 协议栈通过 SerialKeyPressed() 直接轮询 USART1->DR，
 *         不需要 DMA，也不需要空闲线中断，因此这里把 DMA 接收和空闲线中断都关掉，
 *         避免 DMA 与轮询抢字节、以及空闲线误触发命令队列。
 */
void YmodemUart_EnterRxMode(void);


/**
 * @brief  恢复为当前的 DMA 转运模式（OTA 命令接收）
 * @param  cmd_buf : 恢复后用于接收 OTA 命令的缓冲区（如 s_au8_OtaCmd）
 * @param  cmd_len : 命令缓冲区长度
 * @note   在 Ymodem_Receive_2Buffer() 返回之后调用，
 *         把串口交还给 OTA 命令等待状态（一次性 DMA + 空闲线通知）。
 */
void YmodemUart_RestoreDmaMode(uint8_t* cmd_buf, uint16_t cmd_len);

#endif //WATCH_USART_H
