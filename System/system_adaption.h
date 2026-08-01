#ifndef __SYSTEM_ADAPTION_H__
#define __SYSTEM_ADAPTION_H__ 

//1.Compiling system standard head file
#include <stdio.h>
#include <stdint.h>

//2.MCU layer head file
//2.1 CPU driver
#include "stm32f4xx.h"    //ARM provided 
//2.2 Core
#include "main.h"
#include "usart.h"
#include "gpio.h"

//3.OS layer head file
#include "queue.h"
#include "task.h"

//4.BSP layer head file
#include "bsp_led_driver.h"
#include "bsp_led_handler.h"

#define OS_SUPPORTING
#define HANLDER_1_DEBUG
// #define INIT_PATTERN_SYSTEM (uint8_t)0xEC

typedef enum {
	SYSTEM_OK             = 0,      /* LED Operation completed successfully  */
	SYSTEM_ERROR          = 1,      /* LED Run-time error without case matc  */
	SYSTEM_ERRORTIMEOUT   = 2,      /* LED Operation failed with timeout     */
	SYSTEM_ERRORRESOURCE  = 3,      /* LED Resource not available.           */
	SYSTEM_ERRORPARAMETER = 4,      /* LED Parameter error.                  */
	SYSTEM_ERRORNOMEMORY  = 5,      /* LED Out of memory.                    */
	SYSTEM_ERRORISR       = 6,      /* LED Not allowed in ISR context        */
	SYSTEM_RESERVED       = 0xFF,   /* LED Reserved                          */
} system_status_t;

//************** BSP Layer Targets***********************//
extern bsp_led_handler_t handler_1;
extern bsp_led_driver_t led1;


//************** BSP Layer adapters**********************//
/*DRIVER_Layer:---<led_operations_myown>--*/
led_status_t led_on_myown                                               (void);
led_status_t led_off_myown                                              (void);
extern led_operations_t                                   led_operations_myown;

/*DRIVER_Layer:---<time_base_ms_myown>----*/
led_status_t pf_get_time_ms_mywon               ( uint32_t * const time_stamp);
extern time_base_ms_t                                       time_base_ms_myown;

/*DRIVER_Layer:---<os_delay_myown>--------*/
led_status_t pf_os_delay_ms_myown                 ( const uint32_t delay_time);
extern os_delay_t                                               os_delay_myown;

/* =====self-test :: driver-layer-testing========*/
void Test_1();

/*HANDLER_Layer:---<handler_1_os_delay>---*/
led_handler_status_t os_delay_ms_hanler_1         ( const uint32_t delay_time);
extern os_delay_t                                           handler_1_os_delay;

/*HANDLER_Layer:---<handler1_os_queue>----*/
led_handler_status_t os_queue_create_handler_1 (
                                                  uint32_t const     item_num,
                                                  uint32_t const    item_size,
                                                  void ** const queue_handler);
led_handler_status_t os_queue_put_handler_1    (
                                                  void * const  queue_handler,
                                                  void * const           item,
                                                  uint32_t            timeout);
led_handler_status_t os_queue_get_handler_1    (
                                                  void * const  queue_handler,
                                                  void * const            msg,
                                                  uint32_t            timeout);
led_handler_status_t os_queue_delete_handler1  ( void * const   queue_handler);
extern handler_os_queue_t                                    handler1_os_queue;


/*HANDLER_Layer:---<handler1_os_critical>-*/
led_handler_status_t                        os_critical_enter_handler_1(void );
led_handler_status_t                        os_critical_exit_handler_1 (void );
extern handler_os_critical_t                              handler1_os_critical;

led_handler_status_t get_time_ms_handler1       ( uint32_t * const p_os_tick );
extern handler_time_base_ms_t                               handler1_time_base;

/*HANDLER_Layer:---<handler1_os_thread>---*/
led_handler_status_t thread_create_handler1 ( /* thread_create.         */
                void * const                   task_code,/* Defined. Internal*/
                const char * const             task_name,/* Defined. external*/
                const uint32_t               stack_depth,/* Defined. external*/
                void * const                  parameters,/* Defined. Internal*/
                uint32_t                        priority,/* Defined. external*/
                void ** const               task_handler /* Defined. Internal*/
                                     );
led_handler_status_t thread_delete_handler1 ( void * const queue_handler);
extern handler_os_thread_t                            handler1_os_thread ;            

/* =====self-test :: handler-layer-testing=====*/
void Test_3();

/**
 * @brief init all the resources.
 * 
 * Steps:
 *  1, mix up all the resources in this system.
 *  
 * 
 * @param[in] self      : Pointer to the input data.
 * @param[in] led_ops   : Length of the input data.
 * @param[in] os_delay  : Pointer to the input data.
 * @param[in] time_base : Pointer to the input data.
 * 
 * @return led_status_t : The status of running
 * 
 * */
led_status_t system_init_resources ( void );

#endif /* __SYSTEM_ADAPTION_H__ */