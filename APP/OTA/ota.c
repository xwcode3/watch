/******************************************************************************
 * Copyright (C) 2024
 * 
 * All Rights Reserved.
 * 
 * @file ota.c
 * 
 * @par dependencies 
 * - ota.h
 * 
 * @author xiaowu
 * 
 * @brief ota task
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

 #include "ota.h"

TaskHandle_t ota_task_handle = NULL;

// 用于接收串口接收数据长度的队列
QueueHandle_t Q_YmodemReclength = NULL;
extern int32_t packet_length;


static uint8_t s_au8_OtaCmd[4];
static E_Ota_State s_e_ota_state = WaitReqDownload;     // 表示 ota状态机的状态，初始为等待服务器请求下载APP数据 WaitReqDownload

// 下载App数据任务相关资源
TaskHandle_t DownloadAppData_taskHandle = NULL;
QueueHandle_t Queue_AppDataBuffer;  // 用于接收App数据的队列
SemaphoreHandle_t Semaphore_ExtFlashState;

uint8_t g_au8_YmodemRec_A[1030];
uint8_t g_au8_YmodemRec_B[1030];


/**
 * @brief download app data task
 * @param void
 * @return void
*/
void DownloadAppData_task(void * pvParameters) {
    uint8_t * pu8_data = NULL;
    uint8_t * pu32_size = NULL;

    // 第一帧数据一定得是数据的长度
    xQueueReceive(Queue_AppDataBuffer, &pu32_size, portMAX_DELAY);
    // 需要擦除掉flash size对应的所有扇区
    // TODO
    xSemaphoreGive(Semaphore_ExtFlashState);
    while (1) {
        xQueueReceive(Queue_AppDataBuffer, &pu8_data, portMAX_DELAY);
        xSemaphoreTake(Semaphore_ExtFlashState, 0);
        if (pu8_data == NULL) {
            continue;
        }
        // 执行写入W25q的逻辑
        // TODO
        W25Q64_WriteData(pu8_data, (uint32_t)packet_length);
        xSemaphoreGive(Semaphore_ExtFlashState);

        vTaskDelay(50);
    }
}


/**
 * @brief scan the key state
 * @param void
 * @return 1: pressed, -1: timeout
*/
static int8_t ota_key_scan(void) {
    uint16_t t_u16_cnt = 0;
    for (; t_u16_cnt < 400; t_u16_cnt++) {
        if (Bit_RESET == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)) {
            vTaskDelay(20);
            if (Bit_RESET == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)) {
                log_i("Key pressed!");
                return 1;   // 判断为按键按下
            }
        }
        vTaskDelay(50);
    }
    log_i("Key scan timeout!");
    return -1;  // 判断为超时未按下
}

/**
 * @brief reset the system
 * @param void
 * @return void
*/
void SoftReset(void) {
    __set_FAULTMASK(1);
    NVIC_SystemReset();
}

