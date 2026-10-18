#ifndef WATCH_IIC_H
#define WATCH_IIC_H

#include "stm32f4xx.h"
#include "timer.h"

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


/*============================================================================*/
/*                           软件    IIC  -2                                  */
typedef struct
{
	GPIO_TypeDef * IIC_SDA_PORT;
	GPIO_TypeDef * IIC_SCL_PORT;
	uint16_t IIC_SDA_PIN;
	uint16_t IIC_SCL_PIN;
	//void (*CLK_ENABLE)(void);
}iic_bus_t;

void IICStart(iic_bus_t *bus);
void IICStop(iic_bus_t *bus);
uint8_t IICWaitAck(iic_bus_t *bus);
void IICSendAck(iic_bus_t *bus);
void IICSendNotAck(iic_bus_t *bus);
void IICSendByte(iic_bus_t *bus, unsigned char cSendByte);
void IICReceiveByte(iic_bus_t *bus, unsigned char *receiveByte);
void IICInit(iic_bus_t *bus);
void IICDeinit(iic_bus_t *bus);

uint8_t IIC_Write_One_Byte(iic_bus_t *bus, uint8_t daddr,uint8_t reg,uint8_t data);
uint8_t IIC_Write_Multi_Byte(iic_bus_t *bus, uint8_t daddr,uint8_t reg,uint8_t length,uint8_t buff[]);
uint8_t IIC_Read_One_Byte(iic_bus_t *bus, uint8_t daddr,uint8_t reg, unsigned char *data);
uint8_t IIC_Read_Multi_Byte(iic_bus_t *bus, uint8_t daddr, uint8_t reg, uint8_t length, uint8_t buff[]);


/**
  * @brief  以标准库方式完成 I2C2 的 GPIO 与时钟初始化
  * @retval None
  */
void I2C2_GPIO_Init(void);


/**
  * @brief  释放 PB10 / PB3，回到模拟输入（高阻）状态
  * @note   刻意不调用 GPIO_DeInit(GPIOB)：那是复位整个 GPIOB 端口，
  *         会连带清掉 PB5 上的 MPU6050 INT 外部中断配置。
  * @retval None
  */
void I2C2_GPIO_DeInit(void);


/* MPU6050 的 7 位地址（ADO 引脚接地时）。ADO 接高电平则为 0x69 */
#define MPU_ADDR_7BIT           0x68

/**
 * SPL 的 I2C_Send7bitAddress() 只负责清/置 bit0，不做左移。
 * 所以必须传「已左移 1 位」的 8 位形式，否则总线上发出的是错误地址。
 *     0x68 << 1 = 0xD0
 */
#define MPU_ADDR_8BIT           (MPU_ADDR_7BIT << 1)

void hardware_iic_init(void);

void hardware_iic_deinit(void);

int8_t mpu_write_reg(uint8_t reg, const uint8_t *buf, uint16_t len);

uint8_t mpu_read_reg(uint8_t reg, uint8_t *buf, uint16_t len);

int8_t iic_mem_read(void *hi2c,
                             uint16_t dst_address,
                             uint16_t mem_addr,
                             uint16_t mem_size,
                             uint8_t  *p_data,
                             uint16_t size,
                             uint32_t timeout);

int8_t iic_mem_write(void *hi2c,
                              uint16_t dst_address,
                              uint16_t mem_addr,
                              uint16_t mem_size,
                              uint8_t  *p_data,
                              uint16_t size,
                              uint32_t timeout);
                              
                              
int8_t iic_mem_read_dma(void *hi2c,
                                 uint16_t dst_address,
                                 uint16_t mem_addr,
                                 uint16_t mem_size,
                                 uint8_t  *p_data,
                                 uint16_t size);                           

#endif //WATCH_IIC_H