//
// Created by 文武 on 2026/5/11.
//
#include "timer.h"

/**
 * @brief  微秒级延时
 * @param  us: 延时微秒数，范围0-4294967295
 * @retval 无
 */
void delay_us(uint32_t us) {
    uint32_t start_time = TIM2->CNT;
    while ((TIM2->CNT - start_time) < us) {}
}

/**
 * @brief  毫秒级延时
 * @param  ms: 延时毫秒数
 * @retval 无
 */
void delay_ms(uint32_t ms) {
    while (ms--) {
        delay_us(1000); // 1ms = 1000us
    }
}

/**
 * @brief  Init TIM2
 * @param  void
 * @retval void
 */
void timer_2_init() {
    // TIM2时钟初始化
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // TIM2初始化
    TIM_TimeBaseInitTypeDef TIM_2_TimeBaseStructure;
    // 这里Period就是ARR自动重装寄存器的值，注意不能超过0xFFFFFFFF。
    TIM_2_TimeBaseStructure.TIM_Period = 0xFFFFFFFF - 1;
    // PSC预分频器系数和实际值相差1，这里计数频率为1MHz
    TIM_2_TimeBaseStructure.TIM_Prescaler = 100 - 1;

#if 0 // 示例：设置表示以1MHz频率计1000000个数为1s，则每计一个数为1us
    // 这里Period就是ARR自动重装寄存器的值，注意不能超过0xFFFFFFFF。
    TIM_2_TimeBaseStructure.TIM_Period = 1000000 - 1;
    // PSC预分频器系数和实际值相差1
    TIM_2_TimeBaseStructure.TIM_Prescaler = 100 - 1;
#endif

    // 电平采样时钟分频（一般随意设置即可）
    TIM_2_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    // TIM2计数模式选择，这里选择向上计数模式
    TIM_2_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_2_TimeBaseStructure);

    // 启动TIM2
    TIM_Cmd(TIM2, ENABLE);
}

/**
 * @brief  Init TIM5
 * @param  void
 * @retval void
 */

void timer_5_init(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

    TIM_TimeBaseInitTypeDef TIM_5_TimeBaseStructure;
    TIM_5_TimeBaseStructure.TIM_Period = 1000;
    TIM_5_TimeBaseStructure.TIM_Prescaler = 10000 - 1;
    TIM_5_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_5_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM5, &TIM_5_TimeBaseStructure);

    TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);

    NVIC_InitTypeDef NVIC_Timer_5_InitStructure;
    NVIC_Timer_5_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
    NVIC_Timer_5_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x06;
    NVIC_Timer_5_InitStructure.NVIC_IRQChannelSubPriority = 0x00;
    NVIC_Timer_5_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_Timer_5_InitStructure);

    TIM_Cmd(TIM5, ENABLE);
}

/**
 * @brief  TIM5中断服务程序
 * @param  void
 * @retval void
 */
void TIM5_IRQHandler(void) {
    if (TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM5, TIM_IT_Update);
        led_off_on_irq();
    }
}