/**
 * @brief ota task, receive the ota command and then create download task
 * - cmd : 0x11 0x22 0x33 : there is a new app that can be downloaded;
 * - cmd : 0x44 0x55 0x66 : app downloaded successfully, can reset the system 
 * -                        to update the app now;
 * @param void* pvParameters
 * @return TaskFunction_t ota_task
*/
void ota_task(void* pvParameters) {
    uint8_t t_u8_rec_length = 0;
    uint8_t t_au8_AckCmd[] = {0x44, 0x55, 0x66};
    int32_t t_int32_app_data_length = 0;    // app data length

    // 创建消息通知队列
    Q_YmodemReclength = xQueueCreate(1, sizeof(uint8_t));

    uint8_t t_u8_otastate = 0x00;
    ee_WriteBytes(&t_u8_otastate, 0x00, 1);

    for (;;) {
        log_i("ota state: %d", s_e_ota_state);
        switch (s_e_ota_state) {
            case WaitReqDownload:
                // 阻塞式接收串口命令
                uartx_receive2idle_dma(USART1, s_au8_OtaCmd, 4);
                // 等待接收数据完成
                xQueueReceive(
                    Q_YmodemReclength, &t_u8_rec_length, portMAX_DELAY
                );

                for (int i = 0; i < 3; i++) {
                    log_i("OtaCmd[%d]: %x", i, s_au8_OtaCmd[i]);
                }

                // 校验数据长度
                if (3 == t_u8_rec_length) {
                    // 校验数据内容
                    if ((0x11 == s_au8_OtaCmd[0]) &&
                        (0x22 == s_au8_OtaCmd[1]) &&
                        (0x33 == s_au8_OtaCmd[2])) {
                        // 切换状态
                        s_e_ota_state = OtaDownload;
                        log_i("ota state: OtaDownload = %d", s_e_ota_state);

                        // Queue_AppDataBuffer = xQueueCreate(2, sizeof(uint8_t *));
                        // Semaphore_ExtFlashState = xSemaphoreCreateMutex();

                        // 创建线程以及相关队列
                        // if (pdTRUE != xTaskCreate(
                        //     DownloadAppData_task, "DownloadAppData_task",
                        //     1024, NULL, 4, &DownloadAppData_taskHandle)) {
                        //     // 创建失败
                        //     log_e("Create DownloadAppData_task failed!");
                        //     while (1) {}

                        // }


                        t_u8_otastate = 0x11;
                        ee_WriteBytes(&t_u8_otastate, 0x00, 1);
                    } else {
                        s_au8_OtaCmd[0] = 0;
                        s_au8_OtaCmd[1] = 0;
                        s_au8_OtaCmd[2] = 0;
                        s_au8_OtaCmd[3] = 0;
                    }
                } else {
                    s_au8_OtaCmd[0] = 0;
                    s_au8_OtaCmd[1] = 0;
                    s_au8_OtaCmd[2] = 0;
                    s_au8_OtaCmd[3] = 0;
                }
                break;

            case OtaDownload:
                // 进入下载状态，并校验下载状态
                YmodemUart_EnterRxMode();
                t_int32_app_data_length = Ymodem_Receive_2buffer(g_au8_YmodemRec_A, g_au8_YmodemRec_B);
                YmodemUart_RestoreDmaMode(s_au8_OtaCmd, 4);

                if (0 < t_int32_app_data_length) {
                    s_e_ota_state = WaitReqUpdate;
                    log_i("ota state: WaitReqUpdate = %d", s_e_ota_state);

                    usart1_transmit(t_au8_AckCmd, 3);

                    // 清除一切生成的资源
                    // xSemaphoreTake(Semaphore_ExtFlashState, portMAX_DELAY);
                    // xSemaphoreGive(Semaphore_ExtFlashState);

                    // // vTaskDelete(DownloadAppData_taskHandle);
                    // vQueueDelete(Queue_AppDataBuffer);
                    // vSemaphoreDelete(Semaphore_ExtFlashState);

                    t_u8_otastate = 0x22;
                    ee_WriteBytes(&t_u8_otastate, 0x00, 1);

                    // TEST
                    uint8_t t_u8_readstate = 0;
                    ee_ReadBytes(&t_u8_readstate, 0x00, 1);
                    log_i("Read state: %d", t_u8_readstate);

                    // 写入数据长度
                    ee_WriteBytes((uint8_t *)&t_int32_app_data_length, 0x01, sizeof(t_int32_app_data_length));

                    // TEST
                    int8_t t_au8_readLength[4] = {0};
                    ee_ReadBytes(t_au8_readLength, 0x01, 4);
                    // usart1_transmit(t_au8_readLength, 4);
                    for (int i = 0; i < 4; i++) {
                        log_i("Read Length: %x", t_au8_readLength[i]);
                    }

                } else {
                    s_e_ota_state = WaitReqDownload;
                    log_i("ota state: WaitReqDownload = %d", s_e_ota_state);
                    // 下载失败，清除所有生成的资源
                    // vTaskDelete(DownloadAppData_taskHandle);
                    // vQueueDelete(Queue_AppDataBuffer);
                    // vSemaphoreDelete(Semaphore_ExtFlashState);
                }
                break;

            case WaitReqUpdate:
                // 阻塞式接收串口命令
                uartx_receive2idle_dma(USART1, s_au8_OtaCmd, 4);
                // 等待接收数据完成
                xQueueReceive(Q_YmodemReclength, &t_u8_rec_length, portMAX_DELAY);
                
                for (int i = 0; i < 3; i++) {
                    log_i("OtaCmd[%d]: %x", i, s_au8_OtaCmd[i]);
                }
                
                // 校验数据长度
                if (3 == t_u8_rec_length) {
                    // 校验数据内容
                    if ((0x55 == s_au8_OtaCmd[0]) &&
                        (0x66 == s_au8_OtaCmd[1]) &&
                        (0x77 == s_au8_OtaCmd[2])) {
                        // 切换状态
                        s_e_ota_state = OtaEnd;
                        log_i("ota state: OtaEnd = %d", s_e_ota_state);
                    } else {
                        s_au8_OtaCmd[0] = 0;
                        s_au8_OtaCmd[1] = 0;
                        s_au8_OtaCmd[2] = 0;
                        s_au8_OtaCmd[3] = 0;
                    }
                } else {
                    s_au8_OtaCmd[0] = 0;
                    s_au8_OtaCmd[1] = 0;
                    s_au8_OtaCmd[2] = 0;
                    s_au8_OtaCmd[3] = 0;
                }
                break;

            case OtaEnd:
                // 判断按键，20s内
                if(1 == ota_key_scan()) {
                    // 执行软复位
                    SoftReset();
                } else {
                    // 无限期阻塞
                    s_e_ota_state = WaitReqDownload;
                    log_i("ota state: WaitReqDownload = %d", s_e_ota_state);
                }
                break;
            default:
                // no action
        }
    }
}

