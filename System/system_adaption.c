#include "system_adaption.h"

bsp_led_handler_t handler_1;
bsp_led_driver_t led1;

led_status_t led_on_myown(void)
{
    GPIO_ResetBits(GPIOC, GPIO_Pin_13);
    return LED_OK;
}

led_status_t led_off_myown(void)
{
    GPIO_SetBits(GPIOC, GPIO_Pin_13);
    return LED_OK;
}

led_operations_t led_operations_myown = {
    led_on_myown,
    led_off_myown
};

/* [OSAL old] per-instance FreeRTOS adapter functions,
   now wired through the shared osal_freertos_* tables */

led_status_t system_init_resources(void)
{
    led_handler_status_t h_ret;

    if (led_driver_inst(&led1,
                        &led_operations_myown,
                        &osal_delay_freertos,
                        &osal_time_base_freertos) != LED_OK)
    {
        return LED_ERROR;
    }

    h_ret = led_handler_inst(&handler_1,
                             &osal_delay_freertos,
                             &osal_queue_freertos,
                             &osal_critical_freertos,
                             &osal_thread_freertos,
                             &osal_time_base_freertos);
    if (h_ret != HANDLER_OK)
    {
        return LED_ERROR;
    }

    return LED_OK;
}
