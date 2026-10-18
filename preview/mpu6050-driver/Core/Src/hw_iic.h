/**
 * @file    hw_iic.h
 * @brief   硬件 I2C 引擎（标准外设库 SPL 实现），实现 iic_driver_interface_t 契约
 *
 * 将 STM32F411 的硬件 I2C1/2/3 外设包装成与软件位带 I2C 相同的细粒度原语：
 *   IICInit / IICStart / IICStop / IICSendByte / IICReceiveByte /
 *   IICWaitAck / IICSendAck / IICSendNotAck / IICBusReset
 *
 * 设备驱动层（如 bsp_mpu6050_driver.c）只认 iic_driver_interface_t 契约，
 * 因此从"软件 I2C + AHT21"切换到"硬件 I2C + MPU6050"只需在集成层
 * 重新填一张函数指针表，设备逻辑零改动——这正是桥接模式的意义。
 *
 * 事件等待均带超时（迭代计数上限），不会永久阻塞调度器；
 * 总线 BUSY 卡死时提供 hw_iic_bus_reset() 恢复路径。
 */
#ifndef __HW_IIC_H
#define __HW_IIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "iic_interface.h"

/** 硬件 I2C 引擎对外 API（供集成层填入 iic_driver_interface_t 的槽位） */
int32_t hw_iic_init(iic_bus_t * const bus);
int32_t hw_iic_deinit(iic_bus_t * const bus);
int32_t hw_iic_start(iic_bus_t * const bus);
int32_t hw_iic_stop(iic_bus_t * const bus);
int32_t hw_iic_send_byte(iic_bus_t * const bus, uint8_t byte);
int32_t hw_iic_receive_byte(iic_bus_t * const bus, uint8_t * const out);
int32_t hw_iic_wait_ack(iic_bus_t * const bus);
int32_t hw_iic_send_ack(iic_bus_t * const bus);
int32_t hw_iic_send_not_ack(iic_bus_t * const bus);
int32_t hw_iic_bus_reset(iic_bus_t * const bus);

/** 便捷填充：把上述实现挂到一份 iic_driver_interface_t 上 */
void hw_iic_mount_interface(iic_driver_interface_t *itf, iic_bus_t *bus);

#ifdef __cplusplus
}
#endif

#endif /* __HW_IIC_H */
