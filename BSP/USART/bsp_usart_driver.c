//
// Created by 文武 on 2026/6/14.
//

#include "bsp_usart_driver.h"

// #define USART_BUFFER_A 0
// #define USART_BUFFER_B 1

#define IRQ_TO_THREAD 0xA1A2A3A4
#define FRONT_SEND_TO_END  0xB1B3B4

// .bss 用于 在中断 通知 usart驱动线程的 消息队列
static QueueHandle_t usart_driver_irq_queue = NULL;
// 用于通知解包线程的 队列
extern QueueHandle_t usart1_parse_queue;

#if 1 // circular buffer
    uint8_t g_data_buffer = 0;
#endif // end of circular buffer

// .bss 指向环形缓冲区的结构体指针
static circular_buffer_t * g_circular_buffer_irq_thread = NULL;

TaskHandle_t usart_driver_handle = NULL;
void uart_driver_func(void * arg) {


    log_i("front thread start");

    circular_buffer_t* p_buffer = create_empty_circular_buffer();
    if (p_buffer == NULL) {
        log_e("create_empty_circular_buffer fail");
    } else {
        log_i("create_empty_circular_buffer success\r\n");
    }

    // mount the global pointer to circular_buffer
    g_circular_buffer_irq_thread = p_buffer;

    if (g_circular_buffer_irq_thread == NULL) {
        log_e("g_circular_buffer_irq_thread is NULL");
        return;
    }

    USART1_DMA2_Stream2_Init(g_circular_buffer_irq_thread->data, CIRCULAR_BUFFER_SIZE);


    usart_driver_irq_queue = xQueueCreate(1, sizeof(uint32_t));
    if (usart_driver_irq_queue == NULL) {
        log_e("usart_driver_irq_queue creates fail");
        return;
    }
    log_i("usart_driver_irq_queue creates success\r\n");

    log_i("Send HEX data without newline");
    log_i("Frame format: Header(0xFE) + Data + Checksum(sum of data) + Tail(0xFF)");

    uint32_t receive_data = 0;

    while (1) {
        // 接收来自中断的消息，说明有数据到达
        xQueueReceive(usart_driver_irq_queue, &receive_data, portMAX_DELAY);
        log_i("bsp usart receive data: [0x%x] from irq", receive_data);

        if (receive_data == IRQ_TO_THREAD) {
            uint32_t send_to_end = FRONT_SEND_TO_END;
            BaseType_t ret_queue = pdTRUE;
            // queueOVERWRITE表示覆写队列中的数据，如果队列为空，则直接覆盖
            ret_queue = xQueueGenericSend(usart1_parse_queue, &send_to_end, 0, queueOVERWRITE);
            if (ret_queue != pdTRUE) {
                log_e("bsp usart front send to end fail");
                return;
            }
            log_i("bsp usart front send to end success");
        }
        // vTaskDelay(5000);
    }
}


circular_buffer_t * get_ring_buffer_point(void) {
    if (g_circular_buffer_irq_thread == NULL) {
        log_e("g_circular_buffer_irq_thread is NULL");
        return NULL;
    }

    return g_circular_buffer_irq_thread;
}


void dma_half_irq_callback(uint32_t number_of_data) {
    uint32_t head_pos = 0;
    uint8_t ret = 0;
    circular_buffer_t* circular_buffer = get_ring_buffer_point();
    ret = get_head_pos(circular_buffer, &head_pos);
    if (ret != 0x00) {
        log_e("get head pos error");
    }

    // log_i("head pos = [%d]", head_pos);

    // 2. 获取进入半满中断时，数据已经到达的位置： （CIRCULAR_BUFFER_SIZE/2)-1
    // uint32_t current_data_pos = (CIRCULAR_BUFFER_SIZE / 2) - 1;
    uint32_t current_data_pos = (CIRCULAR_BUFFER_SIZE / 2);
    // log_i("current data pos = [%d]", current_data_pos);

    // 3. 对head进行取余数
    uint32_t pos_in_buffer = head_pos % (CIRCULAR_BUFFER_SIZE / 2);


    // 4. 计算出当前偏移的数量
    uint32_t move_pos = 0x00;
    if (current_data_pos < pos_in_buffer) {
        move_pos = (current_data_pos + CIRCULAR_BUFFER_SIZE) - pos_in_buffer;
    } else {
        move_pos = current_data_pos - pos_in_buffer;
    }
    log_i("half move pos = [%d]", move_pos);

    // 5. 对head位置进行累加
    head_pos_increment(circular_buffer, move_pos);

    log_i("half_back head position: %d", circular_buffer->head);

    // 6. 对前端线程进行通知
    // queueOVERWRITE表示覆写队列中的数据，如果队列为空，则直接覆盖
    BaseType_t ret_queue = pdTRUE;
    uint32_t irq_to_front = IRQ_TO_THREAD;
    ret_queue = xQueueGenericSendFromISR(usart_driver_irq_queue, &irq_to_front, NULL, queueOVERWRITE);
    if (ret_queue != pdTRUE) {
        log_e("front send to end fail");
        return;
    }
}

