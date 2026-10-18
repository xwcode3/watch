#include "iic.h"
#include <stdio.h>
#include "bsp_mpu6050_handler.h"

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



/**
  * @brief  以标准库方式完成 I2C2 的 GPIO 与时钟初始化
  * @retval None
  */
void I2C2_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 1. 使能 GPIOB 时钟（APB/AHB 总线时钟必须先开，否则写 GPIO 寄存器无效） */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    /* 2. 配置 PB10 / PB3 的引脚属性
     *
     *    GPIO_Mode_AF   复用功能模式
     *    GPIO_OType_OD  开漏。I2C 总线必须开漏，且必须依赖外部上拉电阻：
     *                   GY-521 这类模块一般板载 4.7k，裸芯片需自行外接。
     *                   所以这里 GPIO_PuPd_NOPULL，不启用内部上下拉。
     *    GPIO_Speed     100 MHz，对应 HAL 的 GPIO_SPEED_FREQ_VERY_HIGH。
     *                   老版本 SPL 里该枚举叫 GPIO_Speed_100MHz，
     *                   1.3.0 之后改名为 GPIO_High_Speed 并保留旧名作兼容宏。
     */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 3. 选择复用功能号
     *
     *    PB10 -> AF4 (I2C2_SCL)
     *    PB3  -> AF9 (I2C2_SDA)
     *
     *    若编译时报 GPIO_AF9_I2C2 未定义：部分 SPL 版本把这个宏放在
     *    "#ifdef STM32F401xx" 分支里（注释写着 Only for STM32F401xx Devices），
     *    F411 工程需要在 stm32f4xx.h 确认已定义 STM32F411xE，
     *    或直接在 stm32f4xx_gpio.h 的 AF9 段补一行：
     *        #define GPIO_AF9_I2C2  ((uint8_t)0x09)
     */
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_I2C2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3,  GPIO_AF9_I2C2);

    /* 4. 使能 I2C2 时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
}


/**
  * @brief  释放 PB10 / PB3，回到模拟输入（高阻）状态
  * @note   刻意不调用 GPIO_DeInit(GPIOB)：那是复位整个 GPIOB 端口，
  *         会连带清掉 PB5 上的 MPU6050 INT 外部中断配置。
  * @retval None
  */
void I2C2_GPIO_DeInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}


/* =========================== 硬件 IIC 实现 ============================== */

/**
 * 各等待环节的超时计数上限。
 * 按 100 kHz、主频 100 MHz 估算，单字节收发约需几百个循环；
 * 100000 次足够覆盖正常情况，又能让总线异常时快速返回。
 */
#define I2C_SPL_TIMEOUT         100000U

void hardware_iic_init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef I2C_InitStructure;

    /* 1. 使能时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    
    /* 2. 引脚复用号 */
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_I2C2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3,  GPIO_AF9_I2C2);

    /* 3. 配置 PB10 / PB3 的引脚属性 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 4. IIC 外设参数 */
    I2C_DeInit(I2C2);

    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;
    I2C_Init(I2C2, &I2C_InitStructure);

    I2C_Init(I2C2, &I2C_InitStructure);
    I2C_Cmd(I2C2, ENABLE);
}

void hardware_iic_deinit(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 1. 关闭外设并复位 */
    I2C_Cmd(I2C2, DISABLE);
    I2C_DeInit(I2C2);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, DISABLE);

    /* 2. 两个引脚恢复为模拟输入（高阻），不再挂在总线上
     *
     *    刻意不调用 GPIO_DeInit(GPIOB)：那是复位整个 GPIOB 端口，
     *    会连带清掉 PB5 上的 MPU6050 外部中断配置。
     */
    
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
 * @brief  等待指定的 I2C 事件
 * @param  event   : SPL 的事件宏，如 I2C_EVENT_MASTER_MODE_SELECT
 * @retval 0 = 等到；非 0 = 超时
 */
static uint8_t i2c_wait_event(uint32_t event)
{
    uint32_t timeout = I2C_SPL_TIMEOUT;

    while (I2C_CheckEvent(I2C2, event) == ERROR)
    {
        if (timeout-- == 0U)
        {
            return 1U;
        }
    }
    return 0U;
}

/**
 * @brief  等待总线空闲（BUSY 标志清零）
 * @retval 0 = 空闲；非 0 = 超时
 */
static uint8_t i2c_wait_idle(void)
{
    uint32_t timeout = I2C_SPL_TIMEOUT;

    while (I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY) != RESET)
    {
        if (timeout-- == 0U)
        {
            return 1U;
        }
    }
    return 0U;
}

