#include "bsp_mpu6050_handler.h"
#include "elog.h"


//******************************** Defines **********************************//
#define HANDLER_UNINITIALIZED 0
#define HANDLER_INITIALIZED   1

//******************************** Variables ********************************//
static uint8_t mpuxxx_handler_is_initialized = HANDLER_UNINITIALIZED;
bsp_mpuxxx_handler_t handler_instance = {0};



//******************************** Functions ********************************//

void (*pf_pin_interrupt_callback)(void *, void *);
void (*pf_dma_interrupt_callback)(void *, void *);


/*
* @brief Register the callback function for mpuxxx INT interrupt
* @param callback: The callback function
*/
void register_callback(void (*callback)(void *, void *))
{
    pf_pin_interrupt_callback = callback;
}

/*
* @brief Register the callback function for DMA complete
* @param callback: The callback function
*/
void register_callback_dma(void (*callback)(void *, void *))
{
    pf_dma_interrupt_callback = callback;
}




/**
 * @brief Initialize the handler.
 * 
 * @param handler_instance: The handler to initialize.
 * 
 * @return: The status of the initialization.
 */
uint8_t mpuxxx_handler_init(bsp_mpuxxx_handler_t *handler_instance)
{
    uint8_t ret = 1;

    if (mpuxxx_handler_is_initialized == HANDLER_INITIALIZED)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_init: handler already initialized\n");
#endif
        return 0;
    }

/*********************************************************/
#if 0 // queue test    
    if (NULL == handler_instance->p_input_args->p_os->os_queue_create)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_init: os queue create is NULL\n");
#endif
        return 0;
    }

    // Create the mpuxxx queue
    ret = handler_instance->p_input_args->p_os->os_queue_create(
                                        handler_instance->queue_length,
                                        handler_instance->queue_item_size,
                                        &handler_instance->p_queue_handle);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_init: queue create failed\n");
#endif
        return ret;
    }
#endif // End of queue test

/*********************************************************/

#if 0 // binary test
    // Create the mpuxxx binary
    ret = handler_instance->p_input_args->p_os->os_semaphore_create_binary(
                                        &handler_instance->semaphore_binary_handle);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_init: binary create failed\n");
#endif
        return ret;
    }
#endif // End of binary test

/*********************************************************/

#if 1 // notify test

#endif // End of notify test

    // Create the unpack queue
    ret = handler_instance->p_input_args->p_queue->queue_create(
                                        handler_instance->queue_length,
                                        handler_instance->queue_item_size,
                                        &handler_instance->p_unpack_queue_handle);
    if (ret != 0)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_init: unpack queue create failed\n");
#endif
        return ret;
    }

    if (NULL == handler_instance->p_driver)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_init: driver is NULL\n");
#endif
        return 0;
    }

    // Initialize the driver
    ret = bsp_mpuxxx_driver_inst(
            handler_instance->p_driver,
            handler_instance->p_input_args->p_iic_driver,
#ifdef OS_SUPPORTING
            handler_instance->p_input_args->p_delay,
            handler_instance->p_input_args->p_loop_delay,
            handler_instance->p_input_args->p_thread,
            handler_instance->p_input_args->p_queue,
            handler_instance->p_input_args->p_mutex,
            handler_instance->p_input_args->p_semaphore,
            handler_instance->p_input_args->p_notify,
            handler_instance->p_input_args->p_critical,
            handler_instance->p_input_args->p_time_base,
#endif /* End of OS_SUPPORTING */
            
            register_callback,
            register_callback_dma

#ifdef OS_SUPPORTING
            ,handler_instance->p_queue_handle,
            handler_instance->mutex_handle,
            handler_instance->semaphore_binary_handle,
            handler_instance->notify_handle
#endif /* End of OS_SUPPORTING */
                                                    );
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_init: driver init failed\n");
#endif
        return ret;
    }

    return ret;
}


/**
 * @brief Initialize the handler.
 * 
 * @param handler_instance: The handler to initialize.
 * @param p_input_args: The input arguments.
 * 
 * @return: The status of the initialization.
 */
uint8_t mpuxxx_handler_inst(
                        bsp_mpuxxx_handler_t *handler_instance, 
                        mpuxxx_handler_input_args_t *p_input_args )
{
    uint8_t ret = 1;
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_handler_inst start\n");
#endif /* End of DEBUG */

/* Check the input arguments */
    if (NULL == handler_instance)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_inst: handler is NULL\n");
#endif
        return 0;
    }

    if (NULL == p_input_args)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_inst: input args is NULL\n");
