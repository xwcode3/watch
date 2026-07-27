/******************************************************************************
 * Copyright (C) 2024 xiaowu, Inc.(Gmbh) or its affiliates.
 * 
 * All Rights Reserved.
 * 
 * @file iwdg.h
 * 
 * @par dependencies 
 * 
 * @author xiaowu
 * 
 * @brief Functions related to reading and writing in the chip's flash area.
 * 
 * Processing flow:
 * 
 * call directly.
 * 
 * @version V1.0 2024-09-13
 *
 * @note 1 tab == 4 spaces!
 * 
 *****************************************************************************/
#ifndef WATCH_IWDG_H
#define WATCH_IWDG_H

#include "stm32f4xx.h"


/**
 * @brief  Initialize the independent watchdog timer (IWDG)
 * @param  prep: Prescaler value for the IWDG
 * @param  rlr: Reload value for the IWDG
 * @retval None
 */
void iwdg_init(uint8_t prep, uint16_t rlr);



#endif /* WATCH_IWDG_H */
