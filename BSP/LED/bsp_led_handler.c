#include "bsp_led_handler.h"

static led_handler_status_t handler_status_from_osal(osal_status_t status)
{
    switch (status)
    {
        case OSAL_OK:             return HANDLER_OK;
        case OSAL_ERRORTIMEOUT:   return HANDLER_ERRORTIMEOUT;
        case OSAL_ERRORRESOURCE:  return HANDLER_ERRORRESOURCE;
        case OSAL_ERRORPARAMETER: return HANDLER_ERRORPARAMETER;
        case OSAL_ERRORNOMEMORY:  return HANDLER_ERRORNOMEMORY;
        case OSAL_ERRORISR:       return HANDLER_ERRORISR;
        default:                  return HANDLER_ERROR;
    }
}

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
 * @return led_handler_status_t : The status of running
 * 
 * */
led_handler_status_t led_blink_handler( bsp_led_driver_t * self )
{
    led_handler_status_t ret = HANDLER_OK;
    //1. check if the target has been instantiated
    if( NULL       == self               || 
        NOT_INITED == self->is_inited      ) 
    {
        //2. if not instantiated, return error to caller 
        //TBD:3.option - mutex to upgrade low priority task to init target ASAP
#ifdef DEBUG
    DEBUG_OUT("HANDLER_ERRORPARAMETER in led_blink_handler\r\n");
#endif  // DEBUG
        ret = HANDLER_ERRORPARAMETER;
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
    DEBUG_OUT("HANDLER_ERRORPARAMETER in led blink\r\n");
#endif  // DEBUG
            ret = HANDLER_ERRORPARAMETER;
            return ret;
        }

        //3. do the operation
        //TBD:4.option - use mutex to protect the critical section
        for(uint32_t i = 0; i < blink_times_local; i++)
        {
            //TBD:5.option - use mutex to protect the critical section
            for(uint32_t j = 0; j < cycle_time_local; j++)
            {
                // [OSAL old] self->p_os_delay->delay_ms(1);
                self->p_os_time_delay->delay_ms(1);
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


led_handler_status_t __event_process(bsp_led_handler_t * self, led_event_t msg)
{
    led_handler_status_t ret = HANDLER_OK;

    /*************0.Checing the target statues  ******************************/
    //1.has checked before starting
    
    /*************1.Checing the input parameters******************************/
    //1.has checked before in handler_led_control
    
    /*************2.process the received event *******************************/
    //2.1 check if the driver target which msg->index pointed to
    
    if(!( ( MAX_INSTANCE_NUMBER >  msg.index ) || 
          ( LED_NOT_INITIALIZED == msg.index )  
        )
      )
    {
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORRESOURCE __event_process index checking\r\n");
#endif // DEBUG
        return HANDLER_ERRORRESOURCE;
    }

    if( LED_NOT_INITIALIZED == msg.index )
    {
        //2.2. if the driver target has not been registered
#ifdef DEBUG
        DEBUG_OUT("__event_process LED_NOT_INITIALIZED\r\n");
#endif // DEBUG
        return HANDLER_ERRORRESOURCE;
    }

    if( INIT_PATTERN == self->instances.led_instance_group[msg.index] )
    {
        //2.2. if the driver target has not been registered
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORRESOURCE at __event_process\r\n");
#endif // DEBUG
        return HANDLER_ERRORRESOURCE;
    }
    
#ifdef DEBUG
        DEBUG_OUT("Start Processing at __event_process\r\n");
#endif // DEBUG

    (self->instances.led_instance_group[msg.index])->cycle_time_ms = \
                                                              msg.Cycle_time;
    (self->instances.led_instance_group[msg.index])->blink_times = \
                                                             msg.blink_times;
    (self->instances.led_instance_group[msg.index])->proportion_on_off = \
                                                       msg.proportion_on_off;
    


    //uint32_t                   Cycle_time;  /* LED Operation completed   */
    //uint32_t                  blink_times;  /* LED Operation completed   */
    //proportion_t        proportion_on_off;  /* LED Operation completed   */
    //led_index_t                     index;  /* LED Operation completed   */


    //TBD:4.process the received event based on the LED's status
    ret = led_blink_handler(self->instances.led_instance_group[msg.index]);
    
    if( HANDLER_OK != ret )
    {
#ifdef DEBUG
        DEBUG_OUT("event processed failed at __event_process\r\n");
#endif // DEBUG
        return HANDLER_ERROR;
    }
    
#ifdef DEBUG
        DEBUG_OUT("event processed at __event_process\r\n");
#endif // DEBUG

    return ret;
}



static led_handler_status_t __array_init(bsp_led_driver_t * array[],
                                         uint32_t array_size        )
{
    for(int i = 0; i < array_size; i++)
    {
        array[i] = (bsp_led_driver_t *)INIT_PATTERN;
    }
    //TBD valid the memory to check if can access;
    return HANDLER_OK;
}


#if 1 /* LED 测试代码 */

// [OSAL old] led_handler_status_t handler_thread( void *argument)
void handler_thread( void *argument)
{    
     vTaskDelay(1000);
#ifdef DEBUG
        DEBUG_OUT("Start_handler_thread \r\n");
#endif // DEBUG

    // [OSAL old] led_handler_status_t ret = HANDLER_OK;
    osal_status_t os_ret = OSAL_OK;
    // [OSAL old] bsp_led_handler_t   *   p_led_handler;
    bsp_led_handler_t   *   p_led_handler = (bsp_led_handler_t *)argument;
    led_event_t                       msg;
    /*************0.Checing the target statues  ******************************/
    //1.check if the target has been instantiated
    /*************1.Checing the input parameters******************************/

    // vTaskDelay(10);
    if ( NULL != argument )
    {
        p_led_handler = argument;
    }
    /*************2.Sending event to LED queue********************************/

    for(;;)
    {
        DEBUG_OUT("Start_handler_thread 2\r\n");
        // [OSAL old] ret = p_led_handler->p_os_queue_interface->queue_get(
        //                                       p_led_handler->queue_handler,
        //                                                       &msg,
        //                                                          0);
        os_ret = p_led_handler->p_os_queue_interface->queue_get(
                                            p_led_handler->queue_handler,
                                            &msg,
                                            OSAL_NO_WAIT);
        if ( os_ret == OSAL_OK )
        {
            DEBUG_OUT("the message received \r\n");
            /*****3.process the received event *******************************/
            __event_process(p_led_handler, msg);
        }
        // vTaskDelay(1000);
#ifdef OS_SUPPORTING
        DEBUG_OUT("handler_thread delay 1s\r\n");
        p_led_handler->p_os_time_delay->delay_ms(1000);
#endif // OS_SUPPORTING
    }

}
#endif /* LED 测试代码 */


led_handler_status_t handler_led_control(
                                         bsp_led_handler_t * const        self,
                                         uint32_t                   Cycle_time,      
                                         uint32_t                  blink_times,
                                         proportion_t        proportion_on_off,
                                         led_index_t          const      index
                                        )
{    
#ifdef DEBUG
        DEBUG_OUT("Start_handler_led_control \r\n");
#endif // DEBUG
    led_handler_status_t ret = HANDLER_OK;

#ifdef DEBUG
        DEBUG_OUT("Checing the target statues \r\n");
#endif // DEBUG
    
    /*************0.Checing the target statues  ******************************/
    //1.check if the target has been instantiated
    if( NOT_INITED == self->is_inited ) 
    {
        //2.if not instantiated, return error to caller 
        //TBD:3.option - mutex to upgrade low priority task to init target ASAP
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORRESOURCE \r\n");
#endif // DEBUG
        ret = HANDLER_ERRORRESOURCE;
        return ret;
    }

#ifdef DEBUG
        DEBUG_OUT("Checing the input parameters \r\n");
#endif // DEBUG
    
    /*************1.Checing the input parameters******************************/
    if( 
        !( 
           (Cycle_time  < 10000 )                        &&
           (blink_times < 1000  )                        &&
           ((PROPORTIONN_1_3 <= proportion_on_off)       &&  
           (PROPORTIONN_1_1 >= proportion_on_off))       &&
           (index <= MAX_INSTANCE_NUMBER)
        ) // end of !(
      )   // end of if(
    {
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORRESOURCE \r\n");
#endif // DEBUG
        ret = LED_ERRORPARAMETER;
        return ret;
    }
   

#ifdef DEBUG
        DEBUG_OUT("Sending event to LED queue \r\n");
#endif // DEBUG
    /*************2.Sending event to LED queue********************************/
     led_event_t let_event = {
        .Cycle_time        =       Cycle_time,
        .blink_times       =       blink_times,
        .proportion_on_off = proportion_on_off,
        .index             =       index
     };

    // [OSAL old] ret = self->p_os_queue_interface->queue_put( self->queue_handler,
    //                                                                   &let_event,
    //                                                                            0);
    ret = handler_status_from_osal(
                self->p_os_queue_interface->queue_put(self->queue_handler,
                                                      &let_event,
                                                      OSAL_NO_WAIT));
    
    if( HANDLER_OK != ret )
    {
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORNOMEMORY\r\n");
#endif // DEBUG

        return ret;
    }

#ifdef DEBUG
        DEBUG_OUT("Sending event to LED queue successfully!!\r\n");
#endif // DEBUG
    
    return ret;

}


/**
 * @brief Regiter the target of led_driver_instance.
 * 
 * Steps:
 *  1, .
 * 
 * @param[in] led_driver      : Pointer to the input data.
 * 
 * @return led_handler_status_t : The status of running
 * 
 * */
led_handler_status_t led_register ( bsp_led_handler_t  * const        self,
                                    bsp_led_driver_t   * const  led_driver, 
                                    led_index_t        * const       index)
{
#ifdef DEBUG
        DEBUG_OUT("Start_led_register \r\n");
#endif // DEBUG
    led_handler_status_t ret = HANDLER_OK;
    /*************0.Checing the target statues  ******************************/
    //1.check if the target has been instantiated
    if( NULL       == led_driver          ||
        NULL       == index               ||
        NOT_INITED == self->is_inited      ) 
    {
        //2.if not instantiated, return error to caller 
        //TBD:3.option - mutex to upgrade low priority task to init target ASAP
#ifdef DEBUG
        DEBUG_OUT("LED_ERRORPARAMETER \r\n");
#endif // DEBUG
        ret = HANDLER_ERRORPARAMETER;
        return ret;
    }

    /*************1.Checing the input parameters******************************/
    if ( INITED != led_driver->is_inited )
    {
        //2.if instantiated, return error to caller
        //TBD:3.option - mutex to upgrade low priority task to init target ASAP
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORRESOURCE \r\n");
#endif // DEBUG
        ret = HANDLER_ERRORRESOURCE;
        return ret;
    }


    /*************2.Adding the instance in target array **********************/
    if( ( MAX_INSTANCE_NUMBER - self->instances.led_instance_num ) == 0 )
    {
        ret = HANDLER_ERRORRESOURCE;
        return ret;
    }
    



#ifdef OS_SUPPORTING
    // [OSAL old] self->p_os_critical->pf_os_critical_enter();
    (void)self->p_os_critical->enter();
#endif //OS_SUPPORTING

    if( (MAX_INSTANCE_NUMBER - self->instances.led_instance_num) > 0  )
    {
        self->instances.led_instance_group[self->instances.led_instance_num] \
                                                                = led_driver;
        *index = self->instances.led_instance_num;
        
        self->instances.led_instance_num++;
    } 

#ifdef OS_SUPPORTING
    // [OSAL old] self->p_os_critical->pf_os_critical_exit();
    (void)self->p_os_critical->exit();
#endif //OS_SUPPORTING

    

#ifdef DEBUG
        DEBUG_OUT("led_register Succees!\r\n");
#endif // DEBUG
    return ret;
}


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
#ifdef DEBUG
        DEBUG_OUT("LED_ERRORPARAMETER \r\n");
#endif // DEBUG
        ret = LED_ERRORPARAMETER;
        return ret;
    }

    /*************1.Checing the input parameters**********/
    //1.cycle time checking

    /*************2.Adding the data in target*************/

    /************3. run the operations of led*************/
    //1.call the fuction to blink


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
 * @param[in] os_delay  : Pointer to the input data.
 * @param[in] time_base : Pointer to the input data.
 * 
 * @return led_status_t : The status of running
 * 
 * */
led_handler_status_t led_handler_inst (
                                  bsp_led_handler_t      * const        self, 
#ifdef OS_SUPPORTING 
                                  // [OSAL old] os_delay_t             * const    os_delay,
                                  const osal_delay_t     * const    os_delay,
                                  // [OSAL old] handler_os_queue_t     * const    os_queue,
                                  const osal_queue_t     * const    os_queue,
                                  // [OSAL old] handler_os_critical_t  * const os_critical,
                                  const osal_critical_t  * const    os_critical,
                                  // [OSAL old] handler_os_thread_t    * const   os_thread,
                                  const osal_thread_t    * const    os_thread,
#endif //OS_SUPPORTING 
                                  // [OSAL old] handler_time_base_ms_t * const  time_base  )
                                  const osal_time_base_t * const  time_base  )
{
    led_handler_status_t ret = HANDLER_OK;
#ifdef DEBUG
    DEBUG_OUT("Handler inst Kick-off \r\n");
#endif // DEBUG

/*************1.Checing the input parameters**************/
    if( NULL == self         ||
#ifdef OS_SUPPORTING
        NULL == os_delay     ||
        NULL == os_queue     ||
        NULL == os_critical  ||
        NULL == os_thread    ||
#endif //OS_SUPPORTING
        NULL == time_base
                              )
    {
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORPARAMETER\r\n");
#endif // DEBUG
        ret = HANDLER_ERRORPARAMETER;
        return ret;
    }

/*************2.Checing the Resources*********************/
    if( INITED == self->is_inited )
    {
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORRESOURCE\r\n");
#endif // DEBUG
        ret = HANDLER_ERRORPARAMETER;
        return ret;
    }

#ifdef DEBUG
    DEBUG_OUT("Handler inst start\r\n");
#endif  // DEBUG

/*************3.Adding the interface**********************/

    //3.1 mount external interface 
    self->p_time_base_ms         =              time_base;
    self->p_os_time_delay        =               os_delay;
    self->p_os_queue_interface   =               os_queue;
    self->p_os_critical          =            os_critical;
    self->p_os_thread            =              os_thread;

 
    //3.2 mount internal interface  
    self->pf_led_countroler      =    handler_led_control;
    self->pf_led_register        =           led_register;

/*************4.Init the target     **********************/
    //4.1 init the thread
    // [OSAL old] ret = self->p_os_thread->pf_os_thread_create      (        handler_thread,
    //                                                                    "handler_thread",
    //                                                                                 256,
    //                                                                                self,
    //                                                                                   0,
    //                                                               &self->thread_handler);
    ret = handler_status_from_osal(
                self->p_os_thread->create(handler_thread,
                                          "handler_thread",
                                          256,
                                          self,
                                          1,
                                          &self->thread_handler));
    if( HANDLER_OK != ret )
    {
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORNOMEMORY at pf_os_thread_create\r\n");
#endif // DEBUG
        return ret;
    }

    //4.2 init the queue that will be used
    // [OSAL old] self->p_os_queue_interface->pf_os_queue_create(                  10,
    //                                                           sizeof(led_event_t),
    //                                                       &(self->queue_handler));
    ret = handler_status_from_osal(
                self->p_os_queue_interface->queue_create(10,
                                                         sizeof(led_event_t),
                                                         &(self->queue_handler)));
    if( HANDLER_OK != ret )
    {
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORNOMEMORY at pf_os_queue_create\r\n");
#endif // DEBUG
        // [OSAL old] self->p_os_thread->pf_os_thread_delete(self->thread_handler);
        self->p_os_thread->delete(self->thread_handler);
        return ret;
    }

    //4.3 init the variables that  will be used
    self->instances.led_instance_num  =                 0;
    ret = __array_init(self->instances.led_instance_group,
                                     MAX_INSTANCE_NUMBER);
if( HANDLER_OK != ret )
    {
#ifdef DEBUG
        DEBUG_OUT("HANDLER_ERRORNOMEMORY at __array_init\r\n");
#endif // DEBUG

        // [OSAL old] self->p_os_thread->pf_os_thread_delete(self->thread_handler);
        self->p_os_thread->delete(self->thread_handler);
        // [OSAL old] self->p_os_queue_interface->pf_os_queue_delete(self->queue_handler);
        self->p_os_queue_interface->queue_delete(self->queue_handler);
        return ret;
    }

    self->is_inited = INITED;

#ifdef DEBUG
    DEBUG_OUT("LED handler init finished\r\n");
#endif  // DEBUG
    
    return ret;
}
