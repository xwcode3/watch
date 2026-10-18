#ifndef __TASK_STUB_H
#define __TASK_STUB_H
#include "FreeRTOS.h"
BaseType_t xTaskCreate(void(*fn)(void*), const char* name, uint16_t stack, void* arg, UBaseType_t prio, void** handle);
#endif