/**
 * @brief  异常收尾：补发 STOP 并恢复 ACK，让总线回到可用的空闲态
 */
static void i2c_abort(void)
{
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_GenerateSTOP(I2C2, ENABLE);
}

int8_t mpu_write_reg(uint8_t reg, const uint8_t *buf, uint16_t len) {
    if ((buf == NULL) || (len == 0)) {
        return -1;
    }

    /* 1. 等总线空闲 */
    if (i2c_wait_idle() != 0) {
        return -2;
    }

    /* 2. START —— EV5：起始条件已发出 */
    I2C_GenerateSTART(I2C2, ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0U)
    {
        i2c_abort();
        return -3;
    }

    /* 3. 从机地址 + 写方向 —— EV6：地址已被应答 */
    I2C_Send7bitAddress(I2C2, MPU_ADDR_8BIT, I2C_Direction_Transmitter);
    if (i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != 0U)
    {
        i2c_abort();
        return -4;
    }

    /* 4. 寄存器地址 —— EV8_2：字节真正发完（TXE=1 且 BTF=1） */
    I2C_SendData(I2C2, reg);
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0U)
    {
        i2c_abort();
        return -5;
    }

    /* 5. 逐个发数据字节 */
    while (len > 0U)
    {
        I2C_SendData(I2C2, *buf);
        if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0U)
        {
            i2c_abort();
            return -6;
        }
        buf++;
        len--;
    }

    /* 6. STOP */
    I2C_GenerateSTOP(I2C2, ENABLE);
    return 0;
}


uint8_t mpu_read_reg(uint8_t reg, uint8_t *buf, uint16_t len)
{
    if ((buf == 0) || (len == 0U))
    {
        return -1;
    }

    /* 1. 等总线空闲 */
    if (i2c_wait_idle() != 0U)
    {
        return -1;
    }

    /* ---------- 第一阶段：把要读的寄存器地址告诉从机 ---------- */

    /* 2. START */
    I2C_GenerateSTART(I2C2, ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0U)
    {
        i2c_abort();
        return -1;
    }

    /* 3. 地址 + 写方向 */
    I2C_Send7bitAddress(I2C2, MPU_ADDR_8BIT, I2C_Direction_Transmitter);
    if (i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != 0U)
    {
        i2c_abort();
        return -1;
    }

    /* 4. 发寄存器地址 */
    I2C_SendData(I2C2, reg);
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0U)
    {
        i2c_abort();
        return -1;
    }

    /* ---------- 第二阶段：RESTART 后改为接收 ---------- */

    /* 5. RESTART（不发 STOP，总线保持占用），EV5 */
    I2C_GenerateSTART(I2C2, ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0U)
    {
        i2c_abort();
        return -1;
    }

    /* 6. 地址 + 读方向，EV6 */
    I2C_Send7bitAddress(I2C2, MPU_ADDR_8BIT, I2C_Direction_Receiver);
    if (i2c_wait_event(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != 0U)
    {
        i2c_abort();
        return -1;
    }

    /* ---------- 第三阶段：逐字节接收 ---------- */

    while (len > 0U)
    {
        if (len == 1U)
        {
            /* 最后一个字节：先关 ACK（让主机回 NACK），再排 STOP。
             * 顺序不能反，否则从机会以为还有下一个字节，重复读取。 */
            I2C_AcknowledgeConfig(I2C2, DISABLE);
            I2C_GenerateSTOP(I2C2, ENABLE);
        }

        /* EV7：RXNE 置位，DR 里有新数据 */
        if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_RECEIVED) != 0U)
        {
            i2c_abort();
            return -1;
        }

        *buf = I2C_ReceiveData(I2C2);
        buf++;
        len--;
    }

    /* 恢复 ACK，为下一次接收做准备 */
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    return 0;
}


/**
 * @brief  读 MPU6050 寄存器（阻塞）
 * @note   包装 i2c_spl.c 的 MPU_ReadReg()。接口签名里的几个参数在 SPL
 *         阻塞实现中的去向（与 HAL 版不同，如实标注）：
 *             hi2c        : SPL 没有句柄概念，不使用（总线固定为 I2C2）
 *             dst_address : 器件地址固定由 i2c_spl.h 的 MPU_ADDR_7BIT(0x68)
 *                           决定；如需 AD0 接高电平的 0x69，改那个宏
 *             mem_size    : MPU6050 寄存器地址固定 8 位（IIC_MEMADD_SIZE_8BIT）
 *             timeout     : SPL 版超时用循环计数（I2C_SPL_TIMEOUT），不用 ms 值
 */
