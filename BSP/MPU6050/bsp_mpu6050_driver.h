#pragma once
#ifndef BSP_MPU6050_DRIVER_H
#define BSP_MPU6050_DRIVER_H

#include <stdint.h>
#include <stdio.h>

#include "elog.h"
#include "bsp_config.h"
#include "osal.h"

#include "bsp_mpu6050_reg.h"
#include "bsp_mpu6050_reg_bit.h"
#include "bsp_circular_buffer.h"

extern mpu_circular_buffer_t circular_buf;

typedef struct {
    void * p_iic;

    void (*pf_iic_init)      (void);           // IIC初始化函数
    void (*pf_iic_deinit)    (void);           // IIC deinit函数
    int8_t (*pf_iic_mem_write) (
        void * p_iic,
        uint16_t dst_addr,
        uint16_t mem_addr,
        uint16_t mem_size,
        uint8_t * p_data,
        uint16_t data_size,
        uint32_t timeout_ms
    );

    int8_t (*pf_iic_mem_read) (
        void * p_iic,
        uint16_t src_addr,
        uint16_t mem_addr,
        uint16_t mem_size,
        uint8_t * p_data,
        uint16_t data_size,
        uint32_t timeout_ms
    );

    // Use DMA to asynchronously read data from IIC device.
    int8_t (*pf_iic_mem_read_dma) (
        void * p_iic,
        uint16_t src_addr,
        uint16_t mem_addr,
        uint16_t mem_size,
        uint8_t * p_data,
        uint16_t data_size
    );
} mpu6050_iic_driver_interface_t;

/* Interrupt of hardware */
typedef struct {
    uint8_t (*pf_it_init) (void);
    uint8_t (*pf_it_deinit) (void);
    uint8_t (*pf_it_enable) (void);
    uint8_t (*pf_it_disable) (void);
    uint8_t (*pf_it_clock_enable) (void);
    uint8_t (*pf_it_clock_disable) (void);
} mpu6050_hardware_interrupt_interface_t;



/* Store data from mpuxxx driver */
typedef struct {
    uint8_t * (*pf_buffer_init) (uint8_t size);
    uint8_t * (*pf_get_rbuffer_addr) (void); 
    uint8_t * (*pf_get_wbuffer_addr) (void);
} buffer_interface_t;



/* mpu6050 data format */
typedef struct {
    /* raw accelerometer data from sensor */
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;

    /* processed accelerometer data in g units */
    double ax;
    double ay;
    double az;

    /* raw gyroscope data from sensor */
    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;

    /* processed gyroscope data in degrees per second (deg/s) */
    double gx;
    double gy;
    double gz;

    /* temperature reading in degress celsius (°C) */
    float temperature;

    /* kalman filter processed angles */
    double kalman_angle_x;
    double kalman_angle_y;
} mpuxxx_data_t;


/* Driver structure */
typedef struct bsp_mpuxxx_driver {
    /* core layer */
    mpu6050_iic_driver_interface_t * p_iic_driver_interface;
    mpu6050_hardware_interrupt_interface_t * p_hardware_interrupt_interface;


    /* os layer */
#ifdef OS_SUPPORTING

    osal_delay_t * p_delay_interface;
    osal_loop_delay_t * p_loop_delay_interface;
    osal_thread_t * p_thread_interface;
    osal_queue_t * p_queue_interface;
    osal_mutex_t * p_mutex_interface;
    osal_sem_t * p_semaphore_interface;
    osal_notify_t * p_notify_interface;
    osal_critical_t * p_critical_interface;

    osal_time_base_t * p_time_base_interface;

    void * queue_handle;
    void * semaphore_mutex_handle;
    void * semaphore_binary_handle;
    void * notify_handle;

    void (*pf_dma_completed_callback) (void);
    void (*pf_int_interrupt_callback) (void);
#endif /* end of OS_SUPPORTING */


    /* interface of mpuxxx driver */
    uint8_t (*pf_deinit) (struct bsp_mpuxxx_driver *);
    uint8_t (*pf_sleep) (struct bsp_mpuxxx_driver *);
    uint8_t (*pf_wakeup) (struct bsp_mpuxxx_driver *);

    /* set full scale range */
    uint8_t (*pf_set_gyro_fsr) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_accel_fsr) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_lpf) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_rate) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_interrupt_enable) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_motion_threshold) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_INT_level) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_user_ctrl) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_pwr_mgmt1_reg) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_pwr_mgmt2_reg) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_set_fifo_en_reg) (struct bsp_mpuxxx_driver *, uint8_t);
    uint8_t (*pf_get_temperature) (struct bsp_mpuxxx_driver *, mpuxxx_data_t *);
    uint8_t (*pf_get_accel) (struct bsp_mpuxxx_driver *, mpuxxx_data_t *);
    uint8_t (*pf_get_gyro) (struct bsp_mpuxxx_driver *, mpuxxx_data_t *);
    uint8_t (*pf_get_all_data) (struct bsp_mpuxxx_driver *, mpuxxx_data_t *);
    uint8_t (*pf_get_interrupt_status_reg) (struct bsp_mpuxxx_driver *, uint8_t *);
    uint8_t (*pf_read_fifo_packet) (struct bsp_mpuxxx_driver *, mpuxxx_data_t *);
    uint8_t (*pf_read_fifo_isr_occur) (struct bsp_mpuxxx_driver *, mpuxxx_data_t *);
} bsp_mpuxxx_driver_t;



uint8_t bsp_mpuxxx_driver_inst(
    bsp_mpuxxx_driver_t * const p_mpuxxx_driver,
    mpu6050_iic_driver_interface_t * const p_iic_driver_interface

#ifdef OS_SUPPORTING
,
    osal_delay_t * const p_delay_interface,
    osal_loop_delay_t * const p_loop_delay_interface,
    osal_thread_t * const p_thread_interface,
    osal_queue_t * const p_queue_interface,
    osal_mutex_t * const p_mutex_interface,
    osal_sem_t * const p_semaphore_interface,
    osal_notify_t * const p_notify_interface,
    osal_critical_t * const p_critical_interface,
    osal_time_base_t * const p_time_base_interface,
    void (*callback_register) (void (*callback)(void *, void *)),
    void (*callback_register_dma) (void (*callback)(void *, void *)),
    void * queue_handle,
    void * mutex_handle,
    void * semaphore_handle,
    void * notify_handle
#endif /* end of OS_SUPPORTING */

);

uint32_t mpuxxx_flag_read(void);
void mpuxxx_flag_set(uint8_t flag);


#endif /* end of bsp_mpu6050_driver.h */