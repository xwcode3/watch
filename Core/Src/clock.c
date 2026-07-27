#include "clock.h"

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
  printf("================ Bootloader ==============\r\n");
  RCC_ClocksTypeDef RccClocks;
  RCC_GetClocksFreq(&RccClocks);

  printf("SYSCLK Frequency: %lu Hz\r\n", RccClocks.SYSCLK_Frequency);
  printf("HCLK Frequency: %lu Hz\r\n", RccClocks.HCLK_Frequency);
  printf("PCLK1 Frequency: %lu Hz\r\n", RccClocks.PCLK1_Frequency);
  printf("PCLK2 Frequency: %lu Hz\r\n", RccClocks.PCLK2_Frequency);

  printf("system core clock = %lu Hz\r\n", SystemCoreClock);
}