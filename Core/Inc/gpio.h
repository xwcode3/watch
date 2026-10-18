//
// Created by 文武 on 2026/5/10.
//

#ifndef WATCH_GPIO_H
#define WATCH_GPIO_H

#include "stm32f4xx.h"
#include "timer.h"

void gpio_init(void);
void led_init(void);
void key_init(void);

void mpu_int_init(void); // 初始化MPU6050中断


#endif //WATCH_GPIO_H