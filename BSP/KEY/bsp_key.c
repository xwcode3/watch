/******************************************************************************
 * Copyright (C) 2024
 * 
 * All Rights Reserved.
 * 
 * @file bsp_key.c
 * 
 * @par dependencies 
 * - stm32f4xx.h
 * - stdio.h
 * - bsp_key.h
 * - stdint.h
 * 
 * @author xiaowu
 * 
 * @brief Provide the Standard APIs of KEY and corresponding opetions.
 * 
 * Processing flow:
 * 
 * call directly.
 * 
 * @version V1.0 2025-05-10
 *
 * @note 1 tab == 4 spaces!
 * 
 *****************************************************************************/

#include "stm32f4xx.h"

#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "bsp_key.h"



#define FALLING_TYPE  0
#define RASING_TYPE   1

#define KEY_GPIO GPIOA
#define KEY_PORT_PIN GPIO_Pin_0

/**
  * @brief  A function for scanning the status of key presses
  * @param  uint32_t* key_value : key action status
  * @retval key_status_t 
  */
key_status_t key_scan(key_press_status_t * key_value) {
    key_press_status_t key_status_value = KEY_NOT_PRESSED;

    // If PA0 is at a low level, the key is pressed and return KEY_PRESSED.
    if (GPIO_ReadInputDataBit(KEY_GPIO, KEY_PORT_PIN) == Bit_RESET) {
        key_status_value = KEY_PRESSED;
        *key_value = key_status_value;
        return KEY_OK;
    }

    *key_value = key_status_value;
    // The key has never been pressed and a timeout has been returned.
    return KEY_ERRORTIMEOUT;
}

key_status_t key_polling_scan_short_long_press(key_press_status_t * key_value,
                                       uint32_t short_press_time) {
    key_status_t ret_key_status = KEY_ERROR;
    key_press_status_t key_value_temp = KEY_NOT_PRESSED;

    // 1 check if the key is pressed
    ret_key_status = key_scan(&key_value_temp);

    // 1.1 if the key is pressed, then check if it is short pressed
    if (ret_key_status == KEY_OK) {
        if (key_value_temp == KEY_PRESSED) {
            // get the timestamp of the first key press
            uint32_t counter_tick = xTaskGetTickCount();
            while (xTaskGetTickCount() <  counter_tick + short_press_time) {}
            
            ret_key_status = key_scan(&key_value_temp);
            if (key_value_temp == KEY_NOT_PRESSED) {
                printf("key short pressed! key status = %d\r\n", ret_key_status);
                ret_key_status = KEY_OK;
                *key_value = KEY_SHORT_PRESSED;
            } else {
                // 1.2 if the key is pressed with long press
                *key_value = KEY_LONG_PRESSED;
                // wait for key release
                while (key_scan(&key_value_temp) == KEY_OK) {}
            }
        } else {
            
            *key_value = KEY_NOT_PRESSED;
        }
    }
    
    return ret_key_status;
}



// Bit_SET: 1, Bit_RESET: 0
// uint8_t Key_Scan(void) {
//     // Bit_SET: 1, Bit_RESET: 0
//     if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET) {
//         // delay to avoid bouncing
//         for (volatile int i = 0; i < 100000; i++);
//
//         if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET) {
//             // wait for key release
//             while (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET);
//             return 1;   // key pressed
//         }
//     }
//     // key not pressed
//     return 0;
// }

static key_press_status_t key_result = KEY_NOT_PRESSED;
extern QueueHandle_t led_queue;     // defined in bsp_led.c

QueueHandle_t key_queue = NULL;
QueueHandle_t inter_key_queue = NULL;
TaskHandle_t key_TaskHandle = NULL;

