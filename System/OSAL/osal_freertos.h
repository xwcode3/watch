#ifndef OSAL_FREERTOS_H
#define OSAL_FREERTOS_H

#include "osal.h"

extern const osal_delay_t osal_delay_freertos;
extern const osal_time_base_t osal_time_base_freertos;
extern const osal_queue_t osal_queue_freertos;
extern const osal_mutex_t osal_mutex_freertos;
extern const osal_sem_t osal_sem_freertos;
extern const osal_sem_counting_t osal_sem_counting_freertos;
extern const osal_notify_t osal_notify_freertos;
extern const osal_critical_t osal_critical_freertos;
extern const osal_thread_t osal_thread_freertos;
extern const osal_yield_t osal_yield_freertos;
extern const osal_loop_delay_t osal_loop_delay_freertos;

#endif /* OSAL_FREERTOS_H */
