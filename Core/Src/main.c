/**
  ******************************************************************************
  * @file    Project/STM32F4xx_StdPeriph_Templates/main.c 
  * @author  MCD Application Team
  * @version V1.8.1
  * @date    27-January-2022
  * @brief   Main program body
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

/* Includes ------------------------------------------------------------------*/

#include "main.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/



/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void) {
    /*!< At this stage the microcontroller clock setting is already configured, 
       this is done through SystemInit() function which is called from startup
       files before to branch to application main.
       To reconfigure the default setting of SystemInit() function, 
       refer to system_stm32f4xx.c file 
    */

    SystemClock_Config();


    // 配置 NVIC 优先级分组为 Group 4（4位抢占优先级，0位子优先级）
    // 这与 FreeRTOS 的配置兼容
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    gpio_init();

    // 这里是因为 easylogger 初始化以及一些 printf、log_i 函数调用输出需要串口
    USART1_Config();
    printf("\r\n");

    get_clock_info();


    easylogger_config();

    spi1_init();  // 初始化 SPI1
    // 初始化 W25Q64 FLASH
    if (W25Q64_Init() == 0) {
      printf("W25Q64_Init ok\r\n");
    }

    if (1 == ee_CheckOk()) {
      printf("Bootloadwe EEPROM is OK!\r\n");
    }

    OTA_StateManager();  // OTA 状态机处理函数

    /* Infinite loop */
    for (;;)
    {

    }
}



void easylogger_config(void) {
  elog_init();
  elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_DIR | ELOG_FMT_FUNC | ELOG_FMT_LINE);
  elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_FUNC | ELOG_FMT_LINE);
  elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_LVL | ELOG_FMT_DIR | ELOG_FMT_FUNC | ELOG_FMT_LINE);
  elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_LVL | ELOG_FMT_DIR | ELOG_FMT_FUNC | ELOG_FMT_LINE);
  elog_start();
}


#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */


