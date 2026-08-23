#include "iic.h"
#include <stdio.h>

// #define IIC_DEBUG

/*============================================================================*/
/*          软件 IIC    IIC总线设备操作函数     SCL PB8      SDA PB9        */

/**
 * @brief 配置IIC总线的GPIO引脚
 * @note  SCL和SDA引脚配置为开漏输出模式, 无上下拉, 高速
 * @retval None
 */
static void i2c_cfgGpio(void)
{
    #ifdef IIC_DEBUG
    printf("IIC i2c_cfgGpio\r\n");
    #endif
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能GPIOB时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    /* 配置SCL PB8 和SDA PB9 引脚 */
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
    #ifdef IIC_DEBUG
    printf("IIC i2c_Delay\r\n");
    #endif
    uint8_t i;
    /* 经实测, 循环40次大约能满足400kHz的I2C时钟要求 */
    for(i = 0; i < 40; i++);
}


void i2c_Start(void)
{
    #ifdef IIC_DEBUG
    printf("IIC i2c_Start\r\n");
    #endif
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
    #ifdef IIC_DEBUG
    printf("IIC i2c_Stop\r\n");
    #endif
    /* 当SCL高电平时，SDA出现一个上跳沿表示I2C总线停止信号 */
    EEPROM_I2C_SDA_0();
    EEPROM_I2C_SCL_1();
    i2c_Delay();
    EEPROM_I2C_SDA_1();
    i2c_Delay();
}


void i2c_Ack(void)
{
    #ifdef IIC_DEBUG
    printf("IIC i2c_Ack\r\n");
    #endif
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
    #ifdef IIC_DEBUG
    printf("IIC i2c_NAck\r\n");
    #endif
    EEPROM_I2C_SDA_1();
    i2c_Delay();  
    EEPROM_I2C_SCL_1();
    i2c_Delay();
    EEPROM_I2C_SCL_0();
    i2c_Delay();
}


uint8_t i2c_WaitAck(void)
{
    #ifdef IIC_DEBUG
    printf("IIC i2c_WaitAck\r\n");
    #endif
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
    #ifdef IIC_DEBUG
    printf("IIC i2c_SendByte\r\n");
    #endif
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
    #ifdef IIC_DEBUG
    printf("IIC i2c_ReadByte\r\n");
    #endif
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
    #ifdef IIC_DEBUG
    printf("IIC i2c_CheckDevice\r\n");
    #endif
    uint8_t ucAck;
    
    i2c_cfgGpio();  /* 配置GPIO */
    i2c_Start();    /* 发送启动信号 */
    
    /* 发送设备地址+读写控制bit（0 = w， 1 = r) bit7 先传 */
    i2c_SendByte(_Address | EEPROM_I2C_WR);
    ucAck = i2c_WaitAck(); /* 检测设备的ACK应答 */
    
    i2c_Stop();     /* 发送停止信号 */
    
    return ucAck;
}


/*============================================================================*/
/*           硬件 IIC     IIC总线设备操作函数      SCL PB8      SDA PB9          */
/**
  * @brief SDA线输入模式配置（直接写寄存器，与 HAL_GPIO_Init 逐位等价）
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @retval None
  */
void SDA_Input_Mode(iic_bus_t *bus)
{
    #ifdef IIC_DEBUG
    printf("IIC SDA_Input_Mode\r\n");
    #endif
    GPIO_InitTypeDef GPIO_InitStructure;

    /* SDA 线：输入 + 上拉 + 高速（标准库 GPIO_Init 写法，对应 HAL GPIO_MODE_INPUT + GPIO_PULLUP） */
    GPIO_InitStructure.GPIO_Pin   = bus->IIC_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;       /* GPIO_MODE_INPUT */
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;       /* GPIO_PULLUP */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   /* GPIO_SPEED_FREQ_HIGH（F4: OSPEEDR=2 → 50MHz） */
    GPIO_Init(bus->IIC_SDA_PORT, &GPIO_InitStructure);
}

/**
  * @brief SDA线输出模式配置（开漏 + 无上拉，与 13_AHT21 一致）
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @retval None
  */
