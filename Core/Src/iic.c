#include "iic.h"

/**
 * @brief 配置IIC总线的GPIO引脚
 * @note  SCL和SDA引脚配置为开漏输出模式, 无上下拉, 高速
 * @retval None
 */
static void i2c_cfgGpio(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能GPIOB时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    /* 配置SCL和SDA引脚 */
    GPIO_InitStructure.GPIO_Pin = EEPROM_I2C_SCL_PIN | EEPROM_I2C_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;      // 输出模式
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;     // 开漏输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;   // 无上下拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; // 高速
    GPIO_Init(EEPROM_I2C_GPIO_PORT, &GPIO_InitStructure);
    
    /* 给一个停止信号, 复位I2C总线上的所有设备到待机模式 */
    i2c_Stop();
}



/**
 * @brief IIC总线位延时函数, 168M主频下适配400kHz
 * @note  该函数用于产生IIC总线的时钟信号, 通过循环延时的方式实现, 需要根据实际主频进行调整
 * @retval None
 */
static void i2c_Delay(void)
{
    uint8_t i;
    /* 经实测, 循环40次大约能满足400kHz的I2C时钟要求 */
    for(i = 0; i < 40; i++);
}


void i2c_Start(void)
{
    /* 当SCL高电平时，SDA出现一个下跳沿表示I2C总线启动信号 */
    EEPROM_I2C_SCL_1();
    EEPROM_I2C_SDA_1();
    i2c_Delay();
    EEPROM_I2C_SDA_0();
    i2c_Delay();
    EEPROM_I2C_SCL_0();
    i2c_Delay();
}


void i2c_Stop(void)
{
    /* 当SCL高电平时，SDA出现一个上跳沿表示I2C总线停止信号 */
    EEPROM_I2C_SDA_0();
    EEPROM_I2C_SCL_1();
    i2c_Delay();
    EEPROM_I2C_SDA_1();
    i2c_Delay();
}


void i2c_Ack(void)
{ 
    EEPROM_I2C_SDA_0();
    i2c_Delay();  
    EEPROM_I2C_SCL_1();
    i2c_Delay();
    
    EEPROM_I2C_SCL_0();
    i2c_Delay();
    
    EEPROM_I2C_SDA_1(); /* CPU释放SDA总线 */
    i2c_Delay();
}


void i2c_NAck(void)
{ 
    EEPROM_I2C_SDA_1();
    i2c_Delay();  
    EEPROM_I2C_SCL_1();
    i2c_Delay();
    EEPROM_I2C_SCL_0();
    i2c_Delay();
}


uint8_t i2c_WaitAck(void)
{
    uint8_t re;
    
    /* CPU释放SDA总线 */
    EEPROM_I2C_SDA_1(); 
    i2c_Delay();  
    
    /* CPU驱动SCL = 1, 此时器件会返回ACK应答 */
    EEPROM_I2C_SCL_1();
    i2c_Delay();
    
    /* CPU读取SDA口线状态 */
    if(GPIO_ReadInputDataBit(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN))
    {
        /* SDA 为高电平, 表示器件没有应答 */
        re = 1;
    }
    else
    {   
        /* SDA 为低电平, 器件应答 */
        re = 0;
    }
    
    EEPROM_I2C_SCL_0();
    i2c_Delay();
    
    return re;
}

void i2c_SendByte(uint8_t _ucByte)
{
    uint8_t i;
    
    /* 先发送字节的高位bit7 */
    for (i = 0; i < 8; i++)
    {  
        if (_ucByte & 0x80)
        {
            EEPROM_I2C_SDA_1();
        }
        else
        {
            EEPROM_I2C_SDA_0();
        }
        i2c_Delay(); 
        
        EEPROM_I2C_SCL_1();
        i2c_Delay();
        EEPROM_I2C_SCL_0();
        i2c_Delay();
        
        if(i == 7)
        {
            EEPROM_I2C_SDA_1();  /* 释放SDA总线 */
        }
        
        _ucByte <<= 1;
        i2c_Delay();
    }
}

/* CPU从I2C总线设备读取8bit数据 */
uint8_t i2c_ReadByte(void)
{
    uint8_t i;
    uint8_t value;

    /* 读到第1个bit为数据的bit7 */
    value = 0;
    for (i = 0; i < 8; i++)
    {
        value <<= 1;
        EEPROM_I2C_SCL_1();
        i2c_Delay();
        
        if (GPIO_ReadInputDataBit(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN))
        {
            value++;
        }
        
        EEPROM_I2C_SCL_0();
        i2c_Delay();
    }
    
    return value;
}

/* 检测I2C总线设备是否存在 */
/**
 * @brief 检测I2C总线设备是否存在
 * @param _Address: I2C设备地址
 * @retval 0: 设备存在，应答
 * @retval 1: 设备不存在，未应答
 */
uint8_t i2c_CheckDevice(uint8_t _Address)
{
    uint8_t ucAck;
    
    i2c_cfgGpio();  /* 配置GPIO */
    i2c_Start();    /* 发送启动信号 */
    
    /* 发送设备地址+读写控制bit（0 = w， 1 = r) bit7 先传 */
    i2c_SendByte(_Address | EEPROM_I2C_WR);
    ucAck = i2c_WaitAck(); /* 检测设备的ACK应答 */
    
    i2c_Stop();     /* 发送停止信号 */
    
    return ucAck;
}