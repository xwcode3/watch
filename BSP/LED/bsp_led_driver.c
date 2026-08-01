#include "bsp_led_driver.h"


#ifndef OS_SUPPORTING
/**
 * @brief perform the detailed behavior of  the bsp_led_driver_t target.
 * 
 * Steps:
 *  1, analyze the target paras..
 *  2, do the appropriate operations of led.
 *  
 * 
 * @param[in] self              : TBD:AI to add comments
 * 
 * @return led_status_t : The status of running
 * 
 * */
led_status_t led_blink( bsp_led_driver_t * self )
{
    led_status_t ret = LED_OK;
    //1. check if the target has been instantiated
    if( NULL       == self               || 
        NOT_INITED == self->is_inited      ) 
    {
        //2. if not instantiated, return error to caller 
        //TBD:3.option - mutex to upgrade low priority task to init target ASAP
#ifdef DEBUG
    DEBUG_OUT("LED_ERRORPARAMETER in led blink\r\n");
#endif  // DEBUG
        ret = LED_ERRORPARAMETER;
        return ret;
    }
    //2. analyze the features
    {
        //2.1. define the time value for saving the features
        uint32_t                    cycle_time_local;
        uint32_t                   blink_times_local;
        proportion_t                proportion_local;
        uint32_t                     led_toggle_time;

        cycle_time_local  =      self->cycle_time_ms;
        blink_times_local =        self->blink_times;
        proportion_local  =  self->proportion_on_off;

        //2.2. calculate the time for each led to toggle
        if( PROPORTIONN_1_1     == proportion_local)
        {
            led_toggle_time = cycle_time_local / 2;
        }
        else if(PROPORTIONN_1_2 == proportion_local)
        {
            led_toggle_time = cycle_time_local / 3;
        }
        else if(PROPORTIONN_1_3 == proportion_local)
        {
            led_toggle_time = cycle_time_local / 4;
        }
        else 
        {
#ifdef DEBUG
    DEBUG_OUT("LED_ERRORPARAMETER in led blink\r\n");
#endif  // DEBUG
            ret = LED_ERRORPARAMETER;
            return ret;
        }

        //3. do the operation
        //TBD:4.option - use mutex to protect the critical section
        for(uint32_t i = 0; i < blink_times_local; i++)
        {
            //TBD:5.option - use mutex to protect the critical section
            for(uint32_t j = 0; j < cycle_time_local; j++)
            {
                self->p_os_time_delay->pf_os_delay_ms(1);
                //TBD:6.option - use mutex to protect the critical section
                if( j < led_toggle_time )
                {
                    //TBD:7.option - LED on
                    self->p_led_opes_inst->pf_led_on();
                }
                else
                {
                    //TBD:8.option - LED off
                    self->p_led_opes_inst->pf_led_off();
                }
            }
        }

    }

    return ret;
}
#endif /* end of OS_SUPPORTING */


#ifndef OS_SUPPORTING
/**
 * @brief Control the bsp_led_driver_t target.
 * 
 * Steps:
 *  1, set the internal values of target, and do the action.
 *  
 * 
 * @param[in] self              : TBD:AI to add comments
 * @param[in] Cycle_time        : TBD:AI to add comments
 * @param[in] blink_times       : TBD:AI to add comments
 * @param[in] proportion_on_off : TBD:AI to add comments
 * 
 * @return led_status_t : The status of running
 * 
 * */
static led_status_t led_control   (
                                bsp_led_driver_t * const       self, // target
                                uint32_t      cycle_time,//     Cycle_time[ms]
                                uint32_t      blink_times,//blink_times[times]
                                proportion_t   proportion//  proportion_on_off
                                )
{
    /*************0.Checing the target statues  **********/
    led_status_t ret = LED_OK;
    //1.check if the target has been instantiated
    if( NULL       == self               || 
        NOT_INITED == self->is_inited      ) 
    {
        //2.if not instantiated, return error to caller 
        //TBD:3.option - mutex to upgrade low priority task to init target ASAP
        ret = LED_ERRORPARAMETER;
        return ret;
    }

    /*************1.Checing the input parameters**********/
    //1.cycle time checking
    if( 
        !( 
           (cycle_time  < 10000 )                 &&
           (blink_times < 1000  )                 &&
           ((PROPORTIONN_1_3 <= proportion)       &&  
           (PROPORTIONN_1_1 >= proportion))         
        ) // end of !(
      )   // end of if(
    {
        ret = LED_ERRORPARAMETER;
        return ret;
    }

    /*************2.Adding the data in target*************/
    self->cycle_time_ms        =  cycle_time;
    self->blink_times          = blink_times;
    self->proportion_on_off    =  proportion;

    /************3. run the operations of led*************/
    //1.call the fuction to blink
    ret = led_blink(self);

    return ret;
}
#endif // #ifndef OS_SUPPORTING



