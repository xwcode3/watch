//
// Created by 文武 on 2026/3/22.
//
#include "usart.h"

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