int8_t iic_mem_read(void *hi2c,
                             uint16_t dst_address,
                             uint16_t mem_addr,
                             uint16_t mem_size,
                             uint8_t  *p_data,
                             uint16_t size,
                             uint32_t timeout)
{
    uint8_t ret;

    (void)hi2c;
    (void)dst_address;
    (void)mem_size;
    (void)timeout;

    ret = mpu_read_reg((uint8_t)mem_addr, p_data, size);

    return (ret == 0) ? 1 : ret;
}


/**
 * @brief  写 MPU6050 寄存器（阻塞）
 * @note   包装 i2c_spl.c 的 MPU_WriteReg()；参数说明同 iic_mem_read()。
 */
int8_t iic_mem_write(void *hi2c,
                              uint16_t dst_address,
                              uint16_t mem_addr,
                              uint16_t mem_size,
                              uint8_t  *p_data,
                              uint16_t size,
                              uint32_t timeout)
{
    int8_t ret;

    (void)hi2c;
    (void)dst_address;
    (void)mem_size;
    (void)timeout;

    ret = mpu_write_reg((uint8_t)mem_addr, p_data, size);

    return (ret == 0) ? 1 : ret;
}

/* DMA 异步读是否还在进行中：0 = 空闲，1 = 已挂 DMA 等待完成。
 * 由 iic_mem_read_dma() 置 1，由 DMA1_Stream2_IRQHandler() 清零。
 * 14 字节 @100 kHz 约 1.5 ms 完成，正常情况下不会和第二轮请求撞上；
 * 该标志用于拦住“上一轮卡死”时的重复发起。 */
static volatile uint8_t s_dma_busy = 0U;
/**
 * @brief  DMA 读的请求阶段失败时收尾：停 DMA 流、关 DMA 请求、
 *         补 STOP、恢复 ACK，让总线回到可用的空闲态
 */
static void i2c_dma_abort(void)
{
    DMA_Cmd(DMA1_Stream2, DISABLE);
    I2C_DMACmd(I2C2, DISABLE);
    I2C_DMALastTransferCmd(I2C2, DISABLE);
    I2C_ClearFlag(I2C2, I2C_FLAG_AF);           /* 地址无应答时清 AF */
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_GenerateSTOP(I2C2, ENABLE);

    s_dma_busy = 0U;
}

/**
 * @brief  等待从机地址被应答（ADDR = 1）
 * @note   刻意只读 SR1，不用 I2C_CheckEvent()：SPL 的 I2C_CheckEvent 内部
 *         先读 SR1 再读 SR2，读 SR2 会把 ADDR 清掉；而 DMA 读里清 ADDR 的
 *         时机必须推迟到“置 CR2.LAST、开 CR2.DMAEN”之后（与 HAL 的顺序一致）。
 * @retval 0 = 已应答；非 0 = 超时
 */
static uint8_t i2c_wait_addr(void)
{
    uint32_t timeout = I2C_SPL_TIMEOUT;

    while ((I2C2->SR1 & I2C_SR1_ADDR) == 0U)
    {
        if (timeout-- == 0U)
        {
            return 1U;
        }
    }
    return 0U;
}

/**
 * @brief  用 DMA 异步读寄存器（对应 HAL_I2C_Mem_Read_DMA）
 *
 * @note   调用上下文：由 EXTI 回调（中断里）发起。函数自身只做“请求阶段”的
 *         短等待，数据接收交给 DMA 后台完成，随后立即返回；完成/出错由
 *         DMA1_Stream2_IRQHandler（本文件第 2 节）通知驱动层。
 *
 *         寄存器读时序（与 HAL 内部一致）：
 *           START → 地址+写 → 寄存器地址 → RESTART → 地址+读 → [挂 DMA]
 *         收尾三步的顺序与 HAL_I2C_Mem_Read_DMA 完全相同：
 *           ① 置 CR2.LAST    —— 最后一个 DMA 字节后硬件自动 NACK + STOP
 *           ② 读 SR1 + SR2   —— 清 ADDR，接收从此刻开始
 *           ③ 置 CR2.DMAEN   —— 放行 DMA 请求
 *
 *         与 HAL 版的已知差异（精简取舍）：
 *           - HAL 会开启 I2C 错误中断（I2C_IT_ERR）监控数据阶段错误，
 *             本实现未启用；DMA 侧错误由 TE 中断兜底。
 *           - HAL 对 size == 1 有单独分支；MPU6050 固定读 14 字节，不涉及。
 */
