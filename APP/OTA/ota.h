/******************************************************************************
 * Copyright (C) 2024
 * 
 * All Rights Reserved.
 * 
 * @file ota.h
 * 
 * @par dependencies 
 * - stm32f4xx.h
 * - stdio.h
 * - stdint.h
 * 
 * @author xiaowu
 * 
 * @brief sta task
 * 
 * Processing flow:
 * 
 * call directly.
 * 
 * @version V1.0 2025-05-10
 *
 * @note 1 tab == 4 spaces!
 * 
 *****************************************************************************/

#ifndef __OTA_H__
#define __OTA_H__

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "ymodem.h"
#include "usart.h"
#include "bsp_AT24Cxx_Driver.h"
#include "bsp_w25qxx_handler.h"


/**
 * @brief  OTA状态枚举类型
 */
typedef enum {
    WaitReqDownload = 0,    // 等待服务器请求
    OtaDownload,            // OTA数据下载中
    WaitReqUpdate,          // 等待服务器请求更新APP
    OtaEnd                  // OTA最终结果
} E_Ota_State;


/**
 * @brief  ota task
 * @param  void
 * @retval void
 */
TaskFunction_t ota_task(void* pvParameters);

#endif /* __OTA_H__ */

