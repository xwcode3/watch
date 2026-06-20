#ifndef _BSP_ADC_H_
#define _BSP_ADC_H_

#include "stm32f4xx.h"
#include <stdlib.h>
#include <string.h>
#include "elog.h"
#include "FreeRTOS.h"
#include "queue.h"

#include "timer.h"

#define BUFFER_SIZE 1

#define ADC_GPIO_PORT GPIOA
#define ADC_GPIO_PIN  GPIO_Pin_0

/**
 * @brief  初始化adc、dma以及初始化两个adc使用的buffer
 * @param  void
 * @retval void
 */
void bsp_adc_init(void);

/**
 * @brief  初始化 adc_queue 消息队列
 * @param  void
 * @retval 0：成功, -1：失败
 */
int adc_quque_init(void);

/**
 * @brief  初始化 adc_queue2 消息队列
 * @param  void
 * @retval 0：成功, -1：失败
 */
int adc_quque2_init(void);


/**
 * @brief  修改DMA搬运数据的目标地址
 * @param  uint32_t* addr：目标地址
 * @retval void
 */
void bsp_adc_change_dma_mem_addr(uint32_t* addr);

void ADC_DMA_Restart(ADC_TypeDef* ADCx, DMA_Stream_TypeDef* DMA_Streamx, uint32_t* pBuffer, uint32_t Length);


#endif
