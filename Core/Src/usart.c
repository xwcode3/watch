//
// Created by 文武 on 2026/3/22.
//
#include "usart.h"

/* DMA半满、全满以及USART空闲中断标志 */
typedef enum {
    UART_EVENT_DMA_HALF_COMPLETE = 0,   // DMA 半缓冲处理
    UART_EVENT_DMA_COMPLETE,    // DMA 全满事件
    UART_EVENT_IDLE_LINE,   // 空闲线事件
} UART_Event;

/* UART状态结构体 */
typedef struct {
    USART_TypeDef* usart;       // USART外设
    uint8_t* dma_buf;           // DMA 接收缓冲区
    uint16_t buf_size;          // 缓冲区大小
    uint8_t dma_full_flag;      // DMA传输完成标志
} UART_DMA_State_t;

/**
 * @brief  UART DMA状态结构体
 */
static UART_DMA_State_t usart1_state = {
    .usart = USART1,
    .dma_buf = NULL,
    .buf_size = 0,
    .dma_full_flag = 0,
};

// 串口的注册回调函数
void UART_RegisterRxEventCallback(USART_TypeDef* usart,
    uint8_t* buf, uint16_t size) {
    if (usart == USART1) {
        // 支持多串口可扩展
        usart1_state.dma_buf = buf;
        usart1_state.buf_size = size;
    }
}


void USART1_DMA2_Stream2_Init(uint8_t* buffer_address, uint32_t buffer_size) {

    // 在调用 DMA 初始化前，先关闭之前的串口配置
    USART_Cmd(USART1, DISABLE);
    USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
    USART_DeInit(USART1);

    UART_RegisterRxEventCallback(USART1, buffer_address, buffer_size);

    // 1. 使能时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    // 2. 配置GPIOA9/10为复用
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &gpio);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    // 3. 配置USART1
    USART_InitTypeDef usart;
    usart.USART_BaudRate = 115200;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &usart);

    // 4. 配置DMA2_Stream2, Channel4, 外设到内存
    DMA_DeInit(DMA2_Stream2);
    while (DMA_GetCmdStatus(DMA2_Stream2) != DISABLE);

    DMA_InitTypeDef dma;
    dma.DMA_Channel = DMA_Channel_4;
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    dma.DMA_Memory0BaseAddr = (uint32_t)usart1_state.dma_buf; // 使用全局状态中的缓冲区
    // dma.DMA_Memory0BaseAddr = (uint32_t)usart1_rx_buf;
    dma.DMA_DIR = DMA_DIR_PeripheralToMemory;
    dma.DMA_BufferSize = usart1_state.buf_size; // 使用全局状态中的缓冲区大小
    // dma.DMA_BufferSize = USART1_RX_BUF_SIZE;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 注意：DMA配置为Byte宽度
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte; // 注意：DMA配置为Byte宽度
    dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_Medium;
    dma.DMA_FIFOMode = DMA_FIFOMode_Disable;
    dma.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    dma.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    dma.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream2, &dma);

    // 5. 使能DMA中断（半传输/传输完成）
    DMA_ITConfig(DMA2_Stream2, DMA_IT_HT | DMA_IT_TC, ENABLE);

    // 6. 使能USART1的DMA接收
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    // 7. 使能USART1
    USART_Cmd(USART1, ENABLE);

    // 8. 使能DMA2_Stream2
    DMA_Cmd(DMA2_Stream2, ENABLE);

    // 9. 配置NVIC
    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel = DMA2_Stream2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 5;  // 注意！！！优先级要比串口空闲中断高
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    // 可选：使能USART1 IDLE空闲中断用于帧结束检测
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 6;  // 注意！！！优先级不能比串口半、全缓冲中断高
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

uint8_t dma_flag = 0; // DMA传输完成标志
void DMA2_Stream2_IRQHandler(void) {
    // 半缓冲处理
    if (DMA_GetITStatus(DMA2_Stream2, DMA_IT_HTIF2) != RESET) {
        DMA_ClearITPendingBit(DMA2_Stream2, DMA_IT_HTIF2);
        // 半缓冲处理
        dma_flag = 2; // 设置标志位，表示半传输完成
        log_i("DMA half transfer complete");

        dma_half_irq_callback(usart1_state.buf_size / 2);
    }

    // 全缓冲处理
    if (DMA_GetITStatus(DMA2_Stream2, DMA_IT_TCIF2) != RESET) {
        DMA_ClearITPendingBit(DMA2_Stream2, DMA_IT_TCIF2);

        // 全缓冲处理
        dma_flag = 1;
        log_i("DMA whole transfer complete");

        dma_comp_irq_callback(usart1_state.buf_size);

        usart1_state.dma_full_flag = 1; // 设置DMA全满传输完成标志
    }
}

