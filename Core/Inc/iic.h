#ifndef WATCH_IIC_H
#define WATCH_IIC_H

#include "stm32f4xx.h"

/*EEPROM写方向*/
#define EEPROM_I2C_WR  0
/*EEPROM读方向*/
#define EEPROM_I2C_RD  1

#define EEPROM_I2C_GPIO_PORT  GPIOB
#define EEPROM_I2C_GPIO_CLK   RCC_AHB1Periph_GPIOB 
#define EEPROM_I2C_SCL_PIN    GPIO_Pin_8 
#define EEPROM_I2C_SDA_PIN    GPIO_Pin_9
    
/* 标准库的引脚操作宏 */
#define EEPROM_I2C_SCL_1()    GPIO_SetBits(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SCL_PIN)
#define EEPROM_I2C_SCL_0()    GPIO_ResetBits(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SCL_PIN)
#define EEPROM_I2C_SDA_1()    GPIO_SetBits(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN)
#define EEPROM_I2C_SDA_0()    GPIO_ResetBits(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN)

/*读取SDA引脚的电平状态 - 标准库版本*/
#define EEPROM_I2C_SDA_READ()  GPIO_ReadInputDataBit(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN)


/**
 * @brief CPU发起 IIC 总线的起始信号
 * @retval None
 */
void i2c_Start(void);


/**
 * @brief CPU发起 IIC 总线的停止信号
 * @retval None
 */
void i2c_Stop(void);

/**
 * @brief 发送一个字节 8bit 数据
 * @param _ucByte 要发送的字节数据
 * @retval None
 */
void i2c_SendByte(uint8_t _ucByte);

/**
 * @brief 接收一个字节 8bit 数据
 * @retval 接收到的字节数据
 */
uint8_t i2c_ReadByte(void);

/**
 * @brief 等待 IIC 总线响应信号
 * @retval 0: 接收到ACK信号 1: 接收到NACK信号
 */
uint8_t i2c_WaitAck(void);

/**
 * @brief 发送ACK信号
 * @retval None
 */
void i2c_Ack(void);

/**
 * @brief 发送NACK信号
 * @retval None
 */
void i2c_NAck(void);

/**
 * @brief 检查 IIC 总线上的指定设备是否在线
 * @param _Address 设备地址
 * @retval 0: 设备在线 1: 设备不在线
 */
uint8_t i2c_CheckDevice(uint8_t _Address);

#endif //WATCH_IIC_H