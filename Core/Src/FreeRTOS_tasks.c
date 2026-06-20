//
// Created by 文武 on 2026/4/22.
//

#include "FreeRTOS_tasks.h"



extern QueueHandle_t key_queue;
extern QueueHandle_t led_queue;        // 传输led闪烁状态的队列


static SemaphoreHandle_t g_xMutex;
static TaskHandle_t g_dma_handle, g_adc_handle;

TaskHandle_t key_led_task_handle;
void key_led_handle_task(void * pv) {
    static led_operation_t led_operation 	= 	LED_INITED_VALUE;
    key_press_status_t *	p_key_value 	= 	NULL;

    while (led_queue == NULL) {
        vTaskDelay(100); // 这里要注意需要先让 key 和 led 任务中的相关队列创建成功
    }

    while (1) {
        if (xQueueReceive(key_queue, &p_key_value, 0) == pdTRUE) {
            led_operation_t* p_led_operation = &led_operation;
            if (*p_key_value == KEY_SHORT_PRESSED) {
                led_operation = LED_BLINK_1_TIMES;
                if (xQueueSendToBack(led_queue, &p_led_operation, 0) != pdTRUE) {
                    printf("send to led_queue failed\r\n");
                }
            } else if (*p_key_value == KEY_LONG_PRESSED) {
                led_operation = LED_BLINK_10_TIMES;
                if (xQueueSendToBack(led_queue, &p_led_operation, 0) != pdTRUE) {
                    printf("send to led_queue failed\r\n");
                }
            } else {
                printf("key press event error: %d\r\n", *p_key_value);
            }
        }
        // led_off_on_irq();
        // printf("key_led_handle_task run!\r\n");
        vTaskDelay(10);
    }
}

#define BIT(x) (1 << x)
#define PRODUCED(x) BIT(x)
#define CONSUMING(x) BIT(x)
#define BIT_DMA BIT(8)

uint32_t *pdata;
uint8_t w_ch = 0, r_ch = 0;
void dma_handler(void * pvParameters)
{
    uint32_t adc_evt_mask = 0, dma_evt_mask = 0;
    pdata = malloc(8);  // 8个字节，相当于 2 个 uint32_t数据
    if (!pdata)
    {
        printf("malloc error\r\n");
        return;
    }
    memset(pdata, 0, 8);

    /* 0. start first dma transfer */
    /* &pdata[w_ch] -> &pdata[0] / &pdata[1] */
    ADC_DMA_Restart(ADC1, DMA2_Stream0, &pdata[w_ch], 1);

    while (1)
    {
        /* 1. wait until dma transfer finished */
        do
        {
            xTaskNotifyWait(0x0, BIT_DMA, &dma_evt_mask, portMAX_DELAY);
        } while (0 == (dma_evt_mask & BIT_DMA));

        /* 2.notify adc thread data produced done */
        xTaskNotifyAndQuery(g_adc_handle, PRODUCED(w_ch), eSetBits, &adc_evt_mask);

        printf("previous adc_evt_mask 0x%x\r\n", adc_evt_mask);

        // 这里是如果另一块 buffer 已被消费则要先开启 adc、dma的转换，因为转换过程可以与 cpu 并行处理
        /* 3. let dma start next transfer */
        w_ch = !w_ch; /* change next write ch */
        if (adc_evt_mask & PRODUCED(w_ch)) {
            /* 3.1 buffer is full, wait adc app consuming data */
            while (0 == (dma_evt_mask & CONSUMING(w_ch))) {
                xTaskNotifyWait(0x0, CONSUMING(w_ch), &dma_evt_mask, portMAX_DELAY);
            }

            // 这里 dma 线程并不直接处理数据，为什么还要获取互斥锁呢？
            // 为了防止 数据处理线程 被其它高优先级任务抢占，数据处理线程中使用了同款互斥锁
            // dma 线程获取互斥锁后
            // 若发现数据处理线程被抢占则会用优先级继承的机制强行提高数据处理线程的优先级，
            // 保证 adc dma 产生数据的实时性
            /* 3.2 wait adc app consumed data finish, take mutex */
            xSemaphoreTake(g_xMutex, portMAX_DELAY);
            xSemaphoreGive(g_xMutex); /* give mutex */
        } else {
            /* buffer may already consumed, clear consuming bit */
            // 这里 NULL 表示自身任务，如果要清除其它任务的通知值，则需要该任务的任务句柄
            ulTaskNotifyValueClear(NULL, CONSUMING(w_ch));
        }

        // vTaskDelay(2000);
        // printf("dma app delay 2s\r\n");

        /* 3.3 start next transfer */
        ADC_DMA_Restart(ADC1, DMA2_Stream0, &pdata[w_ch], 1);

    }
}