void SDA_Output_Mode(iic_bus_t *bus)
{
    #ifdef IIC_DEBUG
    printf("IIC SDA_Output_Mode\r\n");
    #endif
    GPIO_InitTypeDef GPIO_InitStructure;

    /* SDA 线：输出 + 开漏 + 无上拉（标准库 GPIO_Init 写法，对应 HAL GPIO_MODE_OUTPUT_OD + GPIO_NOPULL） */
    GPIO_InitStructure.GPIO_Pin   = bus->IIC_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;        /* GPIO_MODE_OUTPUT_OD 的输出方向 */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;        /* 开漏 */
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;     /* GPIO_NOPULL */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;     /* GPIO_SPEED_FREQ_HIGH */
    GPIO_Init(bus->IIC_SDA_PORT, &GPIO_InitStructure);
}

/**
  * @brief SDA线输出一个位
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @param -[in] val 输出的数据
  * @retval None
  */
void SDA_Output(iic_bus_t *bus, uint16_t val)
{
    #ifdef IIC_DEBUG
    printf("IIC SDA_Output\r\n");
    #endif
    if ( val )
    {
        GPIO_SetBits(bus->IIC_SDA_PORT, bus->IIC_SDA_PIN);
    }
    else
    {
        GPIO_ResetBits(bus->IIC_SDA_PORT, bus->IIC_SDA_PIN);
    }
}

/**
  * @brief SCL线输出一个位
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @param -[in] val 输出的数据
  * @retval None
  */
void SCL_Output(iic_bus_t *bus, uint16_t val)
{
    #ifdef IIC_DEBUG
    printf("IIC SCL_Output\r\n");
    #endif
    if ( val )
    {
        GPIO_SetBits(bus->IIC_SCL_PORT, bus->IIC_SCL_PIN);
    }
    else
    {
        GPIO_ResetBits(bus->IIC_SCL_PORT, bus->IIC_SCL_PIN);
    }
}

/**
  * @brief SDA输入一位
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @retval unsigned char: GPIO读入一位
  */
uint8_t SDA_Input(iic_bus_t *bus)
{
    #ifdef IIC_DEBUG
    printf("IIC SDA_Input\r\n");
    #endif
    if(GPIO_ReadInputDataBit(bus->IIC_SDA_PORT, bus->IIC_SDA_PIN) == Bit_SET){
        return 1;
    }else{
        return 0;
    }
}

/**
  * @brief IIC起始信号
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @retval None
  */
void IICStart(iic_bus_t *bus)
{
    #ifdef IIC_DEBUG
    printf("IIC IICStart\r\n");
    #endif
    SDA_Output(bus,1);
    delay_us(2);
    SCL_Output(bus,1);
    delay_us(1);
    SDA_Output(bus,0);
    delay_us(1);
    SCL_Output(bus,0);
    delay_us(1);
}


/**
  * @brief IIC结束信号
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @retval None
  */
void IICStop(iic_bus_t *bus)
{
    #ifdef IIC_DEBUG
    printf("IIC IICStop\r\n");
    #endif
    SCL_Output(bus,0);
    delay_us(2);
    SDA_Output(bus,0);
    delay_us(1);
    SCL_Output(bus,1);
    delay_us(1);
    SDA_Output(bus,1);
    delay_us(1);
}

/**
  * @brief IIC等待确认信号
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @retval uint8_t: 0表示失败，1表示成功
  */
uint8_t IICWaitAck(iic_bus_t *bus)
{
    #ifdef IIC_DEBUG
    printf("IIC IICWaitAck\r\n");
    #endif
    unsigned short cErrTime = 5;
    SDA_Input_Mode(bus);
    SCL_Output(bus,1);
    while(SDA_Input(bus))
    {
        cErrTime--;
        delay_us(1);
        if (0 == cErrTime)
        {
            SDA_Output_Mode(bus);
            IICStop(bus);

            // printf("IICWaitAck error\r\n");
            return 0;
        }
    }
    SCL_Output(bus,0);
    delay_us(2);
    SDA_Output_Mode(bus);
    // printf("IICWaitAck ok\r\n");
    return 1;
}

/**
  * @brief IIC发送确认信号
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @retval None
  */
void IICSendAck(iic_bus_t *bus)
{
    #ifdef IIC_DEBUG
    printf("IIC IICSendAck\r\n");
    #endif
    SDA_Output(bus,0);
    delay_us(1);
    SCL_Output(bus,1);
    delay_us(1);
    SCL_Output(bus,0);
    delay_us(1);
}


/**
  * @brief IIC发送非确认信号
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @retval None
  */
