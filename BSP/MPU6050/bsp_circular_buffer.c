#include "bsp_circular_buffer.h"
#include <string.h>
#include <stdlib.h>

mpu_circular_buffer_t circular_buf;

uint8_t *get_wbuffer_addr(mpu_circular_buffer_t *buffer)
{
    return buffer->buffer + buffer->wflag * MPU6050_DATA_PACKET_SIZE;
}

uint8_t *get_rbuffer_addr(mpu_circular_buffer_t *buffer)
{
    return buffer->buffer + buffer->rflag * MPU6050_DATA_PACKET_SIZE;
}

void data_writed(mpu_circular_buffer_t *buffer)
{
    // DMA写数据结束
    // todo:buffer已满
    buffer->wflag = (buffer->wflag + 1) % buffer->size;
}

void data_readed(mpu_circular_buffer_t *buffer)
{
    // 读取数据结束
    // todo: 没有可读数据
    buffer->rflag = (buffer->rflag + 1) % buffer->size;
}

void buffer_init(mpu_circular_buffer_t *buffer, uint8_t size)
{
    if (NULL == buffer)
    {
        DEBUG_OUT("buffer is NULL");
    }

    buffer->size = size;  // 槽位数量
    buffer->rflag = 0;
    buffer->wflag = 0;

    /*buffer 分配空间: 槽位数量 × 每个槽位的数据包大小*/
    buffer->buffer = (uint8_t *)malloc(size * MPU6050_DATA_PACKET_SIZE);

    buffer->pf_get_rbuffer_addr = get_rbuffer_addr;
    buffer->pf_get_wbuffer_addr = get_wbuffer_addr;
    buffer->pf_data_readed      = data_readed;
    buffer->pf_data_writed      = data_writed;
}