int8_t iic_mem_read_dma(void *hi2c,
                                 uint16_t dst_address,
                                 uint16_t mem_addr,
                                 uint16_t mem_size,
                                 uint8_t  *p_data,
                                 uint16_t size)
{
    DMA_InitTypeDef  dma_init;
    NVIC_InitTypeDef nvic_init;
    uint8_t  dev_addr;
    uint32_t timeout;

    (void)hi2c;     /* SPL 无句柄，总线固定 I2C2 */

    if ((p_data == 0) || (size == 0U))
    {
        return -1;
    }

    if (s_dma_busy != 0U)       /* 上一轮还没收完（正常约 1.5 ms 完成） */
    {
        return -1;
    }

    /* ---------- 第一步：准备 DMA 流（停流 → 复位 → 配置 → 开中断 → 开流） ---------- */

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);    /* 幂等 */

    nvic_init.NVIC_IRQChannel                   = DMA1_Stream2_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 5;        /* 与 HAL 工程一致；该档位允许调用 FreeRTOS API */
    nvic_init.NVIC_IRQChannelSubPriority        = 0;
    nvic_init.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic_init);

    DMA_Cmd(DMA1_Stream2, DISABLE);
    timeout = I2C_SPL_TIMEOUT;
    while ((DMA_GetCmdStatus(DMA1_Stream2) != DISABLE) && (timeout-- != 0U))
    {
    }
    if (DMA_GetCmdStatus(DMA1_Stream2) != DISABLE)
    {
        return -1;    /* 上一次传输没停干净（正常不会到这里） */
    }

    DMA_DeInit(DMA1_Stream2);
    DMA_StructInit(&dma_init);
    dma_init.DMA_Channel            = DMA_Channel_7;        /* I2C2_RX 固定映射：DMA1_Stream2 / CH7 */
    dma_init.DMA_PeripheralBaseAddr = (uint32_t)&I2C2->DR;
    dma_init.DMA_Memory0BaseAddr    = (uint32_t)p_data;
    dma_init.DMA_DIR                = DMA_DIR_PeripheralToMemory;
    dma_init.DMA_BufferSize         = size;
    dma_init.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma_init.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma_init.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    dma_init.DMA_Mode               = DMA_Mode_Normal;
    dma_init.DMA_Priority           = DMA_Priority_Low;     /* 与 HAL MspInit 相同 */
    dma_init.DMA_FIFOMode           = DMA_FIFOMode_Disable; /* 直连模式 */
    dma_init.DMA_FIFOThreshold      = DMA_FIFOThreshold_HalfFull;
    dma_init.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
    dma_init.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;
    DMA_Init(DMA1_Stream2, &dma_init);

    /* 传输完成 + 传输错误中断 */
    DMA_ITConfig(DMA1_Stream2, DMA_IT_TC | DMA_IT_TE, ENABLE);

    /* 先开流；此时 I2C 还没发 DMA 请求，不会动数据（与 HAL_DMA_Start_IT 顺序一致） */
    DMA_Cmd(DMA1_Stream2, ENABLE);

    /* ---------- 第二步：请求阶段（START → 地址+写 → 寄存器地址 → RESTART → 地址+读） ---------- */

    dev_addr = (uint8_t)(dst_address & 0xFEU);  /* bit0 交给 I2C_Send7bitAddress 按方向处理 */

    if (i2c_wait_idle() != 0U)
    {
        i2c_dma_abort();
        return -1;
    }

    I2C_AcknowledgeConfig(I2C2, ENABLE);

    I2C_GenerateSTART(I2C2, ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0U)
    {
        i2c_dma_abort();
        return -1;
    }

    I2C_Send7bitAddress(I2C2, dev_addr, I2C_Direction_Transmitter);
    if (i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != 0U)
    {
        i2c_dma_abort();
        return -1;
    }

    if (mem_size == 2U)         /* 16 位寄存器地址：先发高字节 */
    {
        I2C_SendData(I2C2, (uint8_t)(mem_addr >> 8));
        if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0U)
        {
            i2c_dma_abort();
            return -1;
        }
    }

    I2C_SendData(I2C2, (uint8_t)(mem_addr & 0xFFU));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0U)
    {
        i2c_dma_abort();
        return -1;
    }

    I2C_GenerateSTART(I2C2, ENABLE);    /* RESTART，不发 STOP，总线保持占用 */
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0U)
    {
        i2c_dma_abort();
        return -1;
    }

    I2C_Send7bitAddress(I2C2, dev_addr, I2C_Direction_Receiver);

    /* 等到 ADDR 即止（只读 SR1，ADDR 保持置位，等下第三步再清） */
    if (i2c_wait_addr() != 0U)
    {
        i2c_dma_abort();
        return -1;
    }

    /* ---------- 第三步：挂上 DMA 并放行（顺序与 HAL_I2C_Mem_Read_DMA 完全一致） ---------- */

    I2C_DMALastTransferCmd(I2C2, ENABLE);   /* ① 最后一个 DMA 字节后硬件自动 NACK + STOP */

    (void)I2C_ReadRegister(I2C2, I2C_Register_SR1);
    (void)I2C_ReadRegister(I2C2, I2C_Register_SR2);     /* ② 先读 SR1 再读 SR2 → 清 ADDR */

    s_dma_busy = 1U;
    I2C_DMACmd(I2C2, ENABLE);               /* ③ 放行 DMA 请求，CPU 从这里开始干别的 */

    return 1;
}


