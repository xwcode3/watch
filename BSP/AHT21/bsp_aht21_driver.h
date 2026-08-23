#pragma once
#ifndef BSP_AHT21_DRIVER_H
#define BSP_AHT21_DRIVER_H

#include <stdio.h>
#include <stdint.h>
#include "stm32f4xx.h"
#include "bsp_config.h"
#include "osal.h"

#include "elog.h"
#include "iic.h"

/* The IIC command of AHT21 */
#define AHT21_REG_READ_ADDR             0x71
#define AHT21_REG_WRITE_ADDR            0x70
#define AHT21_REG_MEASURE_CMD           0xAC
#define AHT21_REG_MEASURE_CMD_ARGS1     0x33
#define AHT21_REG_MEASURE_CMD_ARGS2     0x00
#define AHT21_REG_RESET_CMD             0xBE
#define AHT21_REG_RESET_CMD_ARGS1       0x08
#define AHT21_REG_RESET_CMD_ARGS2       0x00


/* return values from functions */
typedef enum {
  AHT21_OK                  = 0,                /* Operation completed successfully     */
  AHT21_ERROR               = 1,                /* Run-time error without case matched  */
  AHT21_ERRORTIMEOUT        = 2,                /* Operation failed with timeout        */
  AHT21_ERRORRESOURCE       = 3,                /* Resource not available               */
  AHT21_ERRORPARAMETER      = 4,                /* Parameter error                      */
  AHT21_ERRORNOMEMORY       = 5,                /* Out of memory                        */
  AHT21_ERRORISR            = 6,                /* Not allowed in ISR context           */
  AHT21_RESERVED            = 0x7FFFFFFF        /* Reserved                             */
} aht21_status_t;


/*********************** Declaring ****************************/
#ifndef HARDWARE_IIC        // software iic
typedef struct {
    iic_bus_t * iic_bus;
    void (*pf_iic_init)           (iic_bus_t *bus);
    void (*pf_iic_deinit)         (iic_bus_t *bus);
    void (*pf_iic_start)          (iic_bus_t *bus);
    void (*pf_iic_stop)           (iic_bus_t *bus);
    uint8_t (*pf_iic_wait_ack)    (iic_bus_t *bus);
    void (*pf_iic_send_ack)       (iic_bus_t *bus);
    void (*pf_iic_send_no_ack)    (iic_bus_t *bus);
    void (*pf_iic_send_byte)      (iic_bus_t *bus, unsigned char cSendByte);
    void (*pf_iic_receive_byte)   (iic_bus_t *bus, unsigned char *receiveByte);

    // osal_status_t (*pf_critical_enter)     (void);
    // osal_status_t (*pf_critical_exit)      (void);
    osal_critical_t *           pf_critical_interface; 
} iic_driver_interface_t;
#endif /* end of HARDWARE_IIC */

#ifdef HARDWARE_IIC      // hardware iic
typedef struct {
    aht21_status_t (*pf_iic_init)           (void *);
    aht21_status_t (*pf_iic_deinit)         (void *);
    aht21_status_t (*pf_iic_send_ack)       (void *);
    aht21_status_t (*pf_iic_send_no_ack)    (void *, uint8_t);
    aht21_status_t (*pf_iic_send_byte)      (void *, uint8_t);
    aht21_status_t (*pf_iic_receive_byte)   (void *);
} iic_driver_interface_t;   
#endif /* end of HARDWARE_IIC */


/* AHT21 Driver 实例对象结构体 */
typedef struct {
    iic_driver_interface_t              *p_iic_driver_instance;
    osal_time_base_t                      *p_timebase_instance;

#ifdef OS_SUPPORTING
    osal_delay_t                             *p_delay_instance;
#endif /* end of OS_SUPPORTING */

    aht21_status_t (*aht21_instantiate) (
        void * const                          p_aht21_instance,
        iic_driver_interface_t * const   p_iic_driver_instance,
        osal_time_base_t * const           p_timebase_instance,
    
#ifdef OS_SUPPORTING
        osal_delay_t * const                  p_delay_instance
#endif /* end of OS_SUPPORTING */
    );

    aht21_status_t (*pf_init)              (void * const);  /* 初始化 */
    aht21_status_t (*pf_deinit)            (void * const); /* 反初始化 */
    aht21_status_t (*pf_read_id)           (void * const); /* 读取ID */
    /* 读取温湿度数据 */
    aht21_status_t (*pf_read_data)         (void * const, float * const, float * const);
    aht21_status_t (*pf_read_temperature)  (void * const, float * const); /* 读取温度数据 */
    aht21_status_t (*pf_read_humidity)     (void * const, float * const); /* 读取湿度数据 */
    aht21_status_t (*pf_sleep)             (void * const); /* 进入睡眠模式 */
    aht21_status_t (*pf_wakeup)            (void * const); /* 唤醒 */
} bsp_aht21_driver_t;

/* AHT21 driver instance class constructor function */
aht21_status_t aht21_instantiate (
    bsp_aht21_driver_t * const             p_aht21_instance,
    iic_driver_interface_t * const         p_iic_driver_instance,
    
#ifdef OS_SUPPORTING
        osal_delay_t * const                  p_delay_instance,
#endif /* end of OS_SUPPORTING */

    osal_time_base_t * const               p_timebase_instance
);

aht21_status_t aht21_read_data(
    bsp_aht21_driver_t * const p_aht21_instance,
    float * const p_temperature,
    float * const p_humidity);

#endif // BSP_AHT21_DRIVER_H