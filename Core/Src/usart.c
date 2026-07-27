//
// Created by 文武 on 2026/3/22.
//
#include "usart.h"

uint32_t dma2_init_ndtr = 0;    // DMA2_Stream2的初始化缓冲区大小

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

    if (DMA_GetITStatus(DMA2_Stream2, DMA_IT_TCIF2)) {
        // DMA传输完成处理
        // 这里可以添加传输完成后的处理逻辑
        
        // 清除中断标志
        DMA_ClearITPendingBit(DMA2_Stream2, DMA_IT_TCIF2);
    }

#ifdef USART1_DMA_RING
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
#endif

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

/**
 * @brief  用于 OTA 任务的 USART1 DMA配置函数
 * @param  void
 * @retval void
 */
void ota_usart1_dma_config(void) {
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    DMA_InitTypeDef dma_init;
    dma_init.DMA_Channel = DMA_Channel_4;
    dma_init.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);
    // dma_init.DMA_Memory0BaseAddr = (uint32_t)(RxBuffer);
    dma_init.DMA_DIR = DMA_DIR_PeripheralToMemory;  // 外设到内存
    // dma_init.DMA_BufferSize = BUFFER_SIZE; // 缓冲区大小
    dma_init.DMA_PeripheralInc = DMA_PeripheralInc_Disable; // 外设地址不递增
    dma_init.DMA_MemoryInc = DMA_MemoryInc_Enable; // 内存地址递增
    dma_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设数据宽度：字节
    dma_init.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte; // 内存数据宽度：字节
    dma_init.DMA_Mode = DMA_Mode_Normal; // 普通模式
    dma_init.DMA_Priority = DMA_Priority_High; // 高优先级
    dma_init.DMA_FIFOMode = DMA_FIFOMode_Disable; // 关闭FIFO
    dma_init.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    dma_init.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    dma_init.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

    DMA_Init(DMA2_Stream2, &dma_init); //usart1_rx 使用DMA2_Stream2

    DMA_Cmd(DMA2_Stream2, ENABLE);
}


/**
 * @brief  用于 ota 任务的 USART1 配置函数
 * @param  void
 * @retval void
 */
void ota_usart1_config(void) {
    // 先完全禁用并复位USART1
    USART_Cmd(USART1, DISABLE);
    
    // 复位USART1外设
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_USART1, DISABLE);

    // enable USART1 and GPIOA clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);  // enable USART1 clock
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);   // enable GPIOA clock

    // configure GPIO, USART1 TX pin : PA9 and RX pin : PA10
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置GPIO复用功能
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);  // PA9 - USART1_TX
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1); // PA10 - USART1_RX

    // configure usart1
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    // configure usart1 idle line detection interrupt
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

    // configure NVIC for usart1
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE);

    // enable USART1 DMA receive
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    ota_usart1_dma_config();    // 配置DMA2_Stream2用于USART1接收数据
}


extern QueueHandle_t Q_YmodemReclength; // 用于传递串口接收数据长度的消息队列
// USART1接收数据中断服务程序
// USART1 interrupt service program
void USART1_IRQHandler(void) {

#if 1   /* ota 串口接收来自服务器的指令 */
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        char ch = USART_ReceiveData(USART1);
        USART_SendData(USART1, ch);
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }

    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET) {
        // clear IDLE flag by reading SR and DR
        // 清除IDLE标志 - 通过读SR和DR寄存器
        volatile uint32_t temp;
        temp = USART1->SR;  // 读状态寄存器
        temp = USART1->DR;  // 读数据寄存器
        (void)temp;         // 避免编译器警告

        uint16_t rec_len = dma2_init_ndtr - DMA_GetCurrDataCounter(DMA2_Stream2);

        // log_i("USART1 idle interrupt, dma2_init = %d, received length: %d", dma2_init_ndtr, rec_len);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;  
        if (pdFALSE != xQueueSendFromISR(Q_YmodemReclength, &rec_len, 0)) {
            // 成功发送消息到队列
            xHigherPriorityTaskWoken = pdTRUE;
        }

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
#endif

#if 0 /* 串口、dma 配合环形缓冲区模式 */
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
#endif 
}

/**
 * @brief  串口发送函数，使用USART1，使用阻塞方式
 * @param  buffer: 要发送的数据缓冲区
 * @param  length: 要发送的数据长度
 * @retval void
 */
void usart1_transmit(uint8_t *buffer, uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        // 等待发送缓冲区为空
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        // 发送字符
        USART_SendData(USART1, buffer[i]);
    }
}


/**
  * @brief  串口发送字节函数，使用USART2或USART1，使用阻塞方式
  * @param  USARTx USART2, USART1
  * @param  data 数据字节
  * @retval None
  */