void dma_comp_irq_callback(uint32_t number_of_data) {
    uint32_t head_pos = 0;
    uint8_t ret = 0;
    circular_buffer_t* circular_buffer = get_ring_buffer_point();
    ret = get_head_pos(circular_buffer, &head_pos);
    if (ret != 0x00) {
        log_e("get head pos error");
    }

    // log_i("[c] head pos = [%d]", head_pos);

    // 2. 获取进入全满中断时，数据已经到达的位置： CIRCULAR_BUFFER_SIZE - 1
    // uint32_t current_data_pos = CIRCULAR_BUFFER_SIZE - 1;
    uint32_t current_data_pos = CIRCULAR_BUFFER_SIZE;
    // log_i("[c] current data pos = [%d]", current_data_pos);

    // 3. 对heap进行取余数
    uint32_t pos_in_buffer = head_pos % CIRCULAR_BUFFER_SIZE;
    // log_i("[c] pos in buffer = [%d]", pos_in_buffer);

    // 4. 计算出当前偏移的数量
    uint32_t move_pos = 0x00;
    if (current_data_pos < pos_in_buffer) {
        move_pos = (current_data_pos + CIRCULAR_BUFFER_SIZE) - pos_in_buffer;
    } else {
        move_pos = current_data_pos - pos_in_buffer;
    }
    log_i("full move pos = [%d]", move_pos);


    // 5. 对head位置进行累加
    head_pos_increment(circular_buffer, move_pos);
    log_i("comp_back head position: %d", circular_buffer->head);

    // 6. 对前端线程进行通知
    // queueOVERWRITE表示覆写队列中的数据，如果队列为空，则直接覆盖
    BaseType_t ret_queue = pdTRUE;
    uint32_t irq_to_front = IRQ_TO_THREAD;
    ret_queue = xQueueGenericSendFromISR(usart_driver_irq_queue, &irq_to_front, NULL, queueOVERWRITE);
    if (ret_queue != pdTRUE) {
        log_e("front send to end fail");
        return;
    }
}

void uart_idle_irq_callback(uint32_t number_of_data) {
    uint32_t head_pos = 0;
    uint8_t ret = 0;
    circular_buffer_t* circular_buffer = get_ring_buffer_point();
    ret = get_head_pos(circular_buffer, &head_pos);
    if (ret != 0x00) {
        log_e("get head pos error");
    }

    // log_d("[U] head pos = [%d]", head_pos);

    // 2. 获取进入空闲中断时，数据已经到达的位置： CIRCULAR_BUFFER_SIZE - 1
    // uint32_t current_data_pos = number_of_data - 1;
    uint32_t current_data_pos = number_of_data;
    // log_d("[U] current data pos = [%d]", current_data_pos);

    // 3. 对head进行取余数
    uint32_t pos_in_buffer = head_pos % CIRCULAR_BUFFER_SIZE;
    // log_d("[U] pos in buffer = [%d]", pos_in_buffer);

    // 4. 计算出当前偏移的数量
    uint32_t move_pos = 0x00;
    if (current_data_pos < pos_in_buffer) {
        move_pos = (current_data_pos + CIRCULAR_BUFFER_SIZE) - pos_in_buffer;
        // log_d("[U] move pos dma complete = [%d]", move_pos);
    } else {
        move_pos = current_data_pos - pos_in_buffer;
        // log_d("[U]move_pos_normal = [%d]",move_pos);
    }
    log_i("idle move pos = [%d]", move_pos);

    // 5. 对head位置进行累加
    head_pos_increment(circular_buffer, move_pos);
    log_i("idle_back head position: %d", circular_buffer->head);

    // 6. 对前端线程进行通知
    // queueOVERWRITE表示覆写队列中的数据，如果队列为空，则直接覆盖
    BaseType_t ret_queue = pdTRUE;
    uint32_t irq_to_front = IRQ_TO_THREAD;
    ret_queue = xQueueGenericSendFromISR(usart_driver_irq_queue, &irq_to_front, NULL, queueOVERWRITE);
    if (ret_queue != pdTRUE) {
        log_e("front send to end fail");
        return;
    }
}
