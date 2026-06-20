/******************************************************************************
 * Copyright (C) 2024 EternalChip, Inc.(Gmbh) or its affiliates.
 * 
 * All Rights Reserved.
 * 
 * @file bsp_led.c
 * 
 * @par dependencies 
 * - stm32f4xx.h
 * - stdio.h
 * - bsp_led.h
 * 
 * @author xiaowu
 * 
 * @brief Provide the Standard APIs of KEY and corresponding opetions.
 * 
 * Processing flow:
 * 
 * call directly.
 * 
 * @version V1.0 2024-04-25
 *
 * @note 1 tab == 4 spaces!
 * 
 *****************************************************************************/
#include "bsp_led.h"

static uint32_t g_blink_times = 0;  // 闪烁次数
static uint32_t g_blink_order = 0;  // 闪烁顺序

// 点亮LED，输出低电平
static void led_on(void) {
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);
}

// 熄灭LED，输出高电平
static void led_off(void) {
	GPIO_SetBits(GPIOC, GPIO_Pin_13);
}

// 切换LED状态
static void led_toggle(void) {
    GPIO_ToggleBits(GPIOC, GPIO_Pin_13);
}

/**
 * @brief  Control LED
 * @param  led_operation_t : led operation
 * @retval led_status_t : led status
 */
// 控制LED的亮灭，或翻转状态
led_status_t led_control(led_operation_t operation) {
    led_status_t led_status = LED_OK;
    
    switch (operation) {
        case LED_ON:
            led_on();
            break;
        case LED_OFF:
            led_off();
            break;
        case LED_TOGGLE:
            led_toggle();
            break;
        case LED_BLINK_1_TIMES:
            g_blink_times   = 1;
            g_blink_order   = 0;
            break;
        case LED_BLINK_3_TIMES:
            for (int i = 0; i < 6; i++) {
                led_toggle();
                vTaskDelay(300);
            }
            break;
        case LED_BLINK_10_TIMES:
            g_blink_times   = 10;
            g_blink_order   = 0;
            break;
        default:
            printf("led control error: Invalid operation/r/n");
            led_status = LED_ERROR;
            break;
    }

    return led_status;
}


/**
 * @brief  Get LED status
 * @param  void
 * @retval led_operation_t
 */
// 0: 灭; 1: 点亮
// LED_ON：亮，LED_OFF：灭
led_operation_t get_led_status(void) {
    led_operation_t status = LED_OFF;
    if (GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13) == Bit_RESET) {
        status = LED_ON;
    }
	return status;
}



QueueHandle_t led_queue = NULL;        // 传输led闪烁状态的队列
TaskHandle_t led_TaskHandle = NULL;    // led线程句柄
/**
 * @brief  led thread
 * @param  pvParameters: task parameters (unused)
 * @retval void
 */
void led_task(void *pvParameters) {
	led_operation_t* p_led_operation = NULL;
    led_status_t led_ret = LED_ERROR;
	uint32_t loop_count = 0;  // for stack monitoring
	
	// create a queue
	led_queue = xQueueCreate(10, sizeof(led_operation_t*));
	if (led_queue == NULL) {
		printf("create led_queue failed!\r\n");
	} else {
		printf("create led_queue success!\r\n");
	}

	while (1) {
		printf("led_task run!\r\n");
		
        if (led_queue != NULL) {
            if (xQueueReceive(led_queue, &p_led_operation, 0) == pdTRUE) {
                printf("led_task receive operation: %d\r\n", *p_led_operation);
                led_ret = led_control(*p_led_operation);
                if (led_ret != LED_OK) {
                    printf("led_task control failed!\r\n");
                }
            }
        }

		vTaskDelay(100);
	}
}


/**
 * @brief  led control by irq
 * @param  void
 * @retval void
 */
led_status_t led_off_on_irq(void) {
    // printf("led on off\r\n");
    if (g_blink_times > 0) {
        if (g_blink_order % 2 == 0) {
            led_on();
        } else {
            led_off();
            g_blink_times--;
        }
        g_blink_order++;
    } else {
        g_blink_order = 0;
    }

    return LED_OK;
}
