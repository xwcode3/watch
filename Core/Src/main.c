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

#define APP_FLASH_ADDR 0x8019000    // APP 的起始烧录地址

static __IO uint32_t uwTimingDelay;
RCC_ClocksTypeDef RCC_Clocks;

/* Private function prototypes -----------------------------------------------*/

typedef void (*pFunction) (void);   // 声明函数指针
static pFunction JumpToAppLocation;

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
  // RCC_ClockSecuritySystemCmd(ENABLE);
  SystemClock_Config();
  gpio_init();

  // 这里是因为 easylogger 初始化以及一些 printf、log_i 函数调用输出需要串口
  USART1_Config();

  get_clock_info();

  JumpToApp();

  // easylogger_config();

  // bsp_adc_init();

  /* Add your application code here */
  // app_task_create();

  // vTaskStartScheduler();

  /* Infinite loop */
  for (;;)
  {

  }

}


void disable_peripherals(void)
{
  // 关闭 USART1 在 NVIC 中的中断使能
  NVIC_DisableIRQ(USART1_IRQn);
  // 轮询等待发送完成标志（TC）被置位
  while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
  {

  }
  // 禁用 USART1
  USART_Cmd(USART1, DISABLE);

  RCC_RTCCLKCmd(DISABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, DISABLE);
  __disable_irq();
}

void JumpToApp(void)
{
  uint32_t jumpAddr;
  Delay(2000);

  printf("bootloader running...\r\n");

  Delay(1000);


  printf("delay 1s end\r\n");
  uint32_t app_msp = *(__IO uint32_t*)APP_FLASH_ADDR;
  printf("APP MSP Value: 0x%08X\r\n", app_msp);

  printf("Disable all peripherals and interrupts, jump to APP\r\n");
  disable_peripherals();

  if (((*(__IO uint32_t*)APP_FLASH_ADDR) & 0x2FFE0000) == 0x20020000)
  {
    jumpAddr = *(__IO uint32_t*)(APP_FLASH_ADDR + 4);

    JumpToAppLocation = (pFunction)jumpAddr;

    __set_MSP(*(__IO uint32_t*)APP_FLASH_ADDR);

    JumpToAppLocation();
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

int __io_putchar(int ch)
{
    USART1_SendByte((uint8_t)ch);  // 将printf函数重定向输出到串口
  // SEGGER_RTT_PutChar(0, ch); // 将字符发送到RTT的0号通道
    return ch;
}

void Delay(__IO uint32_t nTime)
{ 
  uwTimingDelay = nTime;

  while(uwTimingDelay != 0);
}


void TimingDelay_Decrement(void)
{
  if (uwTimingDelay != 0x00)
  { 
    uwTimingDelay--;
  }
}


void SystemClock_Config(void) {
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 使能 PWR 时钟

  PWR->CR |= PWR_CR_VOS; // 设置电压调节器输出电压范围 1 (1.2V)

  while (PWR_GetFlagStatus(PWR_FLAG_VOSRDY) == RESET);

  /* 使能HSE并等待稳定 */
  RCC_HSEConfig(RCC_HSE_ON);
  while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET);

  RCC_PLLConfig(RCC_PLLSource_HSE, 12, 96, 2, 4);

  RCC_PLLCmd(ENABLE);
  while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

  FLASH_PrefetchBufferCmd(ENABLE);
  FLASH_InstructionCacheCmd(ENABLE);
  FLASH_DataCacheCmd(ENABLE);
  FLASH_SetLatency(FLASH_Latency_3);

  RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
  RCC_HCLKConfig(RCC_SYSCLK_Div1);
  RCC_PCLK1Config(RCC_HCLK_Div2);
  RCC_PCLK2Config(RCC_HCLK_Div1);

  SystemCoreClockUpdate();
  SysTick_Config((SystemCoreClock / 1000));  // 1ms
}

// 配置使用外部高速时钟 HSE 作为时钟源
void SystemClock_Config2(void) {
  ErrorStatus HSEStartUpStatus;

  // 1. 复位 RCC 时钟配置为默认值
  RCC_DeInit();

  // 2. 开启 PWR 时钟并配置电压调节器（跑 100MHz 必须开启 Scale1）
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
  PWR_MainRegulatorModeConfig(PWR_Regulator_Voltage_Scale1);

  // 3. 配置 Flash 等待周期为 3 个时钟周期（100MHz 必须配 3WS）
  FLASH_SetLatency(FLASH_Latency_3);

  // 4. 使能 HSE 并等待启动
  RCC_HSEConfig(RCC_HSE_ON);
  HSEStartUpStatus = RCC_WaitForHSEStartUp();

  if (HSEStartUpStatus == SUCCESS)
  {
    // 5. 配置 PLL：HSE(25MHz) / 12 = 2.083MHz -> * 96 = 200MHz -> / 2 = 100MHz
    RCC_PLLConfig(RCC_PLLSource_HSE, 12, 96, 2, 4);

    // 6. 使能 PLL 并等待锁定
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {}

    // 7. 配置 AHB/APB 总线分频器
    RCC_HCLKConfig(RCC_SYSCLK_Div1);      // AHB = 100MHz
    RCC_PCLK1Config(RCC_HCLK_Div2);       // APB1 = 50MHz (最高限制50MHz)
    RCC_PCLK2Config(RCC_HCLK_Div1);       // APB2 = 100MHz

    // 8. 切换系统时钟源到 PLL
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while (RCC_GetSYSCLKSource() != 0x08) {} // 等待 PLL 成为系统时钟

    SysTick_Config(100000); // 1ms，开启SysTick中断
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

  printf("system core clock = %lu Hz\r\n", SystemCoreClock);
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