void IICSendNotAck(iic_bus_t *bus)
{
    #ifdef IIC_DEBUG
    printf("IIC IICSendNotAck\r\n");
    #endif
    SDA_Output(bus,1);
    delay_us(1);
    SCL_Output(bus,1);
    delay_us(1);
    SCL_Output(bus,0);
    delay_us(2);
}


/**
  * @brief IIC发送一个字节
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @param -[in] unsigned char cSendByte: 需要发送的字节
  * @retval None
  */
void IICSendByte(iic_bus_t *bus, unsigned char cSendByte)
{
    #ifdef IIC_DEBUG
    printf("IIC IICSendByte\r\n");
    #endif
    unsigned char i = 8;
    while (i--)
    {
        SCL_Output(bus,0);
        delay_us(2);
        SDA_Output(bus, cSendByte & 0x80);
        delay_us(1);
        // cSendByte <<= 1;  // 修正：左移一位
        cSendByte += cSendByte;
        delay_us(1);
        SCL_Output(bus,1);
        delay_us(1);
    }
    SCL_Output(bus,0);
    delay_us(2);
}

/**
  * @brief IIC接收一个字节
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @retval unsigned char: 接收到的字节
  */
void IICReceiveByte(iic_bus_t *bus, unsigned char *receiveByte)
{
    #ifdef IIC_DEBUG
    printf("IIC IICReceiveByte\r\n");
    #endif
    unsigned char i = 8;
    unsigned char cR_Byte = 0;
    SDA_Input_Mode(bus);
    while (i--)
    {
        // cR_Byte <<= 1;  // 修正：先左移再接收
        cR_Byte += cR_Byte;
        SCL_Output(bus,0);
        delay_us(2);
        SCL_Output(bus,1);
        delay_us(1);
        cR_Byte |=  SDA_Input(bus);
    }
    SCL_Output(bus,0);
    SDA_Output_Mode(bus);
    *receiveByte = cR_Byte;
    return;
}


/**
  * @brief IIC单字节写入
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @param -[in] uint8_t daddr: 设备地址
  * @param -[in] uint8_t reg: 寄存器地址
  * @param -[in] uint8_t data: 写入数据
  * @retval uint8_t: 0表示成功，1表示失败
  */
uint8_t IIC_Write_One_Byte(iic_bus_t *bus, uint8_t daddr, uint8_t reg, uint8_t data)
{
    #ifdef IIC_DEBUG
    printf("IIC IIC_Write_One_Byte\r\n");
    #endif
    IICStart(bus);
    
    IICSendByte(bus, daddr<<1);
    if(IICWaitAck(bus))    //等待应答
    {
        IICStop(bus);
        return 1;
    }
    IICSendByte(bus, reg);
    IICWaitAck(bus);
    IICSendByte(bus, data);
    IICWaitAck(bus);
    IICStop(bus);
    delay_us(1);
    return 0;
}

/**
  * @brief IIC多字节写入
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @param -[in] uint8_t daddr: 设备地址
  * @param -[in] uint8_t reg: 寄存器地址
  * @param -[in] uint8_t length: 写入字节长度
  * @param -[in] uint8_t buff[]: 写入数据
  * @retval uint8_t: 0表示成功，1表示失败
  */
uint8_t IIC_Write_Multi_Byte(iic_bus_t *bus, uint8_t daddr, uint8_t reg, uint8_t length, uint8_t buff[])
{
    #ifdef IIC_DEBUG
    printf("IIC IIC_Write_Multi_Byte\r\n");
    #endif
    unsigned char i;
    IICStart(bus);
    
    IICSendByte(bus, daddr<<1);
    if(IICWaitAck(bus))
    {
        IICStop(bus);
        return 1;
    }
    IICSendByte(bus, reg);
    IICWaitAck(bus);
    for(i=0; i<length; i++)
    {
        IICSendByte(bus, buff[i]);
        IICWaitAck(bus);
    }
    IICStop(bus);
    delay_us(1);
    return 0;
}

/**
  * @brief IIC单字节读取
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @param -[in] uint8_t daddr: 设备地址
  * @param -[in] uint8_t reg: 寄存器地址
  * @param -[out] unsigned char *data: 存储读取到的数据
  * @retval uint8_t: 0表示成功，1表示失败
  */
