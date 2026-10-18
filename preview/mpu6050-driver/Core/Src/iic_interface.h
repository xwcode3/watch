/**
 * @file    iic_interface.h
 * @brief   I2C 总线抽象契约（桥接模式 · 总线/平台抽象层）
 *
 * 与现有 AHT21 工程的 iic_driver_interface_t 契约保持同一风格：
 *  - 所有 pf_iic_* 槽位均以 iic_bus_t *bus 为第一参数；
 *  - 返回值约定：0 = 失败(NACK/超时)，非 0 = 成功（与软件 I2C 的 IICWaitAck 语义一致）；
 *  - iic_bus_t 在原软件 I2C 字段（引脚）基础上扩展硬件外设字段，
 *    软件/硬件两种引擎共用同一结构：hw_enabled = 0 走软件位带，1 走硬件外设。
 *
 * 集成说明：若工程中已存在等价的 iic 契约头文件，删除本文件并将
 * hw_iic.c 的 include 改指到既有头文件即可（字段名保持一致则无需改代码）。
 *
 * @note    适用平台：STM32F411CEU6 + 标准外设库(SPL) + FreeRTOS
 */
#ifndef __IIC_INTERFACE_H
#define __IIC_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "stm32f4xx.h"

/** 总线描述：一条 I2C 总线 = 一组引脚 + （可选）一个硬件外设 */
typedef struct iic_bus {
    /* ---- 软件 I2C 引脚描述（硬件模式下作为 AF 复用引脚使用） ---- */
    GPIO_TypeDef *scl_port;         /*!< SCL 端口，如 GPIOB */
    uint16_t      scl_pin;          /*!< SCL 引脚，如 GPIO_Pin_6 */
    GPIO_TypeDef *sda_port;         /*!< SDA 端口，如 GPIOB */
    uint16_t      sda_pin;          /*!< SDA 引脚，如 GPIO_Pin_7 */

    /* ---- 硬件 I2C 外设描述（hw_enabled = 1 时生效） ---- */
    I2C_TypeDef  *i2c_periph;       /*!< I2C1 / I2C2 / I2C3 */
    uint32_t      i2c_clk_speed;   /*!< 100000(标准) / 400000(快速) */
    uint32_t      i2c_timeout_ms;  /*!< 单次事件等待超时（毫秒级等效循环数换算见 hw_iic.c） */
    uint8_t       hw_enabled;       /*!< 1 = 硬件外设引擎；0 = 软件位带引擎 */

    struct iic_bus *next;           /*!< 同一外设上挂载的下一总线（预留多从机共用） */
} iic_bus_t;

/**
 * @brief I2C 引擎操作契约（驱动层唯一认账的"实现"维度）
 * @note  返回值约定统一：0 = 失败，非 0 = 成功（对齐软件 I2C 的 IICWaitAck 语义）。
 */
typedef struct iic_driver_interface {
    int32_t (*pf_iic_init)(iic_bus_t * const bus);
    int32_t (*pf_iic_deinit)(iic_bus_t * const bus);
    int32_t (*pf_iic_start)(iic_bus_t * const bus);
    int32_t (*pf_iic_stop)(iic_bus_t * const bus);
    int32_t (*pf_iic_send_byte)(iic_bus_t * const bus, uint8_t byte);
    int32_t (*pf_iic_receive_byte)(iic_bus_t * const bus, uint8_t * const out);
    int32_t (*pf_iic_wait_ack)(iic_bus_t * const bus);     /*!< 1=ACK，0=NACK/超时 */
    int32_t (*pf_iic_send_ack)(iic_bus_t * const bus);
    int32_t (*pf_iic_send_not_ack)(iic_bus_t * const bus);
    int32_t (*pf_iic_bus_reset)(iic_bus_t * const bus);    /*!< 总线卡死恢复（硬件引擎提供） */

    iic_bus_t *iic_bus;                                    /*!< 当前挂载的总线 */
    void      *pf_critical_interface;                     /*!< 临界区接口（沿用现有约定，可空） */
} iic_driver_interface_t;

#ifdef __cplusplus
}
#endif

#endif /* __IIC_INTERFACE_H */
