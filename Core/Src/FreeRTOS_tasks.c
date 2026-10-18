//
// Created by 文武 on 2026/4/22.
//

#include "FreeRTOS_tasks.h"
#include "bsp_aht21_driver.h"
#include <math.h>

#define MPU6050_TEST

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


led_status_t led_on_myown  (void)
{
    printf("led is on\r\n");
    GPIO_ResetBits(GPIOC, GPIO_Pin_13);
    return LED_OK;
}
led_status_t led_off_myown  (void)
{
    printf("led is off\r\n");
    GPIO_SetBits(GPIOC, GPIO_Pin_13);
    return LED_OK;
}

led_operations_t led_operations_myown = {
    .pf_led_on  = led_on_myown,
    .pf_led_off = led_off_myown
};

void Test_3()
{
//系统集成工程师：
//******************************** Driver Handler ***************************//
    printf("System Starting.....\r\n");
    led_handler_status_t ret = HANDLER_OK;   
    bsp_led_handler_t handler_1;         
    ret = led_handler_inst ( &handler_1,            //跑在线程3上，Core1
                            &osal_delay_freertos,
                            &osal_queue_freertos,
                            &osal_critical_freertos,
                            &osal_thread_freertos,
                            //tbd &
                            &osal_time_base_freertos
                            );
    led_status_t ret1 = LED_OK;
    bsp_led_driver_t led1;
    ret1 = led_driver_inst( &led1,
                            &led_operations_myown,
                            &osal_delay_freertos,
                            &osal_time_base_freertos
                            ); 
    if (HANDLER_OK == ret )
    {
        printf("handler_1 has been instantiated \r\n");
    }
    // 到这一步时，系统基础的LED handler1 服务线程 和 LED的对象已经构造完毕

//APP工程师：
//******************************** Driver Handler ***************************//

    
    //APP1 线程1
    led_index_t handler_1_led_index_1 = LED_NOT_INITIALIZED;
    ret = handler_1.pf_led_register( &handler_1,    //跑在线程1上，Core1
                                          &led1,
                         &handler_1_led_index_1);
    
    if (HANDLER_OK == ret )
    {
        printf("leds have been registered \r\n");
    }

    //APP1 线程2
    ret = handler_1.pf_led_countroler(&handler_1,
                                100U,
                                1U,
                                PROPORTIONN_1_2,
                                handler_1_led_index_1);
    if (HANDLER_OK == ret )
    {
        printf("led1 has been controlled\r\n");
    }

    // 到这一步时，APP完成调用
    //UI
    while(1);
}

TaskHandle_t idle_task_handle;
void idle_task(void *pvParameters)
{
    Test_3();

    while(1)
    {
        vTaskDelay(2000);
        printf("idle task is running\r\n");
    }
}



#if 1   // temp_humi_thread

iic_bus_t iic_bus_aht21 = {
    .IIC_SCL_PIN = GPIO_Pin_14,
    .IIC_SCL_PORT = GPIOB,
    .IIC_SDA_PIN = GPIO_Pin_13,
    .IIC_SDA_PORT = GPIOB,
};


iic_driver_interface_t iic_driver_interface = {
    .iic_bus = &iic_bus_aht21,
    .pf_iic_init = IICInit,
    .pf_iic_deinit = IICDeinit,
    .pf_iic_start = IICStart,
    .pf_iic_stop = IICStop,
    .pf_iic_send_byte = IICSendByte,
    .pf_iic_receive_byte = IICReceiveByte,
    .pf_iic_wait_ack = IICWaitAck,
    .pf_iic_send_ack = IICSendAck,
    .pf_iic_send_no_ack = IICSendNotAck,
    .pf_critical_interface = &osal_critical_freertos,
};
temp_humi_handler_all_input_arg_t temp_humi_input_arg ={
    .iic_driver_interface = &iic_driver_interface,
    .timebase_interface = &osal_time_base_freertos,
    .os_queue_interface = &osal_queue_freertos,
    .os_delay_interface = &osal_delay_freertos,
    .pf_callback = NULL,   // 这里可以设置一个回调函数，用于处理温湿度数据
};

bsp_aht21_driver_t aht21_ins = {
    .p_iic_driver_instance = &iic_driver_interface,    
};



#endif 


extern QueueHandle_t temp_humi_queue;
void send_temp_humi_event(void *pvParameters)
{
    temp_humi_event_t event = {
        // .humidity
        .lifetime = 1000,
        // .temperature
        // .timestamp
        .type = TEMP_HUMI_EVENT_BOTH,
        .pf_callback = NULL,
    };

    vTaskDelay(1000); // 等待队列创建完成

    while (1)
    {

        bsp_temp_humi_read(&event);
        // aht21_read_data(&aht21_ins, event.temperature, event.humidity);

        // printf("temp: %f, humi: %f\r\n", event.temperature, event.humidity);

        // while (1);

        vTaskDelay(1000); // 每秒发送一次事件
    }
}

extern TaskHandle_t led_TaskHandle;