/**
 * @brief init the target of bsp_led_driver_t.
 * 
 * Steps:
 *  1, make the targe at a specific statues;
 *  
 * 
 * @param[in] self      : Pointer to the input data.
 * 
 * @return led_status_t : The status of running
 * 
 * */
led_status_t led_driver_init( bsp_led_driver_t * const self)
{
    led_status_t ret = LED_OK;
    
    DEBUG_OUT("led init start \r\n");
    if( NULL == self )
    {
#ifdef DEBUG
        DEBUG_OUT("LED_ERRORPARAMETER\r\n");
#endif // DEBUG
        return LED_ERRORPARAMETER;
    }
    
    self->p_led_opes_inst->pf_led_off();
    self->p_os_time_delay->pf_os_delay_ms(600);
    uint32_t time_stamp = 0;
    self->p_time_base_ms->pf_get_time_ms(&time_stamp);

    return ret;
}



/**
 * @brief instantiate the target of bsp_led_driver_t.
 * 
 * Steps:
 *  1, Adding the Core interfaces into target of bsp_led_driver instance.
 *  2, Adding the OS interfaces into target of bsp_led_driver instance.
 *  3, Adding the timebase interfaces into target of bsp_led_driver instance.
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
led_status_t led_driver_inst (
                                      bsp_led_driver_t * const      self, 
                                      led_operations_t * const   led_ops,
#ifdef OS_SUPPORTING
                                      os_delay_t       * const  os_delay,
#endif //OS_SUPPORTING
                                      time_base_ms_t   * const time_base    )
{
    led_status_t ret = LED_OK;
#ifdef DEBUG
    DEBUG_OUT("led inst Kick-off \r\n");
#endif // DEBUG

/*************1.Checing the input parameters**************/
    if( NULL == self      ||
        NULL == led_ops   ||
        NULL == os_delay  ||
        NULL == time_base
                              )
    {
#ifdef DEBUG
        DEBUG_OUT("LED_ERRORPARAMETER\r\n");
#endif // DEBUG
        return LED_ERRORPARAMETER;
    }

/*************2.Checing the Resources*********************/
    if( INITED == self->is_inited )
    {
#ifdef DEBUG
        DEBUG_OUT("LED_ERRORRESOURCE\r\n");
        return LED_ERRORRESOURCE;
#endif // DEBUG
    }

#ifdef DEBUG
    DEBUG_OUT("led inst start\r\n");
#endif  // DEBUG

/*************3.Adding the interface**********************/

    //3.1 mount external interface 
    self->p_led_opes_inst =   led_ops;
    self->p_os_time_delay =  os_delay;
    self->p_time_base_ms  = time_base;

    //3.2 mount internal interface 
#ifndef OS_SUPPORTING
    self->pf_led_countroler = led_control;
#endif //#ifndef OS_SUPPORTING
    

/*************4.Init the target     **********************/
    self->blink_times   =                   0;
    self->cycle_time_ms =                   0;
    self->proportion_on_off = PROPORTIONN_x_x;
    
    ret = led_driver_init(self);
    if( LED_OK != ret )
    {
#ifdef DEBUG
        DEBUG_OUT("LED init failed\r\n");
#endif  // DEBUG
        self->p_led_opes_inst =  NULL;
        self->p_os_time_delay =  NULL;
        self->p_time_base_ms  =  NULL;
        return ret;
    }
    
    self->is_inited = INITED ;
#ifdef DEBUG
    DEBUG_OUT("LED init finished\r\n");
#endif  // DEBUG
    
    return ret;
}
