//
// Created by 文武 on 2026/6/15.
//

#include "mid_circular_buffer.h"


circular_buffer_t* create_empty_circular_buffer(void) {
    circular_buffer_t* p_buffer_temp = NULL;

    // 1. alloct the memory space
    p_buffer_temp = \
        (circular_buffer_t*) malloc(sizeof(circular_buffer_t));

    if (p_buffer_temp == NULL) {
        log_e("error: crete empty circular buffer failed");
        return NULL;
    }

    // 2. memory init
    memset(p_buffer_temp, 0, sizeof(circular_buffer_t));

    return p_buffer_temp;
}

uint8_t ring_is_empty(circular_buffer_t* p_buffer) {
    if (p_buffer == NULL) {
        log_e("error: circular buffer is NULL");
        return 0xFF;
    }

    if (p_buffer->head == p_buffer->tail) {
        return 0x01;
    } else {
        return 0x00;
    }
}

// 这个判满函数需要注意，在判满时是 （head + 1）% CIRCULAR_BUFFER_SIZE ==  tail % CIRCULAR_BUFFER_SIZE
// 也就是此环形缓冲区是舍弃了一个可以写入的空间来便于判断缓冲区是否已满
// 例如：ring 共有 10个位置，head 指向 9 时表示 9 还未写入，但是在判空的时候 head + 1 = 10，
// 代入判满公示，则 head + 1 = 10， 10 % 10 = 0， 0 == 0， 满足判满条件。
// 所以 head 指向的位置永远不会被写入，即牺牲了一个可以写入的空间来判断缓冲区是否已满
uint8_t ring_is_full(circular_buffer_t* p_buffer) {
    if (p_buffer == NULL) {
        log_e("error: circular buffer is NULL");
        return 0xFF;
    }

    // tail 指向的是有效数据，所以 head 需要加 1
    if ( ((p_buffer->head + 1) % CIRCULAR_BUFFER_SIZE )
            == (p_buffer->tail % CIRCULAR_BUFFER_SIZE) ) {
        return 0x01;
    }

    return 0x00;
}

uint8_t ring_push_head(circular_buffer_t* p_buffer, data_type_t new_data) {
    if (p_buffer == NULL) {
        log_e("error: circular buffer is NULL");
        return 0xFF;
    } else if (ring_is_full(p_buffer) == 0x01) {
        log_e("error: circular buffer is full");
        return 0xFE;
    }

    p_buffer->data[p_buffer->head] = new_data;
    p_buffer->head = (p_buffer->head + 1) % CIRCULAR_BUFFER_SIZE;
    return 0x00;
}

uint8_t ring_pop_tail(circular_buffer_t* p_buffer, data_type_t* data) {
    if (p_buffer == NULL) {
        log_e("error: circular buffer is NULL");
        return 0xFF;
    } else if (ring_is_empty(p_buffer) == 0x01) {
        log_e("error: circular buffer is empty");
        return 0xFE;
    }

    *data = p_buffer->data[p_buffer->tail];
    p_buffer->tail = (p_buffer->tail + 1) % CIRCULAR_BUFFER_SIZE;
    return 0x00;
}

uint8_t get_head_pos(circular_buffer_t* p_buffer, uint32_t* head_pos) {
    if (p_buffer == NULL) {
        return 0xFF;
    }
    *head_pos = p_buffer->head;
    return 0x00;
}

uint8_t head_pos_increment(circular_buffer_t * p_buffer,
    uint32_t increment_num) {

    if (p_buffer == NULL) {
        return 0xFF;
    }

    uint32_t space = 0;
    if (p_buffer->head >= p_buffer->tail)
    {
        // 环形缓冲区是牺牲了一个位置来判断缓冲区是否已满
        space = CIRCULAR_BUFFER_SIZE - (p_buffer->head - p_buffer->tail) - 1;
    } else
    {
        // 环形缓冲区是牺牲了一个位置来判断缓冲区是否已满
        space = p_buffer->tail - p_buffer->head - 1;
    }

    if (increment_num > space)
    {
        log_e("circular buffer increment number > space, overwrite begin!!!\r\n");
        p_buffer->head = (p_buffer->head + increment_num) % CIRCULAR_BUFFER_SIZE;
        p_buffer->tail = (p_buffer->head + 1) % CIRCULAR_BUFFER_SIZE;
        return 0xFE;
    }

    // p_buffer->head += increment_num;
    p_buffer->head = (p_buffer->head + increment_num) % CIRCULAR_BUFFER_SIZE;
    return 0x00;
}

