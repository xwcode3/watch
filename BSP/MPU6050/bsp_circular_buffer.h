#pragma once
#ifndef BSP_CIRCULAR_BUFFER_H
#define BSP_CIRCULAR_BUFFER_H

#include <stdint.h>
#include "elog.h"
#include "bsp_config.h"

// MPU6050数据包大小定义
#define MPU6050_DATA_PACKET_SIZE    14  // 加速度(6) + 温度(2) + 陀螺仪(6) = 14字节

typedef struct circular_buffer
{
    uint8_t *buffer; // 缓冲区
    uint8_t rflag;   // 读位置
    uint8_t wflag;   // 写位置
    uint8_t *(*pf_get_wbuffer_addr)(struct circular_buffer *); // 获取写缓冲区地址
    uint8_t *(*pf_get_rbuffer_addr)(struct circular_buffer *); // 获取读缓冲区地址
    void (*pf_data_writed)(struct circular_buffer *);          // 写数据
    void (*pf_data_readed)(struct circular_buffer *);          // 读数据
    uint8_t size;    // 缓冲区槽位数量
} mpu_circular_buffer_t;

void buffer_init(mpu_circular_buffer_t *buffer, uint8_t size);

#endif //BSP_CIRCULAR_BUFFER_H