//
// Created by 文武 on 2026/6/25.
//

#include "spi.h"

extern volatile uint32_t SysTickUpTime;


// CS IO : GPIOA Pin4
void cs_io_init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;   // 输出
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}



void spi1_init(void) {
    cs_io_init();

    // 将从设备的片选引脚设置为高电平
    GPIO_WriteBit(F_CS_GPIO_Port, F_CS_Pin, 1);

    // 使能SPI1时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    /// 使能GPIOA时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;   // 复用功能
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;   // 上拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1); // PA5复用为SPI1
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_SPI1); // PA6复用为SPI1
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_SPI1); // PA7复用为SPI1

    RCC_APB2PeriphResetCmd(RCC_APB2Periph_SPI1, ENABLE);    // 复位SPI1
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_SPI1, DISABLE);   // 释放SPI1

    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; // SPI双向全双工
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256; // 波特率预分频值:256
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge; // 串行时钟进行奇次采样
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low; // 串行时钟空闲状态为低电平
    SPI_InitStructure.SPI_CRCPolynomial = 7; // CRC值计算的多项式
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b; // SPI数据帧8位
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB; // 数据传输从MSB位开始。即高位在前
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master; // SPI主模式，即时钟时序是由主机产生的
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft; // 片选信号由软件控制
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_Cmd(SPI1, ENABLE); // 使能SPI外设

    spi1_read_write_byte(0xff); //启动传输，发送一个0xff，这是为了将 spi 中的垃圾数据清空
}


void spi1_set_speed(uint8_t SPI_BaudRate_Prescaler) {
    // #define assert_param(expr) ((void)0)
    // 判断有效性 assert_param是STM32库函数判断有效性的函数，如果波特率预分频值不在上述的范围内
    // 就会触发断言报错
    assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));

    // 位操作，首先将某一位清零，然后将寄存器清零的相关位写入所需的值
    SPI1->CR1 &= 0xFFC7; // 0xFFC7对应于 1111 1111 1100 0111，清除CR1寄存器中的位[5:3]
    SPI1->CR1 |= SPI_BaudRate_Prescaler; // 设置新的分频值，上一步清零，这一步写入相应的值

    SPI_Cmd(SPI1, ENABLE); // 使能SPI1
}


uint8_t spi1_read_write_byte(uint8_t WriteData) {
    // 等待SPI1发送完成, TXE为状态位用来判断缓存寄存器是否为空
    // TXE为1表示缓存寄存器非空，TXE为1表示缓存寄存器为空，
    // 只要跳出while循环说明缓存寄存器为空，可以写入下一个字节了
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {}
    // 发送1字节数据
    SPI_I2S_SendData(SPI1, WriteData);
    // 等待SPI1接收完成， RXNE用来判断接收缓存区是否为空，与TXE相同
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) {}

    return SPI_I2S_ReceiveData(SPI1); // 返回通过SPI1接收的数据
}

uint8_t spi1_write_byte(uint8_t *WriteData, uint16_t dataSize, uint32_t timeout) {
    uint32_t time = timeout;
    uint32_t current_time = SysTickUpTime;
    uint16_t txSize = dataSize;
    uint16_t rxSize = dataSize;
    uint8_t *pTxBuffPtr = WriteData;
    uint8_t txflow = 1u;

    while((txSize > 0) || (rxSize > 0)) {
        if ((SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) != RESET) &&
            (txSize > 0) && (txflow == 1)) {
            SPI_I2S_SendData(SPI1, *pTxBuffPtr);
            pTxBuffPtr += sizeof(uint8_t);
            txSize--;
            txflow = 0;
        }

        if ((SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) != RESET) &&
            (rxSize > 0) && (txflow == 0)) {
            SPI_I2S_ReceiveData(SPI1);
            rxSize--;
            txflow = 1;
        }

        if ((SysTickUpTime - current_time) >= time) {
            SPI_I2S_ClearFlag(SPI1, SPI_I2S_FLAG_OVR);
            return 0;
        }
    }

    SPI_I2S_ClearFlag(SPI1, SPI_I2S_FLAG_OVR);
    return 1;
}

uint8_t spi1_read_byte(uint8_t *ReadData, uint16_t dataSize, uint32_t timeout) {
    uint32_t time = timeout;
    uint32_t current_time = SysTickUpTime;
    uint16_t txSize = dataSize;
    uint16_t rxSize = dataSize;
    uint8_t *pTxBuffPtr = ReadData;
    uint8_t *pRxBuffPtr = ReadData;
    uint8_t txflow = 1u;

    while ((txSize > 0) || (rxSize > 0)) {
        if (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) != RESET &&
            (txSize > 0) && (txflow == 1)) {
            SPI_I2S_SendData(SPI1, *pTxBuffPtr);
            pTxBuffPtr += sizeof(uint8_t);
            txSize--;
            txflow = 0;
        }

        if (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) != RESET &&
            (rxSize > 0) && (txflow == 0)) {
            *pRxBuffPtr = SPI_I2S_ReceiveData(SPI1);
            pRxBuffPtr += sizeof(uint8_t);
            rxSize--;
            txflow = 1;
        }

        if ((SysTickUpTime - current_time) >= time) {
            SPI_I2S_ClearFlag(SPI1, SPI_I2S_FLAG_OVR); // ✅ 补上 OVR 清除
            return 0;
        }
    }

    return 1;
}

