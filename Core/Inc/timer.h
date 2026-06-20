//
// Created by 文武 on 2026/5/11.
//

#ifndef WATCH_TIMER_H
#define WATCH_TIMER_H

#include "stm32f4xx.h"
#include "bsp_led.h"


/**
 * @brief  微秒级延时
 * @param  us: 延时微秒数，范围0-4294967295
 * @retval 无
 */
void delay_us(uint32_t us);

/**
 * @brief  毫秒级延时
 * @param  ms: 延时毫秒数
 * @retval 无
 */
void delay_ms(uint32_t ms);

/**
 * @brief  Init TIM2
 * @param  void
 * @retval void
 */
void timer_2_init(void);

/**
 * @brief  Init TIM5
 * @param  void
 * @retval void
 */

void timer_5_init(void);


/**
 * @brief  TIM5中断服务程序
 * @param  void
 * @retval void
 */
void TIM5_IRQHandler(void);

#endif //WATCH_TIMER_H