//
// Created by 文武 on 2026/6/15.
//

#ifndef WATCH_MID_CIRCULAR_BUFFER_H
#define WATCH_MID_CIRCULAR_BUFFER_H


#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "elog.h"

#define CIRCULAR_BUFFER_SIZE 10

typedef uint8_t data_type_t;

// 环形缓冲区结构体
typedef struct {
    data_type_t data[CIRCULAR_BUFFER_SIZE]; // 缓冲区数据
    uint32_t  head; // 缓冲区头部，head 指向的是下一个可写入的位置
    uint32_t  tail; // 缓冲区尾部，tail 指向的是有效数据
} circular_buffer_t;

/**
 * @brief   创建一个空的环形缓冲区
 * @param   void
 * @retval  circular_buffer_t*：返回环形缓冲区的指针
 */
circular_buffer_t* create_empty_circular_buffer(void);

/**
 * @brief   判断环形缓冲区是否为空
 * @param   circular_buffer_t*：环形缓冲区的指针
 * @retval  0xFF：error, 缓冲区为NULL 不合法;
 *          0x01：缓冲区为空；
 *          0x00：缓冲区不为空
 */
uint8_t ring_is_empty(circular_buffer_t*);

/**
 * @brief   判断环形缓冲区是否已满
 * @param   circular_buffer_t*：环形缓冲区的指针
 * @retval  0xFF：error, 缓冲区为NULL 不合法;
 *          0x01：缓冲区已满；
 *          0x00：缓冲区未满
 */
uint8_t ring_is_full(circular_buffer_t*);

/**
 * @brief   在环形缓冲区头部(head)写入数据
 * @param   circular_buffer_t*：环形缓冲区的指针；
 *          data_type_t：要添加的数据
 * @retval  0xFF：error, 缓冲区为NULL 不合法;
 *          0xFE：缓冲区已满；
 *          0x00：写入成功
 */
uint8_t ring_push_head(circular_buffer_t*, data_type_t);

/**
 * @brief   在环形缓冲区尾部(tail)读取数据，并删除该数据
 * @param   circular_buffer_t*：返回环形缓冲区的指针；
 *          data_type_t*：指向从缓冲区取出的数据的指针
 * @retval  0xFF：error, 缓冲区为NULL 不合法;
 *          0xFE：缓冲区为空；
 *          0x00：读取数据成功，并且在将数据从缓冲区删除
 */
uint8_t ring_pop_tail(circular_buffer_t*, data_type_t*);

/**
 * @brief get_the_head_pos.
 * @param[in] circular_buffer_t : Pointer to the target of handler.
 * @param[in] head : Pointer to the head storage varibale.
 * @return      uint8_t :
                        0xff:error, the buffer pointer is NULL;
                        0xfe:error, the buffer is empty;
                        0x00:success
                        0x01:failed
 * */
uint8_t get_head_pos(circular_buffer_t* p_buffer, uint32_t* head_pos);


/**
 * @brief head pos. increment.
 * @param[in] circular_buffer_t : Pointer to the target of handler.
 * @param[in] increament_num : increment number.
 * @return      uint8_t :
                        0xff:error, the buffer pointer is NULL;
                        0xfe:error, the buffer is full and overwrite;
                        0x00:success
                        0x01:failed
 * */
uint8_t head_pos_increment(circular_buffer_t * p_buffer, uint32_t increment_num);

#endif //WATCH_MID_CIRCULAR_BUFFER_H