extern bsp_mpuxxx_handler_t handler_instance;
extern void (*pf_dma_interrupt_callback)(void *, void *);
void DMA1_Stream2_IRQHandler(void)
{
    /* ---------- 传输完成：最后一个字节已进内存，硬件已自动 NACK + STOP ---------- */
    // if (DMA_GetITStatus(DMA1_Stream2, DMA_IT_TCIF2) != RESET)
    // {
    //     DMA_ClearITPendingBit(DMA1_Stream2, DMA_IT_TCIF2);

    //     DMA_Cmd(DMA1_Stream2, DISABLE);
    //     I2C_DMACmd(I2C2, DISABLE);
    //     I2C_AcknowledgeConfig(I2C2, ENABLE);    /* 恢复 ACK，为下一次接收做准备 */
    //     s_dma_busy = 0U;

    //     GPIO_ResetBits(GPIOA, GPIO_Pin_1);      /* 原 DMA_FUNC 测量脚（PA1），不需要可删 */
    //     if (pf_dma_interrupt_callback != 0)
    //     {
    //         pf_dma_interrupt_callback(handler_instance.p_driver, NULL);
    //     }
    //     GPIO_SetBits(GPIOA, GPIO_Pin_1);
    // }
    if (DMA_GetITStatus(DMA1_Stream2, DMA_IT_TCIF2) != RESET) {
        uint32_t timeout = I2C_SPL_TIMEOUT;

        DMA_ClearITPendingBit(DMA1_Stream2, DMA_IT_TCIF2);

        /* 1. 停 DMA */
        DMA_Cmd(DMA1_Stream2, DISABLE);
        I2C_DMALastTransferCmd(I2C2, DISABLE);
        I2C_DMACmd(I2C2, DISABLE);

        /* 2. 收尾：先关 ACK，再软件补 STOP —— 顺序同 HAL_I2C_MasterRxCplt */
        I2C_AcknowledgeConfig(I2C2, DISABLE);
        I2C_GenerateSTOP(I2C2, ENABLE);

        /* 3. 确认总线真的空闲了（这一步就是用来验证 -2 有没有被治掉） */
        while ((I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY) != RESET) && (timeout-- != 0U))
        {
        }
        if (I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY) != RESET)
        {
            log_e("dma isr: bus busy stuck");     /* 还卡 => 从机拉死 SDA，见下 */
        }

        I2C_ClearFlag(I2C2, I2C_FLAG_AF);
        I2C_AcknowledgeConfig(I2C2, ENABLE);      /* 为下一次接收恢复 ACK */
        s_dma_busy = 0U;

        GPIO_ResetBits(GPIOA, GPIO_Pin_1);
        if (pf_dma_interrupt_callback != 0)
        {
            pf_dma_interrupt_callback(handler_instance.p_driver, NULL);
        }
        GPIO_SetBits(GPIOA, GPIO_Pin_1);
    }


    /* ---------- 传输错误（DMA 侧，极罕见）：按失败路径收尾 ---------- */
    if (DMA_GetITStatus(DMA1_Stream2, DMA_IT_TEIF2) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_Stream2, DMA_IT_TEIF2);
        i2c_dma_abort();
    }
}