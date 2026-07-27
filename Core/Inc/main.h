/**
  ******************************************************************************
  * @file    Project/STM32F4xx_StdPeriph_Templates/main.h 
  * @author  MCD Application Team
  * @version V1.8.1
  * @date    27-January-2022
  * @brief   Header for main.c module
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
  
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "SEGGER_RTT.h"
#include "elog.h"
#include "ymodem.h"

#include "usart.h"
#include "FreeRTOS_tasks.h"
#include "gpio.h"
#include "spi.h"
#include "boot_manager.h"
#include "clock.h"

// #include "bsp_adc.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_w25qxx.h"
#include "bsp_w25qxx_handler.h"


/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */








/**
  * @brief  EasyLogger 配置
  * @note
  * @retval None
  */
void easylogger_config(void);





#endif /* __MAIN_H */


