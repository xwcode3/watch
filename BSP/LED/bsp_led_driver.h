#pragma once
#ifndef BSP_LED_DRIVER_H
#define BSP_LED_DRIVER_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "bsp_config.h"

#include "osal.h"


#define INITED      1                   /* LED has been initialized */
#define NOT_INITED  0                   /* LED has not been initialized */



// Forward declare the struct
typedef struct bsp_led_driver bsp_led_driver_t; 

/* LED status codes */
typedef enum {
	LED_OK             = 0,      /* Operation completed successfully         */
	LED_ERROR          = 1,      /* General runtime error                    */
	LED_ERRORTIMEOUT   = 2,      /* Operation timed out                      */
	LED_ERRORRESOURCE  = 3,      /* Required resource is unavailable         */
	LED_ERRORPARAMETER = 4,      /* Invalid parameter error                  */
	LED_ERRORNOMEMORY  = 5,      /* Memory allocation failed                 */
	LED_ERRORISR       = 6,      /* Not allowed in ISR context               */
	LED_RESERVED       = 0xFF,   /* Reserved status                          */
} led_status_t;


/* LED proportion */
typedef enum
{
    PROPORTIONN_1_3    = 0,      /* ON/OFF ratio 1:3                         */
    PROPORTIONN_1_2    = 1,      /* ON/OFF ratio 1:2                         */
    PROPORTIONN_1_1    = 2,      /* ON/OFF ratio 1:1                         */
    PROPORTIONN_x_x    = 0xFF,   /* Custom ON/OFF ratio                      */
} proportion_t;


typedef struct
{
    led_status_t (*pf_led_on)  (void); /* Turns the LED on                   */
    led_status_t (*pf_led_off) (void); /* Turns the LED off                  */
} led_operations_t;

/* [OSAL old] LED-specific time base interface, replaced by osal_time_base_t */
// typedef struct
// {
//     /* Retrieves time in ms */
//     led_status_t (*pf_get_time_ms)  ( uint32_t * const );
// } time_base_ms_t;

/* [OSAL old] LED-specific OS delay interface, replaced by osal_delay_t */
// #ifdef OS_SUPPORTING
// typedef struct {
//     /* OS-specific delay */
//     led_status_t (*pf_os_delay_ms)  ( const   uint32_t );
// } os_delay_t;
// #endif //OS_SUPPORTING



typedef led_status_t (*pf_led_control_t)(
                                     bsp_led_driver_t * const self,
                                     uint32_t ,   // Cycle time in milliseconds
                                     uint32_t ,   //     Blink repetition count
                                     proportion_t //          ON/OFF proportion
                                        );


/*
 * bsp_led_driver_t
 *
 * LED 驱动器对象，包含 LED 闪烁配置、运行状态、外部接口和控制回调。
 * - is_inited: 是否完成初始化
 * - cycle_time_ms: 一个 LED 闪烁周期的总时长（毫秒）
 * - blink_times: 闪烁次数
 * - proportion_on_off: 开/关时间比例
 * - p_led_opes_inst: LED 硬件操作接口指针
 * - p_time_base_ms: 毫秒时间基接口指针
 * - p_os_time_delay: OS 延时接口指针（仅 OS_SUPPORTING 时可用）
 * - pf_led_countroler: LED 控制函数指针
 */
typedef struct bsp_led_driver {
    /******************* Internal State **************************************/
    uint8_t                              is_inited;  /* Initialization state */

    /******************* Blink Configuration *********************************/
    uint32_t                        cycle_time_ms; /* Total blink cycle time */
    uint32_t                          blink_times; /* Number of blinks       */
    proportion_t                proportion_on_off; /* ON/OFF proportion      */

    /******************* Interface Pointers **********************************/
    led_operations_t            *p_led_opes_inst; /* LED operation interface */
    // [OSAL old] time_base_ms_t               *p_time_base_ms; /* Timebase interface      */
    const osal_time_base_t       *p_time_base_ms; /* Timebase interface      */

#ifdef OS_SUPPORTING
    // [OSAL old] os_delay_t      *p_os_time_delay; /* OS delay interface      */
    const osal_delay_t             *p_os_time_delay; /* OS delay interface      */
#endif //OS_SUPPORTING

    /******************* Control API *****************************************/
    pf_led_control_t           pf_led_countroler; /* LED control function    */
} bsp_led_driver_t;


/**
 * @brief Instantiates the bsp_led_driver_t structure.
 * 
 * Initializes the LED driver instance with provided core, OS, and timebase
 * interfaces.
 * 
 * @param[in] self      : Pointer to the LED driver instance.
 * @param[in] led_ops   : Core LED operations interface.
 * @param[in] os_delay  : OS delay interface (if OS support is enabled).
 * @param[in] time_base : Timebase interface.
 * 
 * @return led_status_t : Operation status
 * 
 */
led_status_t led_driver_inst (
                                      bsp_led_driver_t * const      self, 
                                      led_operations_t * const   led_ops,
#ifdef OS_SUPPORTING
                                    const osal_delay_t * const  os_delay,
#endif //OS_SUPPORTING
                                      // [OSAL old] time_base_ms_t   * const time_base     );
                                      const osal_time_base_t * const time_base     );


#endif