#ifdef MPU6050_TEST
extern bsp_mpuxxx_handler_t handler_instance;

/* ================= MPU6050 姿态解算（互补滤波，仅本文件使用） ================= */
#define MPU_ACC_LSB_PER_G       16384.0f    /* ±2g     -> 16384 LSB/g   */
#define MPU_GYRO_LSB_PER_DPS    16.4f       /* ±2000dps -> 16.4 LSB/dps */
#define ATT_TAU_S               0.5f        /* 互补滤波时间常数, 越大越信陀螺 */
#define ATT_GYRO_DEADBAND_DPS   0.20f       /* 静止死区, 抑制积分抖动 */
#define ATT_ACC_TOL_G           0.25f       /* |a| 偏离 1g 超此值判为有线性加速度 */
#define ATT_CALIB_FRAMES        500U        /* 上电静止标定帧数, 50Hz 约 10s */
#define ATT_RAD_TO_DEG          57.29577951f

typedef struct {
    float    roll;              /* deg, ±180 */
    float    pitch;             /* deg, ±90  */
    float    yaw;               /* deg, 无绝对基准, 会漂移 */
    float    gyro_bias[3];      /* dps */
    float    bias_acc[3];
    uint16_t bias_cnt;
    uint8_t  bias_ready;
} att_ctx_t;

static float att_wrap180(float a)
{
    while (a >  180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

/* 上电静止累计陀螺零偏, 累计够帧数后置位 bias_ready */
static void att_calib_gyro(att_ctx_t *p, const float gyro[3])
{
    if (p->bias_ready) return;

    for (uint8_t i = 0; i < 3; i++) {
        p->bias_acc[i] += gyro[i];
    }

    if (++p->bias_cnt >= ATT_CALIB_FRAMES) {
        for (uint8_t i = 0; i < 3; i++) {
            p->gyro_bias[i] = p->bias_acc[i] / (float)ATT_CALIB_FRAMES;
        }
        p->bias_ready = 1;
    }
}

/* 一阶互补滤波: 陀螺积分做预测, 加速度计(重力矢量)做校正 */
static void att_update(att_ctx_t *p, const float acc[3], const float gyro[3], float dt)
{
    float w[3];

    for (uint8_t i = 0; i < 3; i++) {
        w[i] = gyro[i] - p->gyro_bias[i];
        if (fabsf(w[i]) < ATT_GYRO_DEADBAND_DPS) w[i] = 0.0f;
    }

    float roll  = p->roll  + w[0] * dt;
    float pitch = p->pitch + w[1] * dt;
    float yaw   = p->yaw   + w[2] * dt;

    float n = sqrtf(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
    if (fabsf(n - 1.0f) <= ATT_ACC_TOL_G) {
        float a_roll  = atan2f(acc[1], acc[2]) * ATT_RAD_TO_DEG;
        float a_pitch = atan2f(-acc[0], sqrtf(acc[1] * acc[1] + acc[2] * acc[2])) * ATT_RAD_TO_DEG;
        float k = ATT_TAU_S / (ATT_TAU_S + dt);     /* 50Hz 时约 0.96 */

        roll  += (1.0f - k) * att_wrap180(a_roll  - roll);
        pitch += (1.0f - k) * att_wrap180(a_pitch - pitch);
    }

    p->roll  = att_wrap180(roll);
    p->pitch = (pitch >  89.0f) ?  89.0f :
               (pitch < -89.0f) ? -89.0f : pitch;
    p->yaw   = att_wrap180(yaw);
}
/* ============================================================================ */


void unpack_task(void *pvParameters) {
    static att_ctx_t att;
    float    acc[3], gyro[3];
    uint32_t tick_now = 0, tick_last = 0;
    float    dt;

    log_i("unpack task start");

    for (;;) {
        if (NULL == handler_instance.p_unpack_queue_handle) {
            vTaskDelay(1);
            continue;
        }

        uint8_t data = 0;
        if (handler_instance.p_input_args->p_queue->queue_get(
                handler_instance.p_unpack_queue_handle, &data, portMAX_DELAY) != 0) {
            continue;
        }

        uint8_t *addr = circular_buf.pf_get_rbuffer_addr(&circular_buf);

        /* 1. int16 大端 -> 物理量 */
        acc[0] = (int16_t)((uint16_t)addr[0] << 8 | addr[1]) / MPU_ACC_LSB_PER_G;
        acc[1] = (int16_t)((uint16_t)addr[2] << 8 | addr[3]) / MPU_ACC_LSB_PER_G;
        acc[2] = (int16_t)((uint16_t)addr[4] << 8 | addr[5]) / MPU_ACC_LSB_PER_G;

        gyro[0] = (int16_t)((uint16_t)addr[8]  << 8 | addr[9])  / MPU_GYRO_LSB_PER_DPS;
        gyro[1] = (int16_t)((uint16_t)addr[10] << 8 | addr[11]) / MPU_GYRO_LSB_PER_DPS;
        gyro[2] = (int16_t)((uint16_t)addr[12] << 8 | addr[13]) / MPU_GYRO_LSB_PER_DPS;

        circular_buf.pf_data_readed(&circular_buf);

        /* 2. dt 用 1ms 时基实测, 不要写死 0.02 */
        osal_time_base_freertos.get_tick_ms(&tick_now);
        dt = (tick_now - tick_last) / 1000.0f;
        tick_last = tick_now;
        if (dt <= 0.0f || dt > 0.2f) dt = 0.02f;

        /* 3. 上电静止标定期间只采不发 */
        if (!att.bias_ready) {
            att_calib_gyro(&att, gyro);
            continue;
        }

        /* 4. 姿态解算 */
        att_update(&att, acc, gyro, dt);

        /* 5. VOFA+ FireWater: 纯数字 + 逗号分隔 + 换行结尾
         *    必须用 printf, 不能用 log_i(elog 会加前缀与颜色码, 破坏帧)
         *    通道: I0=roll, I1=pitch, I2=yaw  (单位: 度) */
        printf("%.2f,%.2f,%.2f\r\n", att.roll, att.pitch, att.yaw);
    }
}

I2C_InitTypeDef iic2 = {
    .I2C_ClockSpeed = 100000,                                   /* 100kHz */
    .I2C_DutyCycle = I2C_DutyCycle_2,                           /* 50% duty cycle */
    .I2C_OwnAddress1 = 0x00,
    .I2C_Ack = I2C_Ack_Enable,                                  /* 接收时自动回复 Ack */
    .I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit,    /* 7 位地址 */
    .I2C_Mode = I2C_Mode_I2C,                                   /* I2C 模式 */
};


mpu6050_iic_driver_interface_t mpu6050_iic_driver_interface = {
    .p_iic = &iic2,
    .pf_iic_init = hardware_iic_init,
    .pf_iic_deinit = hardware_iic_deinit,
    .pf_iic_mem_read = iic_mem_read,
    .pf_iic_mem_write = iic_mem_write,
    .pf_iic_mem_read_dma = iic_mem_read_dma
};

mpuxxx_handler_input_args_t mpu6050_input_arg = {
    .p_iic_driver = &mpu6050_iic_driver_interface,
    .p_time_base = &osal_time_base_freertos,
    .p_loop_delay = &osal_loop_delay_freertos,
    .p_queue = &osal_queue_freertos,
    .p_semaphore = &osal_sem_freertos,
    .p_notify = &osal_notify_freertos,
    .p_mutex = &osal_mutex_freertos,
    .p_thread = &osal_thread_freertos,
    .p_critical = &osal_critical_freertos,
    .p_delay = &osal_delay_freertos
};


/* External variables */
extern void (*pf_pin_interrupt_callback)(void *, void *);
/* stm32f4xx_it.c */
void EXTI9_5_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line5) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line5);      /* 1) 清 EXTI 挂起位，否则反复进中断 */

        if (pf_pin_interrupt_callback != NULL)   /* 3) 通知上层 */
            pf_pin_interrupt_callback(handler_instance.p_driver, NULL);
    }
}
#endif /* End of MPU6050_TEST */