void adc_app(void * pvParameters)
{
    uint32_t adc_evt_mask = 0;
    while (1)
    {
        /* 1. wait any data finish, not clear PRODUCED(0) | PRODUCED(1) */
        xTaskNotifyWait(0x0, 0x0, &adc_evt_mask, portMAX_DELAY);

        /* 2. get mutex and log data */
        for (int i = 0; i < 2; i++)
        {
            if (adc_evt_mask & PRODUCED(i))
            {
                /* 2.1 take mutex, and set consuming bit */
                // xQueueSemaphoreTake(g_xMutex, portMAX_DELAY);
                xSemaphoreTake(g_xMutex, portMAX_DELAY);
                xTaskNotify(g_dma_handle, CONSUMING(i), eSetBits);

                /* 2.2 consuming data */
                printf("buffer %d, adc code is %d\r\n", i, pdata[i]);

                /* 2.3 after consuming done, clear produced bit */
                ulTaskNotifyValueClear(NULL, PRODUCED(i));

                xSemaphoreGive(g_xMutex);
            }
        }
    }
}



/* DMA2 Stream0中断处理函数 */
void DMA2_Stream0_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0))
    {
        /* 清除DMA2 Stream0传输完成标志 */
        DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE, xResult;
        xResult = xTaskNotifyFromISR(g_dma_handle, BIT_DMA, eSetBits, &xHigherPriorityTaskWoken);
        if (xResult == pdPASS)
        {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

void adc_and_dma_init()
{
    g_xMutex = xSemaphoreCreateMutex();
    if (g_xMutex == NULL)
    {
        printf("create g_xMutex failed\r\n");
    }

    xTaskCreate(adc_app, "adc_app", 0x200, NULL, 1, &g_adc_handle);
    if (!g_adc_handle)
    {
        printf("create adc app task failed\r\n");
    }

    xTaskCreate(dma_handler, "dma app", 0x200, NULL, 2 ,&g_dma_handle);
    if (!g_dma_handle)
    {
        printf("create dma_handler task failed\r\n");
    }

    return;
}


extern TaskHandle_t usart1_parse_handle;    // 解包任务句柄
extern TaskHandle_t usart_driver_handle;    // 串口驱动任务句柄
void usart_dma_init()
{
    xTaskCreate(usart1_parse_task, "usart1_parse_task", 256, NULL, 4, &usart1_parse_handle);
    if (usart1_parse_handle == NULL)
    {
        log_e("usart driver thread create failed");
    }

    xTaskCreate(uart_driver_func, "usart_driver", 256, NULL, 3, &usart_driver_handle);
    if (usart_driver_handle == NULL)
    {
        log_e("usart driver thread create failed");
    }

    return;
}


void app_task_create(void)
{
#if 0
    BaseType_t task_create_ret = pdFAIL;
    task_create_ret = xTaskCreate(key_task, "key_task", 256, NULL, 4, &key_TaskHandle);
    if (task_create_ret != pdPASS) {
        printf("key_task create failed\r\n");
    }

    task_create_ret = xTaskCreate(led_task, "led_task", 128, NULL, 2, &led_TaskHandle);
    if (task_create_ret != pdPASS)
    {
        printf("led_task create failed\r\n");
    }

    task_create_ret = xTaskCreate(key_led_handle_task, "led_key",
            128, NULL, 3, &key_led_task_handle);
    if (task_create_ret != pdPASS)
    {
        printf("key_led_handle_task create failed\r\n");
    }
#endif


    // adc_and_dma_init();

    usart_dma_init();

}

