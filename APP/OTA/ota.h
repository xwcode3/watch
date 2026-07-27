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


typedef enum {
    OTA_STATE_NO_APP_UPDATE = 0x00,
    OTA_STATE_APP_DOWNLOADING = 0x11,
    OTA_STATE_APP_DOWNLOAD_COMPLETE = 0x22,
    OTA_STATE_APP_FIRST_CHECK_START = 0x33,
    OTA_STATE_APP_FIRST_CHECKING = 0x44,
    OTA_STATE_APP_NEED_UPDATE = 0x55,
} ota_state;

/**
 * @brief  OTA数据包结构体，用于队列传递
 */
typedef struct {
    uint8_t *data;      /* 数据指针，NULL 表示传输结束 */
    int32_t  length;    /* 数据长度 */
} st_ota_packet;

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
 * @retval TaskHandle_t
 */
void ota_task(void * pvParameters);

/**
 * @brief reset the system
 * @param void
 * @return void
*/
void SoftReset(void);

#endif /* __OTA_H__ */