/*
* @brief 发送单个字节
* @param data: 要发送的数据
* @retval 无
*/
void USART1_SendByte(uint8_t data)
{
    // 等待发送数据寄存器为空
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {}
    USART_SendData(USART1, data);
}

/**
 * @brief USART1 串口配置函数
 * @param 无
 * @retval 无
 */
void USART1_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. 使能USART1和GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    // 2. 配置USART1引脚复用功能
    // PA9 - TX, PA10 - RX
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    // 3. 配置GPIO引脚模式
    // TX引脚配置为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // RX引脚配置为复用浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 配置USART参数
    USART_InitStructure.USART_BaudRate = 115200;                    // 波特率
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;     // 8位数据位
    USART_InitStructure.USART_StopBits = USART_StopBits_1;          // 1位停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;             // 无校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 收发模式
    USART_Init(USART1, &USART_InitStructure);

    // 5. 配置中断优先级和使能
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;       // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;              // 响应优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 6. 使能接收中断
    // USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    // 7. 使能USART1
    USART_Cmd(USART1, ENABLE);
}

// USART1接收数据中断服务程序
// USART1 interrupt service program
void USART1_IRQHandler(void) {

    uint16_t data_len = 0;

    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET) {
        // 清除IDLE标志的关键步骤：
        (void) USART1->SR;
        (void) USART1->DR;

        log_i("usart idle");

        data_len = usart1_state.buf_size - DMA_GetCurrDataCounter(DMA2_Stream2);

        // 核心逻辑：DMA 全满后若是没有新的数据，空闲中断不触发回调函数
        if (usart1_state.dma_full_flag && data_len == 0) {
            usart1_state.dma_full_flag = 0; // 清除DMA全满标志
        } else {
            uart_idle_irq_callback(data_len);
            // usart1_state.dma_full_flag = 0; // 清除DMA全满标志
        }
    }
}


#if 0

// USART1接收数据中断服务程序
// USART1 interrupt service program
void USART1_IRQHandler(void) {

    uint16_t data_len;

    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET) {
        // 清除IDLE标志的关键步骤：
        (void) USART1->SR;
        (void) USART1->DR;

        log_i("usart idle");

        data_len = usart1_state.buf_size - DMA_GetCurrDataCounter(DMA2_Stream2);

        // 核心逻辑：DMA 全满后若是没有新的数据，空闲中断不触发回调函数
        if (usart1_state.dma_full_flag && data_len == 0) {
            usart1_state.dma_full_flag = 0; // 清除DMA全满标志
        } else {
            uart_idle_irq_callback(data_len);
        }
    }
}

/**
 * @brief USART1 串口配置函数
 * @param 无
 * @retval 无
 */
void USART1_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. 使能USART1和GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    // 2. 配置USART1引脚复用功能
    // PA9 - TX, PA10 - RX
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    // 3. 配置GPIO引脚模式
    // TX引脚配置为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // RX引脚配置为复用浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 配置USART参数
    USART_InitStructure.USART_BaudRate = 115200;                    // 波特率
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;     // 8位数据位
    USART_InitStructure.USART_StopBits = USART_StopBits_1;          // 1位停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;             // 无校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 收发模式
    USART_Init(USART1, &USART_InitStructure);

    // 5. 配置中断优先级和使能
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;       // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;              // 响应优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 6. 使能接收中断
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    // 7. 使能USART1
    USART_Cmd(USART1, ENABLE);
}

/**
 * @brief 发送单个字节
 * @param data: 要发送的数据
 * @retval 无
 */
void USART1_SendByte(uint8_t data)
{
    // 等待发送数据寄存器为空
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, data);
}

/**
 * @brief 发送字符串
 * @param str: 字符串指针
 * @retval 无
 */
void USART1_SendString(char* str)
{
    while(*str)
    {
        USART1_SendByte(*str++);
    }
}

/**
 * @brief USART1中断处理函数
 * @param 无
 * @retval 无
 */
void USART1_IRQHandler(void)
{
    uint8_t data;

    // 检查接收中断标志
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        // 读取接收到的数据
        data = USART_ReceiveData(USART1);

        // 将接收到的数据立即发送回去
        USART1_SendByte(data);

        // 清除中断标志
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}
#endif

