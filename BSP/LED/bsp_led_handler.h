#pragma once
#ifndef BSP_LED_HANDLER_H
#define BSP_LED_HANDLER_H
#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"


#include "bsp_led_driver.h"
#include "osal.h"

#define OS_SUPPORTING            /* OS_SUPPORTING depends on OS availability */
// #define DEBUG                    /* Enable DEBUG                             */
// #define DEBUG_OUT(X)   printf(X) /* DEBUG output info to indicate status     */

#define INIT_PATTERN  (bsp_led_driver_t *)(0xA6A6A6A6) 
                                 /* Init space with this pattern             */

typedef struct bsp_led_driver bsp_led_driver_t;

typedef enum
{
	HANDLER_NOT_INITED     = 1,      /* LED handler is not inited            */ 
    HANDLER_INITED         = 0,      /* LED handler is inited                */
} led_handler_init_t;

typedef enum
{
	LED_1                  = 0,      /* LED index number which passed to app */ 
    LED_2,                           /* LED index number which passed to app */
    LED_3,                           /* LED index number which passed to app */
    LED_4,                           /* LED index number which passed to app */
    LED_5,                           /* LED index number which passed to app */
    LED_6,                           /* LED index number which passed to app */
    LED_7,                           /* LED index number which passed to app */
    LED_8,                           /* LED index number which passed to app */
    LED_9,                           /* LED index number which passed to app */
    LED_10,                          /* LED index number which passed to app */
    MAX_INSTANCE_NUMBER,             /* LED index number which passed to app */ 
    LED_NOT_INITIALIZED = 0xFFFFFFFF /* LED index number which passed to app */
} led_index_t;

typedef enum
{
	HANDLER_OK             = 0,      /* LED operation completed successfully */
	HANDLER_ERROR          = 1,      /* LED runtime error without cause      */
	HANDLER_ERRORTIMEOUT   = 2,      /* LED operation failed due to timeout  */
	HANDLER_ERRORRESOURCE  = 3,      /* LED resource unavailable             */
	HANDLER_ERRORPARAMETER = 4,      /* LED parameter error                  */
	HANDLER_ERRORNOMEMORY  = 5,      /* LED out of memory                    */
	HANDLER_ERRORISR       = 6,      /* LED not allowed in ISR context       */
	HANDLER_RESERVED       = 0xFF,   /* Reserved for future use              */
} led_handler_status_t;


typedef struct {
    uint32_t                   Cycle_time;  /* LED Operation completed       */
    uint32_t                  blink_times;  /* LED Operation completed       */
    proportion_t        proportion_on_off;  /* LED Operation completed       */
    led_index_t                     index;  /* LED Operation completed       */
} led_event_t;

// [OSAL old] module-local OS interface types, replaced by osal.h
// #TODO: 这里为什么还要 get time base ？在 driver 中已经定义了 time base
// 因为 handler 也需要时基函数来进行一些调度工作
// typedef struct
// {
//     led_handler_status_t (*pf_get_time_ms)  ( uint32_t * const );/* LED Op   */
// } handler_time_base_ms_t;


// #ifdef OS_SUPPORTING
// typedef struct
// {
//     led_handler_status_t (*pf_os_delay_ms)  ( const   uint32_t );/* LED Op   */
// } handler_os_delay_t;

// typedef struct
// {
//     /* OS queue create */
//     led_handler_status_t (*pf_os_queue_create )  ( 
//                                                   uint32_t const     item_num,
//                                                   uint32_t const    item_size,
//                                                   void ** const queue_handler);
    
//     /* OS queue put */
//     led_handler_status_t (*pf_os_queue_put    )  ( 
//                                                   void * const  queue_handler,
//                                                   void * const           item,
//                                                   uint32_t           timeout );
    
//     /* OS queue get */                                        
//     led_handler_status_t (*pf_os_queue_get    )  ( 
//                                               void * const      queue_handler,
//                                               void * const                msg,
//                                               uint32_t               timeout );
    
//     /* OS queue delete */                                        
//     led_handler_status_t (*pf_os_queue_delete )  ( 
//                                               void * const      queue_handler);
// } handler_os_queue_t;

// typedef struct
// {
//     led_handler_status_t (*pf_os_critical_enter) (void );/* Enter critical   */
//     led_handler_status_t (*pf_os_critical_exit ) (void );/* Exit critical    */
// } handler_os_critical_t;

// typedef struct
// {
//     led_handler_status_t (*pf_os_thread_create ) ( /* Thread create          */
//                 void * const                   task_code, /* Defined internal*/
//                 const char * const             task_name, /* Defined external*/
//                 const uint32_t               stack_depth, /* Defined external*/
//                 void * const                  parameters, /* Defined internal*/
//                 uint32_t                        priority, /* Defined external*/
//                 void ** const               task_handler  /* Defined internal*/
//                                      );

//     led_handler_status_t (*pf_os_thread_delete ) (void *);/* Thread delete   */

// } handler_os_thread_t;
// #endif //OS_SUPPORTING

typedef struct bsp_led_handler bsp_led_handler_t;