void USART_SendChar(USART_TypeDef* USARTx, uint8_t data)
{
    // 等待发送数据寄存器为空
    while(USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET) {}
    USART_SendData(USARTx, data);
}


/**
  * @brief  串口接收字节函数，使用USART2或USART1，使用阻塞方式
  * @param  USARTx USART2, USART1
  * @retval 返回数据
  */
uint8_t USART_ReceiveChar(USART_TypeDef* USARTx) {
    // 等待接收数据寄存器满
    while (USART_GetFlagStatus(USARTx, USART_FLAG_RXNE) == RESET);
    // 读取接收到的数据，这里读取的数据是16位的，但我们只需要低8位，所以直接返回即可
    return USART_ReceiveData(USARTx);
}





/**
 * @brief  此函数用于重启dma接收，要求对应的usart空闲中断已使能，且已经配置好DMA接收参数
 * @param  usartx: USART外设
 * @param  buffer: 接收缓冲区
 * @param  length: 缓冲区长度
 * @retval void
 */
void uartx_receive2idle_dma( USART_TypeDef* usartx, uint8_t* buffer, uint16_t length) {
    if (usartx == USART1) {
        // 1. 先禁用USART的DMA请求（防止新数据触发DMA）
        USART_DMACmd(USART1, USART_DMAReq_Rx, DISABLE);
        
        // 2. 快速禁用DMA（不等待）
        DMA2_Stream2->CR &= ~DMA_SxCR_EN;
        
        // 3. 快速配置
        DMA2_Stream2->M0AR = (uint32_t)buffer;
        DMA2_Stream2->NDTR = length;
        dma2_init_ndtr = length;
        
        // 4. 清除标志
        DMA2->LIFCR = DMA_FLAG_TCIF2 | DMA_FLAG_HTIF2 | DMA_FLAG_TEIF2 | DMA_FLAG_DMEIF2 | DMA_FLAG_FEIF2;
        
        // 5. 重新启用
        DMA2_Stream2->CR |= DMA_SxCR_EN;
        USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    }
}


/**
 * @brief  进入 Ymodem 接收模式：关闭 USART1 的 DMA 转运，改为轮询 DR 接收
 * @note   在调用 Ymodem_Receive_2Buffer() 之前调用。
 *         ymodem 协议栈通过 SerialKeyPressed() 直接轮询 USART1->DR，
 *         不需要 DMA，也不需要空闲线中断，因此这里把 DMA 接收和空闲线中断都关掉，
 *         避免 DMA 与轮询抢字节、以及空闲线误触发命令队列。
 */
void YmodemUart_EnterRxMode(void)
{
    /* 1. 关闭 USART -> DMA 的接收请求，停止 DMA 搬运 */
    USART_DMACmd(USART1, USART_DMAReq_Rx, DISABLE);

    /* 2. 关闭 DMA2_Stream2（Normal 模式下一收完就停，这里确保彻底关掉） */
    DMA_Cmd(DMA2_Stream2, DISABLE);
    while (DMA_GetCmdStatus(DMA2_Stream2) != DISABLE) { }

    /* 3. 关闭空闲线中断（ymodem 用轮询，不需要；也避免误触发 Q_YmodemReclength） */
    USART_ITConfig(USART1, USART_IT_IDLE, DISABLE);

    /* 4. 丢弃可能残留的字节，清除 ORE / IDLE / RXNE 标志，防止 ymodem 读到旧数据 */
    (void)USART1->SR;
    (void)USART1->DR;
    USART_ClearFlag(USART1, USART_FLAG_ORE | USART_FLAG_IDLE | USART_FLAG_RXNE);

    /* 5. 确保接收器使能（Rx 必须开着，ymodem 才能收到字节） */
    USART_Cmd(USART1, ENABLE);
}


/**
 * @brief  恢复为当前的 DMA 转运模式（OTA 命令接收）
 * @param  cmd_buf : 恢复后用于接收 OTA 命令的缓冲区（如 s_au8_OtaCmd）
 * @param  cmd_len : 命令缓冲区长度
 * @note   在 Ymodem_Receive_2Buffer() 返回之后调用，
 *         把串口交还给 OTA 命令等待状态（一次性 DMA + 空闲线通知）。
 */
void YmodemUart_RestoreDmaMode(uint8_t* cmd_buf, uint16_t cmd_len)
{
    /* 1. 复位并重新初始化 DMA2_Stream2 为 Normal（一次性）模式 */
    ota_usart1_dma_config();

    /* 2. 重新打开 USART -> DMA 接收请求 */
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    /* 3. 重新打开空闲线中断（命令帧结束 -> 通知 Q_YmodemReclength） */
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

    /* 4. 武装 DMA：把下一帧 OTA 命令收进 cmd_buf */
    // uartx_receive2idle_dma(USART1, cmd_buf, cmd_len);
}
