#pragma once
#ifndef BSP_MPU6050_HANDLER_H
#define BSP_MPU6050_HANDLER_H

#include "bsp_mpu6050_driver.h"


typedef struct {
    mpu6050_iic_driver_interface_t * p_iic_driver;
    osal_time_base_t * p_time_base;
    osal_delay_t * p_delay;
    osal_loop_delay_t * p_loop_delay;
    osal_queue_t * p_queue;
    osal_sem_t * p_semaphore;
    osal_notify_t * p_notify;
    osal_mutex_t * p_mutex;
    osal_thread_t * p_thread;
    osal_critical_t * p_critical;
} mpuxxx_handler_input_args_t;

/* handler structure */
typedef struct {
    mpuxxx_handler_input_args_t * p_input_args;
    bsp_mpuxxx_driver_t * p_driver;
    void * p_queue_handle;
    void * p_unpack_queue_handle;

    uint32_t queue_item_size;
    uint32_t queue_length;

    void *mutex_handle;
    void *semaphore_binary_handle;
    void *notify_handle;
} bsp_mpuxxx_handler_t;


uint8_t mpuxxx_handler_inst(
        bsp_mpuxxx_handler_t *p_handler, 
        mpuxxx_handler_input_args_t *p_input_args );

/**
 * @brief The thread function for mpuxxx handler
 * 
 * @param argument: The argument
 */
void mpuxxx_handler_thread(void *argument);




#endif /* end of bsp_mpu6050_handler_h */