//
// Created by 文武 on 2026/6/16.
//

#include "usart_parse.h"

// 消息队列：用于通知 处理来自串口数据的 线程
QueueHandle_t usart1_parse_queue = NULL;

// 环形缓冲区指针
static circular_buffer_t * g_ring_buffer_from_driver = NULL;
TaskHandle_t usart1_parse_handle = NULL;
void usart1_parse_task(void * arg) {
	log_i("usart1_parse_task start");

    usart1_parse_queue = xQueueCreate(1, 4);
    if (usart1_parse_queue == NULL) {
        log_e("usart1_parse_queue create fail");
    } else {
        log_i(
        "usart1_parse_queue create success, usart1_parse_queue: [%x]",
        usart1_parse_queue);
    }

	while (1) {
		uint32_t receive = 0;
		xQueueReceive(usart1_parse_queue, &receive, portMAX_DELAY);
		log_i("end thread receive [0x%x] from front thread", receive);

		if (g_ring_buffer_from_driver == NULL) {
			g_ring_buffer_from_driver = get_ring_buffer_point();
			if (g_ring_buffer_from_driver == NULL) {
				log_e("g_ring_buffer_from_driver is NULL");
				return;
			}
		}
		while (ring_is_empty(g_ring_buffer_from_driver) == 0x00) {
			// 缓冲区 非空，开始解析
			static uint8_t temp_data_array[20] = {0x00};
			static uint8_t data_counter = 0;
			uint8_t temp_data = 0;
			if (0x00 == ring_pop_tail(g_ring_buffer_from_driver, &temp_data)) {
				log_i("buffer read out from APP = [%d]", temp_data);
			}

			// 状态机进行解析数据
			static uint32_t status = FRAME_NOT_DETECTED;
			switch(status) {
				case FRAME_NOT_DETECTED:
					if (temp_data == FRAME_HEAD_FLAG) {
						log_i("data frame start");
						status = FRAME_HEAD;
					}
					break;
				case FRAME_HEAD:
					if (temp_data == FRAME_TAIL_FLAG) {
						log_i("data frame end");
						status = FRAME_NOT_DETECTED;
						// 计算校验和
						uint32_t data_sum = temp_data_array[data_counter - 1];
						uint32_t data_sum_temp = 0;
						for (int i = 0; i < (data_counter - 1); i++) {
							data_sum_temp += temp_data_array[i];
						}
						log_i("calculated data_sum = [%d]", data_sum_temp);
						if (data_sum == data_sum_temp) {
							log_i("data sum check ok, receive data:");
							for (int i = 0; i < (data_counter - 1); i++) {
								log_i("[%d]", temp_data_array[i]);
								temp_data_array[i]  = 0x00;
							}
							temp_data_array[data_counter - 1] = 0x00; // 清除最后一个校验和数据
						} else {
							log_e("data sum check error:");
							log_i("receive sum = [%d] calculated sum = [%d]", data_sum, data_sum_temp);
							for (int i = 0; i <= (data_counter - 1); i++) {
								temp_data_array[i]  = 0x00;
							}
						}
						data_counter = 0;
						status = FRAME_NOT_DETECTED;
					} else {
						// 净荷数据
						// log_i("FRAME_HEAD data = [%d]", temp_data);
						// 数据暂存
						temp_data_array[data_counter] = temp_data;
						data_counter++;
						// log_i("data_counter = [%d]", data_counter);
						// log_i("data = [%d]", temp_data);
					}
					break;
			}
		}
		vTaskDelay(1);
	}
}
