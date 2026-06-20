//
// Created by 文武 on 2026/5/10.
//
#include "gpio.h"


void gpio_init() {
    // 配置 NVIC 优先级分组为 Group 4（4位抢占优先级，0位子优先级）
    // 这与 FreeRTOS 的配置兼容
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    
    // led_init();  // 启用 LED 初始化
    // Key_Init();
    // timer_2_init();
    // timer_5_init();
}


// LED硬件GPIO初始化
void led_init(void) {
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef blue_led_pin;
    blue_led_pin.GPIO_Pin = GPIO_Pin_13;
    blue_led_pin.GPIO_Mode = GPIO_Mode_OUT;
    blue_led_pin.GPIO_Speed = GPIO_Speed_2MHz;
    blue_led_pin.GPIO_OType = GPIO_OType_PP;
    blue_led_pin.GPIO_PuPd = GPIO_PuPd_NOPULL;

    // 先初始化 GPIO
    GPIO_Init(GPIOC, &blue_led_pin);
    
    // 再设置初始电平：低电平点亮 LED
    GPIO_ResetBits(GPIOC, GPIO_Pin_13);
}

void Key_Init(void) {
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_Key_InitStructure;
    GPIO_Key_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_Key_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_Key_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Key_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    // 输入模式不需要配置 OType
    // GPIO_Key_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOA, &GPIO_Key_InitStructure);

    // 使能SYSCFG时钟（用于配置外部中断）
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    // 配置SYSCFG外部中断线
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);

    // 配置EXTI
    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  // 下降沿触发
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    // 配置NVIC中断
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;  // PA0对应EXTI0中断
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x07;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

