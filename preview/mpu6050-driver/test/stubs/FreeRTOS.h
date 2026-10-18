#ifndef __FREERTOS_STUB_H
#define __FREERTOS_STUB_H
#include <stdint.h>
typedef uint32_t TickType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
#define pdTRUE  ((BaseType_t)1)
#define pdFALSE ((BaseType_t)0)
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY ((TickType_t)0xFFFFFFFFu)
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount(void);
#endif
