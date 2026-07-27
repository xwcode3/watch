/******************************************************************************
 * Copyright (C) 2024 xiaowu, Inc.(Gmbh) or its affiliates.
 * 
 * All Rights Reserved.
 * 
 * @file iwdg.c
 * 
 * @par dependencies 
 * - iwdg.h
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

#include "iwdg.h"

void iwdg_init(uint8_t prep, uint16_t rlr) {
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);   // 使能对IWDG的访问权限
    IWDG_SetPrescaler(prep);                        // 设置预分频器
    IWDG_SetReload(rlr);                            // 设置重装载值，1秒钟溢出
    IWDG_ReloadCounter();                           // 重新加载计数器
    IWDG_Enable();                                  // 使能看门狗
}

