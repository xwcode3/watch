#pragma once
#ifndef AHT21_TEST_H
#define AHT21_TEST_H

#include <stdint.h>
#include "bsp_aht21_driver.h"

/**
 * @brief AHT21 硬件自检函数（独立于 bsp_aht21_handler 体系，不改动任何现有代码）
 *
 * 检测内容（参照 AHT21 官方流程）：
 *   1. 打开 GPIOB 时钟，配置 PB13(SDA)/PB14(SCL)；
 *   2. 上电等待 100ms；
 *   3. 发 0x70 探测地址，检查从机是否 ACK —— 硬件是否在总线上的直接判据；
 *   4. 发 0x71 读 1 字节状态字，0x18 = 已校准且空闲；
 *   5. 若应答正常，触发 0xAC 测量，等待 80ms，轮询状态字 Bit[7]，
 *      连续读 6 字节并换算打印温湿度，校验数值范围。
 *
 * 实现特点：
 *   - 完全自包含，只复用 iic.c 中已在 iic.h 声明的原语
 *     （IICInit/IICStart/IICSendByte/IICReceiveByte/IICSendAck/IICSendNotAck/IICStop）；
 *   - ACK 等待在文件内自行实现并加了 10us 稳定延时，
 *     规避了 iic.c 中 IICWaitAck "SCL 拉高立即采样"导致的假 ACK 误判；
 *   - 依赖 timer_2_init()（delay_us/delay_ms），请在 TIM2 初始化之后再调用。
 *
 * @return int:
 *   0  = 检测通过（有应答 + 读到有效温湿度）
 *   1  = 无应答 NACK（传感器不在 PB13/PB14 上：供电/接线/上拉问题）
 *   2  = 有应答但状态字/数据全 0xFF（芯片可能不是 AHT21，如 AHT30 同地址 0x38 但协议不同）
 *   3  = 测量失败/超时，或读到的温湿度超出合理范围
 */
int aht21_hw_test(void);

uint8_t aht21_test2(iic_bus_t AHT_bus);

#endif /* AHT21_TEST_H */