void app_task_create(void)
{
    // adc_and_dma_init();

    BaseType_t task_create_ret = pdFAIL;
    // task_create_ret = xTaskCreate(led_blink_task, "led_blink_task", 128, NULL, 1, &led_TaskHandle);
    // if (task_create_ret != pdPASS)
    // {
    //     printf("led_blink_task create failed\r\n");
    // }

    // extern TaskHandle_t ota_task_handle;
    // task_create_ret = xTaskCreate(ota_task, "ota_task", 256, NULL, 2, &ota_task_handle);
    // if (task_create_ret != pdPASS)
    // {
    //     printf("ota_task create failed\r\n");
    // }

    // task_create_ret = xTaskCreate(idle_task, "idle_task", 256, NULL, 1, &idle_task_handle);
    // if (task_create_ret != pdPASS)
    // {
    //     printf("idle_task create failed\r\n");
    // }

    // task_create_ret = xTaskCreate(temp_humi_handler_thread, "temp_humi_task", 256, &temp_humi_input_arg, 3, NULL);
    // if (task_create_ret != pdPASS) {
    //     printf("temp_humi_task create failed\r\n");
    // }

    // task_create_ret = xTaskCreate(send_temp_humi_event, "send_task", 256, NULL, 1, NULL);
    // if (task_create_ret != pdPASS) {
    //     printf("send_task create failed\r\n");
    // }

    task_create_ret = xTaskCreate(mpuxxx_handler_thread, "handler_thread", 1024, &mpu6050_input_arg, 3, NULL);
    if (task_create_ret != pdPASS) {
        printf("handler_thread create failed\r\n");
    }

    task_create_ret = xTaskCreate(unpack_task, "unpack_task", 1024, NULL, 1, NULL);
    if (task_create_ret != pdPASS) {
        printf("unpack_task create failed\r\n");
    }

    // usart_dma_init();

}




