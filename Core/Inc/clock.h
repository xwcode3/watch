#ifndef WATCH_CLOCK_H
#define WATCH_CLOCK_H

#include "stm32f4xx.h"
#include <stdio.h>

/**
 * @brief  设置系统时钟信息
 * @param  None
 * @retval None
 */
void SystemClock_Config(void);

/**
 * @brief  设置系统时钟信息
 * @param  None
 * @retval None
 */
void SystemClock_Config2(void);

/**
 * @brief  获取系统时钟信息
 * @param  None
 * @retval None
 */
void get_clock_info(void);


#endif //WATCH_CLOCK_H