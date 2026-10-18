#ifndef __QUEUE_STUB_H
#define __QUEUE_STUB_H
#include "FreeRTOS.h"
typedef void* QueueHandle_t;
QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemsize);
BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t t);
BaseType_t xQueueReceive(QueueHandle_t q, void* out, TickType_t t);
#endif
