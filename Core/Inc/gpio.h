//
// Created by 文武 on 2026/5/10.
//

#ifndef WATCH_GPIO_H
#define WATCH_GPIO_H

#include "stm32f4xx.h"
#include "timer.h"

void gpio_init(void);
void led_init(void);
void led_on(void);
void led_off(void);
uint8_t get_led_status(void);
void led_toggle(void);
void Key_Init(void);
// void timer_init(void);


#endif //WATCH_GPIO_H