uint8_t IIC_Read_One_Byte(iic_bus_t *bus, uint8_t daddr, uint8_t reg, unsigned char *data)
{
    #ifdef IIC_DEBUG
    printf("IIC IIC_Read_One_Byte\r\n");
    #endif
    unsigned char dat;
    IICStart(bus);
    IICSendByte(bus, daddr<<1);
    IICWaitAck(bus);
    IICSendByte(bus, reg);
    IICWaitAck(bus);
    
    IICStart(bus);
    IICSendByte(bus, (daddr<<1)+1);
    IICWaitAck(bus);
    IICReceiveByte(bus, &dat);
    IICSendNotAck(bus);
    IICStop(bus);
    *data = dat;
    return 0;
}

/**
  * @brief IIC多字节读取
  * @param -[in] iic_bus_t *bus: IIC总线结构体指针
  * @param -[in] uint8_t daddr: 设备地址
  * @param -[in] uint8_t reg: 寄存器地址
  * @param -[in] uint8_t length: 读取字节数
  * @param -[out] uint8_t buff[]: 存储读取数据
  * @retval uint8_t: 0表示成功，1表示失败
  */
uint8_t IIC_Read_Multi_Byte(iic_bus_t *bus, uint8_t daddr, uint8_t reg, uint8_t length, uint8_t buff[])
{
    #ifdef IIC_DEBUG
    printf("IIC IIC_Read_Multi_Byte\r\n");
    #endif
    unsigned char i;
    IICStart(bus);
    IICSendByte(bus, daddr<<1);
    if(IICWaitAck(bus))
    {
        IICStop(bus);
        return 1;
    }
    IICSendByte(bus, reg);
    IICWaitAck(bus);
    
    IICStart(bus);
    IICSendByte(bus, (daddr<<1)+1);
    IICWaitAck(bus);
    for(i=0; i<length; i++)
    {
        IICReceiveByte(bus, &buff[i]);
        if(i < length-1)
        {
            IICSendAck(bus);
        }
    }
    IICSendNotAck(bus);
    IICStop(bus);
    return 0;
}

/**
  * @brief IIC初始化
  * @param iic_bus_t *bus: IIC总线结构体指针
  * @retval None
  */
void IICInit(iic_bus_t *bus)
{
    #ifdef IIC_DEBUG
    printf("IIC IICInit\r\n");
    #endif
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 时钟使能：iic_bus_t 没有 CLK_ENABLE 回调成员，沿用工程现有约定由调用方负责
       若 SDA/SCL 都在 GPIOB，调用方需先执行：
       RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);  */

    /* SDA：推挽输出 + 上拉 + 高速（标准库 GPIO_Init 写法，对应 HAL GPIO_MODE_OUTPUT_PP + GPIO_PULLUP） */
    GPIO_InitStructure.GPIO_Pin   = bus->IIC_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;        /* GPIO_MODE_OUTPUT_PP 的输出方向 */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;        /* 推挽 */
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;         /* GPIO_PULLUP */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;     /* GPIO_SPEED_FREQ_HIGH */
    GPIO_Init(bus->IIC_SDA_PORT, &GPIO_InitStructure);

    /* SCL：推挽输出 + 上拉 + 高速（与 SDA 同一份配置，仅换引脚/端口） */
    GPIO_InitStructure.GPIO_Pin = bus->IIC_SCL_PIN;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_Init(bus->IIC_SCL_PORT, &GPIO_InitStructure);
}


/**
  * @brief IIC反初始化（释放GPIO，停止驱动总线）
  * @param iic_bus_t *bus: IIC总线结构体指针
  * @retval None
  */
void IICDeinit(iic_bus_t *bus) {
    #ifdef IIC_DEBUG
    printf("IIC IICDeinit\r\n");
    #endif
    GPIO_InitTypeDef GPIO_InitStructure;

    if (bus == NULL)
    {
        return;
    }

    /* 将 SDA / SCL 配置为输入、无上下拉，释放 MCU 对总线的驱动。
       外部上拉电阻会将其拉回 I2C 空闲态（高电平）。
       注意：不关闭 GPIOB 时钟——其他外设（如 EEPROM 用的 i2c_*）可能仍在使用 GPIOB。 */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    /* SDA 线 */
    GPIO_InitStructure.GPIO_Pin = bus->IIC_SDA_PIN;
    GPIO_Init(bus->IIC_SDA_PORT, &GPIO_InitStructure);

    /* SCL 线 */
    GPIO_InitStructure.GPIO_Pin = bus->IIC_SCL_PIN;
    GPIO_Init(bus->IIC_SCL_PORT, &GPIO_InitStructure);
}