#endif
        return 0;
    }

//     if (NULL == p_input_args->p_iic_driver ||
//         NULL == p_input_args->p_delay      ||
//         NULL == p_input_args->p_os         ||
//         NULL == p_input_args->p_timebase )
//     {
// #ifdef DEBUG
//         DEBUG_OUT("mpuxxx_handler_inst: input args is invalid\n");
// #endif
//         return 0;
//     }
/* Save the input arguments */
    handler_instance->p_input_args = p_input_args;

/* Initialize the handler */
    ret = mpuxxx_handler_init(handler_instance);
    if (ret != 1)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_inst: handler init failed\n");
#endif
        return ret;
    }

/* Set the handler initialized */
    mpuxxx_handler_is_initialized = HANDLER_INITIALIZED;

#ifdef DEBUG
    DEBUG_OUT("mpuxxx_handler_inst end\n");
#endif /* End of DEBUG */
    return ret;
}


/**
 * @brief The thread function for mpuxxx handler
 * 
 * @param argument: The argument
 */
void mpuxxx_handler_thread(void *argument)
{
    uint8_t ret = 1;
	uint8_t data = 1;
	
#ifdef DEBUG
    DEBUG_OUT("mpuxxx_handler_thread start\n");
#endif

    if (NULL == argument)
    {
#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_thread: argument is NULL\n");
#endif
        return;
    }

    mpuxxx_handler_input_args_t *p_input_args =\
                                (mpuxxx_handler_input_args_t *)argument;

    buffer_init(&circular_buf, 10);
	// mpuxxx driver 
    bsp_mpuxxx_driver_t p_driver;
	// mpuxxx handler
    handler_instance.p_driver                = &p_driver;
    handler_instance.p_queue_handle          = NULL;
    handler_instance.p_unpack_queue_handle   = NULL;
    handler_instance.semaphore_binary_handle = NULL;
    handler_instance.notify_handle           = NULL;
    handler_instance.queue_item_size         = 1;
    handler_instance.queue_length            = 20;

    ret = mpuxxx_handler_inst(&handler_instance, p_input_args);
	if (ret != 1)
	{
#ifdef DEBUG
		DEBUG_OUT("mpuxxx_handler_thread: mpuxxx_handler_inst failed\n");
#endif
		return;
	}
#ifdef DEBUG
	DEBUG_OUT("=====mpuxxx_handler_inst success=====\n");
#endif

    for (;;)
    {
/*********************************************************/
#if 0 // queue test
        // Get the data from the mpuxxx queue
        ret = handler_instance.p_input_args->p_os->os_queue_get(handler_instance.p_queue_handle,
                                                    &data,
                                                    0xffffffff);
        if (ret != 1)
        {
#ifdef DEBUG
            DEBUG_OUT("mpuxxx_handler_thread: queue get failed\n");
#endif
        }

#ifdef DEBUG
        DEBUG_OUT("mpuxxx_handler_thread: data = %d\n", data);
#endif

#endif // queue test

/*********************************************************/

#if 0 // binary test
        ret = handler_instance.p_input_args->p_os->os_semaphore_wait_binary(handler_instance.semaphore_binary_handle);
        if (ret != 1)
        {
#ifdef DEBUG
            DEBUG_OUT("mpuxxx_handler_thread: semaphore wait failed\n");
#endif
		}
#endif // binary test

/*********************************************************/

#if 0 // notify test
        ret = handler_instance.p_input_args->p_os->os_semaphore_wait_notify(
                                                                            0,
                                                                            0,
                                                                            NULL,
                                                                            0xffffffff);
#endif // End of notify test

/*********************************************************/
#if 1 // global variable test
        if (1 == mpuxxx_flag_read())
        {
            // Put the data into the unpack queue for unpacking
            ret = handler_instance.p_input_args->p_queue->queue_put(handler_instance.p_unpack_queue_handle,
                                                        &data,
                                                        0);
            if (ret != 0)
            {
#ifdef DEBUG
                DEBUG_OUT("mpuxxx_handler_thread: queue put failed\r\n");
#endif
            }
            
            mpuxxx_flag_set(0);
        }

#endif // End of notify test

        handler_instance.p_input_args->p_delay->delay_ms(1);
    }
}

