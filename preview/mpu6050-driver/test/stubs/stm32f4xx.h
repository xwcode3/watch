/* SPL stub: 仅覆盖本驱动用到的类型/函数/宏（交叉语法检查用） */
#ifndef __STM32F4XX_STUB_H
#define __STM32F4XX_STUB_H
#include <stdint.h>

typedef enum {DISABLE = 0, ENABLE = !DISABLE} FunctionalState;
typedef enum {RESET = 0, SET = !RESET} FlagStatus, ITStatus;
typedef enum {ERROR = 0, SUCCESS = !ERROR} ErrorStatus;

typedef struct { volatile uint32_t SR1, SR2; } I2C_TypeDefStub;
typedef struct { volatile uint32_t CRL, CRH, IDR, ODR, BSRR, LCKR, AFR[2]; } GPIO_TypeDefStub;

/* 实际工程中 I2C_TypeDef/GPIO_TypeDef 由 SPL 提供；stub 用简化布局 */
typedef struct { volatile uint32_t CR1,CR2,OAR1,OAR2,DR,SR1,SR2,CCR,TRISE; } I2C_TypeDef;
typedef struct { volatile uint32_t MODER,OTYPER,OSPEEDR,PUPDR,IDR,ODR,BSRR,LCKR,AFR[2]; } GPIO_TypeDef;

#define I2C1 ((I2C_TypeDef*)0x40005400UL)
#define I2C2 ((I2C_TypeDef*)0x40005800UL)
#define I2C3 ((I2C_TypeDef*)0x40005C00UL)
#define GPIOA ((GPIO_TypeDef*)0x40020000UL)
#define GPIOB ((GPIO_TypeDef*)0x40020400UL)
#define GPIOC ((GPIO_TypeDef*)0x40020800UL)

/* GPIO */
typedef struct {
  uint32_t GPIO_Pin; uint32_t GPIO_Mode; uint32_t GPIO_Speed;
  uint32_t GPIO_OType; uint32_t GPIO_PuPd;
} GPIO_InitTypeDef;
#define GPIO_Pin_0 ((uint16_t)0x0001)
#define GPIO_Pin_1 ((uint16_t)0x0002)
#define GPIO_Pin_2 ((uint16_t)0x0004)
#define GPIO_Pin_3 ((uint16_t)0x0008)
#define GPIO_Pin_4 ((uint16_t)0x0010)
#define GPIO_Pin_5 ((uint16_t)0x0020)
#define GPIO_Pin_6 ((uint16_t)0x0040)
#define GPIO_Pin_7 ((uint16_t)0x0080)
#define GPIO_Pin_8 ((uint16_t)0x0100)
#define GPIO_Pin_9 ((uint16_t)0x0200)
#define GPIO_Pin_10 ((uint16_t)0x0400)
#define GPIO_Pin_11 ((uint16_t)0x0800)
#define GPIO_Pin_12 ((uint16_t)0x1000)
#define GPIO_Pin_13 ((uint16_t)0x2000)
#define GPIO_Pin_14 ((uint16_t)0x4000)
#define GPIO_Pin_15 ((uint16_t)0x8000)
#define GPIO_Pin_All ((uint16_t)0xFFFF)
#define GPIO_Mode_IN   0x00
#define GPIO_Mode_OUT  0x01
#define GPIO_Mode_AF   0x02
#define GPIO_Mode_AN   0x03
#define GPIO_OType_OD  0x00
#define GPIO_OType_PP  0x01
#define GPIO_PuPd_NOPULL 0x00
#define GPIO_PuPd_UP     0x01
#define GPIO_PuPd_DOWN   0x02
#define GPIO_Low_Speed    0x00
#define GPIO_Medium_Speed 0x01
#define GPIO_Fast_Speed   0x02
#define GPIO_High_Speed   0x03
#define GPIO_AF_I2C1  ((uint8_t)0x04)
#define GPIO_AF_I2C2  ((uint8_t)0x04)
#define GPIO_AF_I2C3  ((uint8_t)0x04)

void GPIO_Init(GPIO_TypeDef* GPIOx, GPIO_InitTypeDef* GPIO_InitStruct);
void GPIO_PinAFConfig(GPIO_TypeDef* GPIOx, uint16_t GPIO_PinSource, uint8_t GPIO_AF);
void GPIO_SetBits(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void GPIO_ResetBits(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);

/* RCC */
#define RCC_AHB1Periph_GPIOA ((uint32_t)0x00000001)
#define RCC_AHB1Periph_GPIOB ((uint32_t)0x00000002)
#define RCC_AHB1Periph_GPIOC ((uint32_t)0x00000004)
#define RCC_APB1Periph_I2C1  ((uint32_t)0x00200000)
#define RCC_APB1Periph_I2C2  ((uint32_t)0x00400000)
#define RCC_APB1Periph_I2C3  ((uint32_t)0x00800000)
void RCC_AHB1PeriphClockCmd(uint32_t periph, FunctionalState state);
void RCC_APB1PeriphClockCmd(uint32_t periph, FunctionalState state);
void RCC_APB1PeriphResetCmd(uint32_t periph, FunctionalState state);

/* I2C */
typedef struct {
  uint32_t I2C_ClockSpeed; uint32_t I2C_Mode; uint32_t I2C_DutyCycle;
  uint32_t I2C_OwnAddress1; uint32_t I2C_Ack; uint32_t I2C_AcknowledgedAddress;
} I2C_InitTypeDef;
#define I2C_Mode_I2C       ((uint16_t)0x0000)
#define I2C_DutyCycle_16_9 ((uint16_t)0x4000)
#define I2C_DutyCycle_2    ((uint16_t)0xBFFF)
#define I2C_Ack_Enable     ((uint16_t)0x0400)
#define I2C_Ack_Disable    ((uint16_t)0x0000)
#define I2C_AcknowledgedAddress_7bit ((uint16_t)0x4000)
typedef enum { I2C_Register_SR1=0x00, I2C_Register_SR2=0x04 } I2C_Register;
#define I2C_FLAG_BUSY ((uint32_t)0x00020000)
#define I2C_FLAG_AF   ((uint32_t)0x00000400)
#define I2C_FLAG_ADDR ((uint32_t)0x00000002)
#define I2C_FLAG_TXE  ((uint32_t)0x00000080)
#define I2C_FLAG_RXNE ((uint32_t)0x00000040)
#define I2C_FLAG_SB   ((uint32_t)0x00000001)
#define I2C_FLAG_BTF  ((uint32_t)0x00000004)
void I2C_Init(I2C_TypeDef* I2Cx, I2C_InitTypeDef* I2C_InitStruct);
void I2C_StructInit(I2C_InitTypeDef* I2C_InitStruct);
void I2C_Cmd(I2C_TypeDef* I2Cx, FunctionalState state);
void I2C_GenerateSTART(I2C_TypeDef* I2Cx, FunctionalState state);
void I2C_GenerateSTOP(I2C_TypeDef* I2Cx, FunctionalState state);
void I2C_SendData(I2C_TypeDef* I2Cx, uint8_t Data);
uint8_t I2C_ReceiveData(I2C_TypeDef* I2Cx);
void I2C_AcknowledgeConfig(I2C_TypeDef* I2Cx, FunctionalState state);
FlagStatus I2C_GetFlagStatus(I2C_TypeDef* I2Cx, uint32_t flag);
void I2C_ClearFlag(I2C_TypeDef* I2Cx, uint32_t flag);
uint16_t I2C_ReadRegister(I2C_TypeDef* I2Cx, I2C_Register reg);

#define __NOP() __asm volatile ("nop")
#endif