// key thread
void key_task(void * pvParameters) {
    printf("key_task running, preparing for key task\r\n");

	// key_status_t key_ret                = KEY_OK;
	// key_press_status_t key_status       = KEY_NOT_PRESSED;
    key_press_event_t* p_key_press_event   = NULL;
    uint32_t event_index                = 0;
    uint32_t first_trigger_tick         = 10101;
    uint32_t short_press_time           = SHORT_PRESS_TIME;
	
	// create the key queue
	// key_queue = xQueueCreate(10, sizeof(key_press_status_t));
    key_queue = xQueueCreate(10, sizeof(key_press_status_t *)); // implement zero-copy function
	if (key_queue == NULL) {
		printf("create key_queue failed!\r\n");
	} else {
		printf("create key_queue success!\r\n");
	}

    // create the inter key queue
    inter_key_queue = xQueueCreate(10, sizeof(key_press_event_t *));
    if (inter_key_queue == NULL) {
        printf("create inter_key_queue failed!\r\n");
    } else {
        printf("create inter_key_queue success!\r\n");
    }

    // inter_key_queue = xQueueCreate(10, sizeof(key_press_event_t));

	uint32_t counter_tick = 0;

	while (1) {
        printf("key task is running!\r\n");
        // 1. check if there is new data about the key press in the queue
        if (xQueueReceive(inter_key_queue, &p_key_press_event, 0) == pdTRUE) {
            printf("inter key queue received! At [%d] ticks\r\n", p_key_press_event->trigger_tick);
            // 1.1 if there is the new data about the key,
            // then update it in state machine
            if (p_key_press_event->edge_type == FALLING && event_index == 0) {
                printf("key FALLING fetched! At [%d] tick\r\n", 
                        p_key_press_event->trigger_tick);
                event_index = 1;
                
                // mark the first tick when event coming
                first_trigger_tick = p_key_press_event->trigger_tick;
                printf("first_trigger_tick: %d\r\n", first_trigger_tick);

            } else if (p_key_press_event->edge_type == RASING && event_index == 0) {
                printf("key RASING fetched! At [%d] tick\r\n",
                        p_key_press_event->trigger_tick);

                // change the index for estimating the state machine
                event_index = 1;
            }
            
            key_press_status_t * p_key_result  = &key_result;

            if (p_key_press_event->edge_type == RASING && event_index == 1) {
                printf("key RASING after the falling\r\n");
                // 1.1.1 if the interval in new key event 
                // between two key is less than 10ms,
                // int n = p_key_press_event->trigger_tick - first_trigger_tick;
                if (p_key_press_event->trigger_tick - first_trigger_tick < 10) {
                    printf("invalid key event, first = [%d] ticks"
                            "new = [%d] ticks", first_trigger_tick, p_key_press_event->trigger_tick);
                    // 1.1.1.1 the new key press event is not valid
                    continue;
                }

                if (p_key_press_event->trigger_tick - first_trigger_tick
                    < short_press_time) {
                    
                    *p_key_result = KEY_SHORT_PRESSED;
                    printf("short p_key_result: %d, 0x%p\r\n", *p_key_result, p_key_result);
                    if (xQueueSendToFront(key_queue, 
                                          &p_key_result, 0) == pdTRUE) {
                        printf("Short : LED_BLINK_1_TIMES send successfully at [%d] tick, first = [%d] ticks"
                            "new = [%d] ticks\r\n", xTaskGetTickCount(), first_trigger_tick, p_key_press_event->trigger_tick);
                        
                    } else {
                       printf("LED_BLINK_1_TIMES send failed\r\n");
                    }

                    event_index = 0;
                }

                // 1.1.2 if the interval in new key event between two key
                // is more than 10ms,
                    // 1.1.2.1 the new key press event is valid
                    // 1.1.2.1.1 if the interval is less than the short_press time
                    // then it should be short press.
                        // 1.1.2.1.2 send the short press message to key_queue
                    // 1.1.2.1.2 if the interval is more than the short_press time
                    // then it should be long press.
                if (p_key_press_event->trigger_tick - first_trigger_tick 
                    > short_press_time) {
                    
                    *p_key_result = KEY_LONG_PRESSED;
                    printf("long p_key_result: %d\r\n", *p_key_result);
                    if (xQueueSendToFront(key_queue, 
                                          &p_key_result, 0) == pdTRUE) {
                        printf("Long : LED_BLINK_3_TIMES send successfully at [%d] tick, first = [%d] ticks"
                            "new = [%d] ticks\r\n", xTaskGetTickCount(),first_trigger_tick, p_key_press_event->trigger_tick);
                        
                    } else {
                       printf("LED_BLINK_3_TIMES send failed\r\n");
                    }

                    event_index = 0;
                }
            }
        }
        
		vTaskDelay(100);
	}
}

void EXTI0_IRQHandler() {
    static uint32_t irq_type = FALLING_TYPE;
    static key_press_event_t key_press_event = {
        .edge_type = FALLING,
        .trigger_tick = 0,
    };
    key_press_event_t* p_key_press_event = &key_press_event;

    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
		EXTI_ClearITPendingBit(EXTI_Line0);
		
        BaseType_t xHigherPriorityTaskWoken;
        if (irq_type == FALLING_TYPE) {


            if (inter_key_queue == NULL) {
                printf("inter_key_queue is NULL\r\n");
            } else {
                key_press_event.edge_type = FALLING;
                key_press_event.trigger_tick = xTaskGetTickCount();
                if (xQueueSendToBackFromISR(inter_key_queue,
                    &p_key_press_event, NULL) == pdTRUE) {
                    printf("key_press_event_1 send FALLING to queue success,"
                         "at [%d] ticks\r\n", xTaskGetTickCount());
                }
            }

            irq_type = RASING_TYPE;

            // 配置EXTI
            EXTI_InitTypeDef EXTI_InitStructure;
            EXTI_InitStructure.EXTI_Line = EXTI_Line0;
            EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
            // 上升沿触发
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;  
            EXTI_InitStructure.EXTI_LineCmd = ENABLE;
            EXTI_Init(&EXTI_InitStructure);

        } else if (irq_type == RASING_TYPE) {
            

            if (inter_key_queue == NULL) {
                printf("inter_key_queue is NULL\r\n");
            } else {
                key_press_event.edge_type = RASING;
                key_press_event.trigger_tick = xTaskGetTickCount();

                if (xQueueSendToBackFromISR(inter_key_queue, 
                        &p_key_press_event, &xHigherPriorityTaskWoken) 
                        == pdTRUE) {
                    printf("key_press_event_2 send RASING to queue success,"
                        " at [%d] ticks\r\n", xTaskGetTickCount());
                }
            }

            irq_type = FALLING_TYPE;

            // 配置EXTI
            EXTI_InitTypeDef EXTI_InitStructure;
            EXTI_InitStructure.EXTI_Line = EXTI_Line0;
            EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  // 下降沿触发
            EXTI_InitStructure.EXTI_LineCmd = ENABLE;
            EXTI_Init(&EXTI_InitStructure);
        }

        // 清除中断标志位
        // EXTI_ClearITPendingBit(EXTI_Line0);
    }
}