typedef led_handler_status_t (*pf_handler_led_control_t)(
                                         bsp_led_handler_t * const        self,
                                         uint32_t ,       //    Cycle time [ms]
                                         uint32_t ,       // Blink times[times]
                                         proportion_t,    // Proportion  on/off
                                         led_index_t         const        index
                                        );

typedef led_handler_status_t (*pf_handler_led_register_t)(
                                         bsp_led_handler_t * const        self,
                                         bsp_led_driver_t  * const  led_driver,
                                         led_index_t        * const       index
                                        );

typedef struct
{
    uint32_t           led_instance_num;           /* Number of instances    */
    bsp_led_driver_t * led_instance_group[MAX_INSTANCE_NUMBER]; /* Array     */
} instance_registered_t;


/*
 * bsp_led_handler_t
 *
 * LED 处理器对象，包含运行状态、注册的 LED 实例、OS/时间基接口以及控制 API。
 * - is_inited: 是否完成初始化
 * - instances: 已注册的 LED 驱动实例集合
 * - queue_handler: OS 消息队列句柄，用于内部事件传递
 * - thread_handler: OS 线程句柄，用于执行 LED 处理任务
 * - p_time_base_ms: Core 层时间基接口指针，用于获取毫秒时钟
 * - p_os_time_delay: OS 延时接口指针，用于在 OS 环境中延时
 * - p_os_queue_interface: OS 队列接口指针，用于创建、发送、接收队列消息
 * - p_os_critical: OS 临界区接口指针，用于进入/退出临界区
 * - p_os_thread: OS 线程接口指针，用于创建和删除线程
 * - pf_led_countroler: 对外 LED 控制 API 函数指针
 * - pf_led_register: 内部驱动注册 API 函数指针
 */
typedef struct bsp_led_handler
{
    /**********Internal runtime data for internal status**********************/
    /**********Target of internal status**********/
    uint8_t                              is_inited;              /* Initialization state */
    
    /**********Internal registered instances******/
    instance_registered_t                instances;              /* Registered LED instances */
    /*************Target of features**************/
    void *                           queue_handler;          /* OS queue handle for internal messages */
    void *                          thread_handler;          /* OS thread handle for LED task */
    // May have more features later...

    /**********Interfaces for internal use***********************************/
    /*************Target of IOs needed************/
    /* Interface from Core layer                  */
    // handler_time_base_ms_t         *p_time_base_ms;         /* Core time base interface */
    // [OSAL old] osal_time_base_t    *p_time_base_ms;         /* Core time base interface */
    const osal_time_base_t            *p_time_base_ms;         /* Core time base interface */
    /* Interface from OS layer                    */
#ifdef OS_SUPPORTING
    // [OSAL old] osal_delay_t        *p_os_time_delay;         /* OS delay interface */
    const osal_delay_t                *p_os_time_delay;         /* OS delay interface */
    // [OSAL old] osal_queue_t        *p_os_queue_interface;    /* OS queue API interface */
    const osal_queue_t                *p_os_queue_interface;    /* OS queue API interface */
    // [OSAL old] osal_critical_t     *p_os_critical;           /* OS critical section interface */
    const osal_critical_t             *p_os_critical;           /* OS critical section interface */
    // [OSAL old] osal_thread_t       *p_os_thread;             /* OS thread API interface */
    const osal_thread_t               *p_os_thread;             /* OS thread API interface */
#endif //OS_SUPPORTING

    /**********Interfaces for external use***********************************/
    /*************Target of APIs******************/
    /* For APP                                    */
    pf_handler_led_control_t     pf_led_countroler;       /* LED control API for application */
    /* For internal driver                        */
    pf_handler_led_register_t      pf_led_register;         /* Register LED driver instance API */

} bsp_led_handler_t;

/**
 * @brief Processes the event.
 * 
 */
led_handler_status_t __event_process(bsp_led_handler_t * self, led_event_t msg);

/**
 * @brief Instantiates the bsp_led_handler_t target.
 * 
 * Steps:
 *  1. Adds Core interfaces into bsp_led_driver instance target.
 *  2. Adds OS interfaces into bsp_led_driver instance target.
 *  3. Adds timebase interfaces into bsp_led_driver instance target.
 *  
 * @param[in] self      : Pointer to the input data.
 * @param[in] led_ops   : Length of the input data.
 * @param[in] os_delay  : Pointer to the input data.
 * @param[in] time_base : Pointer to the input data.
 * 
 * @return led_handler_status_t : Status of the operation.
 * 
 * */
led_handler_status_t led_handler_inst (
                                  bsp_led_handler_t      * const        self, 
#ifdef OS_SUPPORTING 
                                  // [OSAL old] osal_delay_t    * const    os_delay,
                                  const osal_delay_t    * const  os_delay,
                                  // [OSAL old] osal_queue_t     * const    os_queue,
                                  const osal_queue_t     * const  os_queue,
                                  // [OSAL old] osal_critical_t  * const os_critical,
                                  const osal_critical_t  * const  os_critical,
                                  // [OSAL old] osal_thread_t    * const   os_thread,
                                  const osal_thread_t    * const  os_thread,
#endif //OS_SUPPORTING 
                                  // [OSAL old] osal_time_base_t * const  time_base  );
                                  const osal_time_base_t * const  time_base  );

#endif /* BSP_LED_HANDLER_H */
