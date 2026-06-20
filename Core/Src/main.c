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

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "SEGGER_RTT.h"
#include "elog.h"

#include "usart.h"
#include "FreeRTOS_tasks.h"
#include "gpio.h"

#include "bsp_adc.h"

/** @addtogroup Template_Project
  * @{
  */ 

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static __IO uint32_t uwTimingDelay;
RCC_ClocksTypeDef RCC_Clocks;

/* FreeRTOS 是否已启动的标志 */
// volatile uint8_t g_freertos_started = 0;

/* Private function prototypes -----------------------------------------------*/
static void Delay(__IO uint32_t nTime);
void easylogger_config(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
 /*!< At this stage the microcontroller clock setting is already configured, 
       this is done through SystemInit() function which is called from startup
       files before to branch to application main.
       To reconfigure the default setting of SystemInit() function, 
       refer to system_stm32f4xx.c file */

  /* SysTick end of count event each 10ms */
  // RCC_GetClocksFreq(&RCC_Clocks);
  // SysTick_Config(RCC_Clocks.HCLK_Frequency / 100);
  SystemClock_Config();
  gpio_init();

  // 这里是因为 easylogger 初始化以及一些 printf、log_i 函数调用输出需要串口
  USART1_Config();

  get_clock_info();

  easylogger_config();

  // bsp_adc_init();

  /* Add your application code here */
  app_task_create();

  // g_freertos_started = 1;


  vTaskStartScheduler();

  /* Infinite loop */
  for (;;)
  {
    SEGGER_RTT_printf(0, "OI hello, world!\r\n");

    // LED 闪烁测试
    GPIO_ResetBits(GPIOC, GPIO_Pin_13);  // 点亮
    Delay(50);
    GPIO_SetBits(GPIOC, GPIO_Pin_13);    // 熄灭
    Delay(50);

    Delay(50);
    log_i("easy logger test");
    log_e("error test");
    Delay(50);
    printf("printf test\r\n");
    Delay(50);
  }

}


/**
  * @brief  EasyLogger 配置
  * @note
  * @retval None
  */
void easylogger_config(void) {
  elog_init();
  elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_DIR | ELOG_FMT_FUNC | ELOG_FMT_LINE);
  elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_FUNC | ELOG_FMT_LINE);
  elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_LVL | ELOG_FMT_DIR | ELOG_FMT_FUNC | ELOG_FMT_LINE);
  elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_LVL | ELOG_FMT_DIR | ELOG_FMT_FUNC | ELOG_FMT_LINE);
  elog_start();
}


/**
 * @brief 重定向 printf 到 USART1
 * @note 这样就能直接使用 printf 函数了
 */
int __io_putchar(int ch)
{
    USART1_SendByte((uint8_t)ch);  // 将printf函数重定向输出到串口
  // SEGGER_RTT_PutChar(0, ch); // 将字符发送到RTT的0号通道
    return ch;
}

/**
  * @brief  Inserts a delay time.
  * @param  nTime: specifies the delay time length, in milliseconds.
  * @retval None
  */
void Delay(__IO uint32_t nTime)
{ 
  uwTimingDelay = nTime;

  while(uwTimingDelay != 0);
}

/**
  * @brief  Decrements the TimingDelay variable.
  * @param  None
  * @retval None
  */
void TimingDelay_Decrement(void)
{
  if (uwTimingDelay != 0x00)
  { 
    uwTimingDelay--;
  }
}

/**
  * @brief  标记 FreeRTOS 已启动
  * @note   在 vTaskStartScheduler() 之前调用
  * @param  None
  * @retval None
  */
void FreeRTOS_Started(void)
{
  // g_freertos_started = 1;
}


// 配置使用外部高速时钟 HSE 作为时钟源
void SystemClock_Config(void) {
  ErrorStatus HSEStartUpStatus;
  RCC_DeInit();	// 复位 RCC 时钟配置为默认值

  RCC_HSEConfig(RCC_HSE_ON);	// 使能 HSE
  HSEStartUpStatus = RCC_WaitForHSEStartUp();	// 等待 HSE 启动

  if (HSEStartUpStatus == SUCCESS) {
    // 配置 PLL
    RCC_PLLConfig(RCC_PLLSource_HSE, 12, 96, 2, 4);
    // 使能 PLL
    RCC_PLLCmd(ENABLE);

    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {
      // 等待 PLL 启动
    }

    // 配置系统时钟
    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    // 配置 APB1 和 APB2 时钟
    RCC_PCLK1Config(RCC_HCLK_Div2);
    RCC_PCLK2Config(RCC_HCLK_Div1);
    // 设置系统时钟
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    // 等待系统时钟切换到 PLL
    while (RCC_GetSYSCLKSource() != 0x08) {}
  } else {
    while (1) {}
    // 配置失败，死循环
  }
}

// 获取当前系统时钟频率
void get_clock_info(void) {
  RCC_ClocksTypeDef RccClocks;
  RCC_GetClocksFreq(&RccClocks);

  printf("SYSCLK Frequency: %lu Hz\r\n", RccClocks.SYSCLK_Frequency);
  printf("HCLK Frequency: %lu Hz\r\n", RccClocks.HCLK_Frequency);
  printf("PCLK1 Frequency: %lu Hz\r\n", RccClocks.PCLK1_Frequency);
  printf("PCLK2 Frequency: %lu Hz\r\n", RccClocks.PCLK2_Frequency);
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


