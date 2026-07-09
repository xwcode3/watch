//
// Created by 文武 on 2026/6/25.
//

#ifndef WATCH_SPI_H
#define WATCH_SPI_H

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"

#define F_CS_Pin GPIO_Pin_4
#define F_CS_GPIO_Port GPIOA

/**
 * @brief  片选IO初始化, IO: GPIOA Pin4
 * @param  无
 * @retval 无
 */
void cs_io_init(void);


/**
 * @brief  SPI1初始化
 * @param  无
 * @retval 无
 */
void spi1_init(void);


/**
 * @brief  SPI1速度设置函数， SPI速度 = fAPB2 / 分屏系数
 * SPI_BaudRatePrescaler范围： SPI_BaudRatePrescaler_2 ~ SPI_BaudRatePrescaler_256
 * fAPB2时钟一般为84MHz
 * @param  SPI_BaudRatePrescaler: 波特率分频系数
 * 可选参数： SPI_BaudRatePrescaler_4, SPI_BaudRatePrescaler_8,
 *          SPI_BaudRatePrescaler_16, SPI_BaudRatePrescaler_32,
 *          SPI_BaudRatePrescaler_64, SPI_BaudRatePrescaler_128,
 *          SPI_BaudRatePrescaler_256
 * @retval void
 */
void spi1_set_speed(uint8_t SPI_BaudRate_Prescaler);


/**
 * @brief  SPI1读写函数，读写一个字节
 * @param  WriteData : uint8_t 要写入的数据
 * @retval 读到的数据 : uint8_t
 */
uint8_t spi1_read_write_byte(uint8_t WriteData);


/**
 * @brief SPI write function
 * @param WriteData : uint8_t * 要写入的数据
 * @param dataSize :uint16_t 要写入的数据长度
 * @param timeout : uint32_t 超时时间
 * @return uint8_t 0: 成功 1: 失败
 */
uint8_t spi1_write_byte(uint8_t *WriteData, uint16_t dataSize, uint32_t timeout);

/**
 * @brief SPI read function
 * @param ReadData : uint8_t * 读取的数据
 * @param dataSize : uint16_t 读取的数据长度
 * @param timeout : uint32_t 超时时间
 * @return uint8_t 0: 成功 1: 失败
 */
uint8_t spi1_read_byte(uint8_t *ReadData, uint16_t dataSize, uint32_t timeout);
#endif //WATCH_SPI_H
