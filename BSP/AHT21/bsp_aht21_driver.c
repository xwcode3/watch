#include "bsp_aht21_driver.h"
#include "timer.h"


#define AHT21_MEASURE_WAITING_TIME  80      // Measurement time [ms]
#define AHT21_NOT_INITED            0       // Not init. flag
#define AHT21_INITED                1       // Init. flag
#define AHT21_ID                    0x18    // AHT21 ID

#define CRC8_POLYNOMIAL             0x31    // CRC-8 polynomial
#define CRC8_INITIAL                0xFF    // CRC-8 initial value

#define IS_INITED (AHT21_INITED == g_inited) // if the device has been inited


static uint8_t g_inited = AHT21_NOT_INITED;     // init. flag
static uint8_t g_device_id = 4;                 // device ID


/**
 * @brief Function for calculating CRC-8 checksum
 * 
 * Steps:
 *  1, XOR each byte of the input data with the CRC register
 *  2, For each bitm check the most significant bit
 *  3, Shift the CRC register left and, if the most significant bit is 1,
 *     XOR with the polynomial.
 * @param[in] p_data : Pointer to the input data.
 * @param[in] length : Length of the input data.
 * 
 * @return uin8_t : The calculated CRC-8 checksum.
 */
static uint8_t check_crc8(const uint8_t *p_data, const uint8_t length) {
    uint8_t crc = CRC8_INITIAL; // Initialize CRC register

    for (uint8_t i = 0; i < length; i++) {
        crc ^= p_data[i]; // XOR the current byte with the CRC

        // Process each bit of the current byte
        for (uint8_t j  =0; j < 8; j++) {
            if (crc & 0x80) {
                // If the MSB is 1, shift and XOR with the polynomial
                crc = (crc << 1) ^ CRC8_POLYNOMIAL;
            } else {
                // otherwise, just shift left
                crc <<= 1;
            }
        }
    }

    return crc; // Return the final CRC value
}



/**
 * @brief Function for reading AHT21 ID.
 * 
 * Steps:
 *  1, Figure out which objects
 *  2, Internallyu call the related functions to read ID.
 * 
 * @param[in] p_aht21_instance : The pointer to object of bsp_aht21_driver_t.
 * 
 * @return aht21_status_t : The status of the operation.
 */
static aht21_status_t __read_id(bsp_aht21_driver_t * const p_aht21_instance) {
    log_i("__read_id");
    iic_bus_t * p_iic_bus = p_aht21_instance->p_iic_driver_instance->iic_bus;

    uint8_t data = 0;

#ifndef HARDWARE_IIC
    p_aht21_instance->p_iic_driver_instance->pf_critical_interface->enter();
#endif /* End of HARDWARE_IIC */

    // Send the IIC start signal
    p_aht21_instance->p_iic_driver_instance->pf_iic_start(p_iic_bus);

    // Send command to read ID
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                                         AHT21_REG_READ_ADDR);
    
    // Wait the ACK of IIC slave device
    // NOTE: IICWaitAck returns 1 = ACK received, 0 = NACK/timeout.
    //       (Do NOT compare against AHT21_OK(0) — that was inverted!)
    if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
       // Receive the data from slave device
       p_aht21_instance->p_iic_driver_instance->pf_iic_receive_byte(p_iic_bus,\
                                                                   &data);
    } else {
       log_e("__read_id: AHT21 NACK (no device on bus?)");
    }

    // Send the Stop signal
    p_aht21_instance->p_iic_driver_instance->pf_iic_stop(p_iic_bus);

#ifndef HARDWARE_IIC
    p_aht21_instance->p_iic_driver_instance->pf_critical_interface->exit();
#endif /* End of HARDWARE_IIC */

    // Check the ID
    if (AHT21_ID == (data & AHT21_ID)) {
        g_device_id = data;
        log_i("id = 0x%x", data);
        log_i("id = 0x%d", data);
        return AHT21_OK;
    }

    log_e("__read_id: unexpected status byte = 0x%x", data);
    return AHT21_ERRORRESOURCE;
}

/**
 * @brief Function for reading AHT21 ID from outside calling.
 * 
 * //@param[in] p_aht21_instance : The pointer to object of bsp_aht21_driver_t.
 * @param[out] p_id : The pointer to ID of AHT21 device.
 * 
 * @return IF of AHT21 : [uint8_t] ID of AHT21 device.
 */
static aht21_status_t aht21_read_id(
    //bsp_aht21_driver_t * const p_aht21_instance, 
    uint8_t* p_id                                   ) 
{

    if (!IS_INITED) {
        return AHT21_ERRORRESOURCE;
    }

    log_i("aht21_read_id");

    *p_id = g_device_id;

    return AHT21_OK;
}

static uint8_t aht21_read_status(bsp_aht21_driver_t * const p_aht21_instance);

/**
 * @brief Function for reading AHT21 Driver Layer init.
 */
static aht21_status_t aht21_init(bsp_aht21_driver_t * const p_aht21_instance) {
    aht21_status_t ret = AHT21_OK;

    log_i("aht21_init start");

    iic_bus_t * p_iic_bus = p_aht21_instance->p_iic_driver_instance->iic_bus;

    delay_ms(300);   /* TIM2 忙等延时，与 13_AHT21 例程逻辑一致 */

    if (NULL == p_aht21_instance->p_iic_driver_instance ||
        NULL == p_aht21_instance->p_iic_driver_instance->pf_iic_init) {
        
       log_e("iic_driver_instance is NULL");
    }

    // Enter critical segment
#ifndef HARDWARE_IIC
    // p_aht21_instance->p_iic_driver_instance->pf_critical_interface->enter();
#endif /* End of HARDWARE_IIC */

    // Init the IIC Interface
    p_aht21_instance->p_iic_driver_instance->pf_iic_init(p_iic_bus);


    if ( (aht21_read_status(p_aht21_instance) && 0x08) != 0x08) {
        p_aht21_instance->p_iic_driver_instance->pf_iic_start(p_iic_bus);
        p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                                         AHT21_REG_WRITE_ADDR);
        if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
            log_i("aht21_init: 1 - AHT21 ACK");
        }
        p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                                         AHT21_REG_RESET_CMD);
        if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
            log_i("aht21_init: 2 - AHT21 ACK");
        }
        p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                                         AHT21_REG_RESET_CMD_ARGS1);
        if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
            log_i("aht21_init: 3 - AHT21 ACK");
        }
        p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                                         AHT21_REG_RESET_CMD_ARGS2);
        if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
            log_i("aht21_init: 4 - AHT21 ACK");
        }                                            
        p_aht21_instance->p_iic_driver_instance->pf_iic_stop(p_iic_bus);
        delay_ms(10);
    }

    log_i("aht21_init iic_driver_init-----------");

    // Read the ID internally
//     ret = __read_id(p_aht21_instance);
//     if (AHT21_OK != ret) {
//         /* MUST exit the critical section before returning, otherwise
//            interrupts stay disabled (taskENTER_CRITICAL) and the
//            FreeRTOS scheduler freezes. */
// #ifndef HARDWARE_IIC
//         p_aht21_instance->p_iic_driver_instance->pf_critical_interface->exit();
// #endif /* End of HARDWARE_IIC */
//         return AHT21_ERRORRESOURCE;
//     }

    // Exit critical segment
#ifndef HARDWARE_IIC
    // p_aht21_instance->p_iic_driver_instance->pf_critical_interface->exit();
#endif /* End of HARDWARE_IIC */

    // Change the flag of the internal flag
    g_inited = AHT21_INITED;

    // Read the AHT21 ID
    // uint8_t aht21_id = 3;
    // aht21_read_id(&aht21_id);
    // log_i("aht21's ID = [0x%x]", aht21_id);

    ret = AHT21_OK;
    return ret;
}



/**
 * @brief Function for reading AHT21 Driver Layer Deinit.
 * 
 * @param[in] p_aht21_instance : The pointer to object of bsp_aht21_driver_t.
 * 
 * @return aht21_status_t : The status of the operation.
 */
static aht21_status_t aht21_deinit(bsp_aht21_driver_t * const p_aht21_instance) {
    log_i("aht21_deinit");
    g_inited = AHT21_NOT_INITED;
    return AHT21_OK;
}



/**
 * @brief Function for reading AHT21 status.
 * 
 * @param[in] p_aht21_instance : The pointer to object of bsp_aht21_driver_t.
 * 
 * @return [uint8_t] : The status of the aht21 device.
 */
static uint8_t aht21_read_status(bsp_aht21_driver_t * const p_aht21_instance) {
    log_i("aht21_get_status");

    iic_bus_t * p_iic_bus = p_aht21_instance->p_iic_driver_instance->iic_bus;

    uint8_t rx_data = 0;        

    /* TEMP-EXP: 与 13_AHT21（可正常工作）一致，去掉临界区包裹。
       位带期间被打断也只是略微拉长时钟周期，从机能容忍；
       若确认为根因，可考虑用 portENTER_CRITICAL 仅保护最短事务。 */
#ifndef HARDWARE_IIC
    // p_aht21_instance->p_iic_driver_instance->pf_critical_interface->enter();
#endif  /* End of HARDWARE_IIC */
    
    // Send the IIC start signal 
    p_aht21_instance->p_iic_driver_instance->pf_iic_start(p_iic_bus);

    // Send the address of IIC slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                                         AHT21_REG_READ_ADDR);

    // Wait the ACK of IIC slave device
    if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
        log_i("aht21_get_status: 1 - AHT21 ACK");
    }

    // Receive the data from slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_receive_byte(p_iic_bus, \
                                                                 &rx_data);
    
    // Send the non-ack signal from Master to Slave
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_no_ack(p_iic_bus);

    // Send the stop signal
    p_aht21_instance->p_iic_driver_instance->pf_iic_stop(p_iic_bus);

    /* TEMP-EXP: 对应上面的 enter 被注释 */
#ifndef HARDWARE_IIC
    // p_aht21_instance->p_iic_driver_instance->pf_critical_interface->exit();
#endif /* End of HARDWARE_IIC */
    
    log_i("read_status = 0x%x", rx_data);
    return rx_data;
}



/**
 * @brief Function for reading AHT21 temperature and humidity data.
 * 
 * @param[in] p_aht21_instance : The pointer to object of bsp_aht21_driver_t.
 * @param[out] p_temperature : The pointer to temperature data.
 * @param[out] p_humidity : The pointer to humidity data.
 * 
 * @return aht21_status_t : [uint8_t] aht21 status.
 */
 aht21_status_t aht21_read_data(
    bsp_aht21_driver_t * const p_aht21_instance,
    float * const p_temperature,
    float * const p_humidity) 
{
    
    log_i("aht21_read_data");

    // if (!IS_INITED) {
    //     return AHT21_ERRORRESOURCE;
    // }

    iic_bus_t * p_iic_bus = p_aht21_instance->p_iic_driver_instance->iic_bus;

    uint8_t cnt = 5;
    uint8_t byte_1th = 0;
    uint8_t byte_2th = 0;
    uint8_t byte_3th = 0;
    uint8_t byte_4th = 0;
    uint8_t byte_5th = 0;
    uint8_t byte_6th = 0;
    uint32_t retu_data = 0;

    /* TEMP-EXP: 与 13_AHT21（可正常工作）一致，去掉临界区包裹 */
#ifndef HARDWARE_IIC
    // p_aht21_instance->p_iic_driver_instance->pf_critical_interface->enter();
#endif /* End of HARDWARE_IIC */

    // Send the IIC start signal
    p_aht21_instance->p_iic_driver_instance->pf_iic_start(p_iic_bus);

    // Send the address of IIC slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                             AHT21_REG_WRITE_ADDR);

    // Wait the ACK of IIC slave device
    if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
        log_i("aht21_read_data: 1 - AHT21 ACK");
    }

    // Send the command to ask aht21 prepare data
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                              AHT21_REG_MEASURE_CMD);

    // Wait the ACK of IIC slave device
    if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
        log_i("aht21_read_data: 2 - AHT21 ACK");
    }

    // Send the command to configure aht21 parameter[1]
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                        AHT21_REG_MEASURE_CMD_ARGS1);

    // Wait the ACK of IIC slave device
    if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
        log_i("aht21_read_data: 3 - AHT21 ACK");
    }

    // Send the command to configure aht21 parameter[2]
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus,\
                                        AHT21_REG_MEASURE_CMD_ARGS2);                                    
    
    // Wait the ACK of IIC slave device
    if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
        log_i("aht21_read_data: 4 - AHT21 ACK");
    }

    // Send the stop signal
    p_aht21_instance->p_iic_driver_instance->pf_iic_stop(p_iic_bus);

    /* TEMP-EXP: 对应上面的 enter 被注释 */

    

    // Do the delay with AHT21_MEASURE_WAITING_TIME
    /* TEMP-EXP: 与 13_AHT21 一致，直接延时等待测量完成 */
#ifdef OS_SUPPORTING
    delay_ms(AHT21_MEASURE_WAITING_TIME);   /* 忙等延时 */
#endif /* End of OS_SUPPORTING */

    // Do the delay with AHT21_MEASURE_WAITING_TIME
    while ((0x80 == (aht21_read_status(p_aht21_instance) & 0x80)) && cnt) {
        delay_ms(5);   /* 忙等延时 */
        cnt--;
        // if (0 == cnt) {
        //     return AHT21_ERRORTIMEOUT;
        // }
        aht21_read_status(p_aht21_instance);
    }

    if (!cnt) {
        log_i("cnt = %d", cnt);
        return AHT21_ERRORTIMEOUT;
    }

    log_i("read temperature");

    /* TEMP-EXP: 与 13_AHT21 一致，去掉读阶段临界区包裹 */
#ifndef HARDWARE_IIC
    // p_aht21_instance->p_iic_driver_instance->pf_critical_interface->enter();
#endif  /* End of HARDWARE_IIC */

    // Send the IIC start signal
    p_aht21_instance->p_iic_driver_instance->pf_iic_start(p_iic_bus);

    // Send the address of IIC slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_byte(p_iic_bus, \
                                                          AHT21_REG_READ_ADDR);
            
    // Wait the ACK of IIC slave device
    if (p_aht21_instance->p_iic_driver_instance->pf_iic_wait_ack(p_iic_bus)) {
       log_i("aht21_read_data: 5 - AHT21 ACK");
    }

    // Receive the data from slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_receive_byte(p_iic_bus,\
                                                                    &byte_1th);
    // Send the ack signal from Master to Slave
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_ack(p_iic_bus);

    // Receive the data from slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_receive_byte(p_iic_bus,\
                                                                    &byte_2th);
    // Send the ack signal from Master to Slave
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_ack(p_iic_bus);

    // Receive the data from slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_receive_byte(p_iic_bus,\
                                                                    &byte_3th);
    // Send the ack signal from Master to Slave
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_ack(p_iic_bus);
    
    // Receive the data from slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_receive_byte(p_iic_bus,\
                                                                    &byte_4th);
    // Send the ack signal from Master to Slave
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_ack(p_iic_bus);
    
    // Receive the data from slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_receive_byte(p_iic_bus,\
                                                                    &byte_5th);
    // Send the ack signal from Master to Slave
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_ack(p_iic_bus);
    
    // Receive the data from slave device
    p_aht21_instance->p_iic_driver_instance->pf_iic_receive_byte(p_iic_bus,\
                                                                    &byte_6th);
    // Send the non-ack signal from Master to Slave
    p_aht21_instance->p_iic_driver_instance->pf_iic_send_no_ack(p_iic_bus);
    
    // Send the stop signal
    p_aht21_instance->p_iic_driver_instance->pf_iic_stop(p_iic_bus);
    
    /* TEMP-EXP: 对应上面的 enter 被注释 */
#ifndef HARDWARE_IIC
    // p_aht21_instance->p_iic_driver_instance->pf_critical_interface->exit();
#endif /* End of HARDWARE_IIC */

    /* TEMP DEBUG: 打印传感器返回的 6 个原始字节 */
    log_i("raw6: %02X %02X %02X %02X %02X %02X",
          byte_1th, byte_2th, byte_3th, byte_4th, byte_5th, byte_6th);

    retu_data = (retu_data | byte_2th) << 8;
    retu_data = (retu_data | byte_3th) << 8;
    retu_data = (retu_data | byte_4th);
    retu_data = retu_data >> 4;
    *p_humidity = (retu_data * 1000 >> 20);
    *p_humidity /= 10;

    retu_data = 0;
    // retu_data = (retu_data | byte_4th) << 8;
    retu_data = (retu_data | (byte_4th & 0x0f)) << 8;
    retu_data = (retu_data | byte_5th) << 8;
    retu_data = (retu_data | byte_6th);
    retu_data = retu_data & 0xfffff;
    *p_temperature = ((retu_data * 2000 >> 20) - 500);
    *p_temperature /= 10;

    log_i("humidity: %f", *p_humidity);
    log_i("temperature: %f", *p_temperature);

    return AHT21_OK;
}



/**
 * @brief Function for reading AHT21 humidity
 * 
 * @param[in] p_aht21_instance: The pointer to object of bsp_aht21_driver_t.
 * @param[out] p_humidity: The pointer to humidity value.
 * 
 * @return aht21_status_t : [uint8_t] aht21 status.
 */
static aht21_status_t aht21_read_humidity(
    bsp_aht21_driver_t *p_aht21_instance, int32_t *p_humidity) {
    
    log_i("aht21_read_humidity");

    if (!IS_INITED) {
        return AHT21_ERRORRESOURCE;
    }

    return AHT21_OK;
}


/**
 * @brief Function for make AHT21 sleep.
 * 
 * @param[in] p_aht21_instance: The pointer to object of bsp_aht21_driver_t.
 * 
 * @return aht21_status_t : [uint8_t] aht21 status.
 */
static aht21_status_t aht21_sleep(bsp_aht21_driver_t *p_aht21_instance) {
    log_i("aht21_sleep");

    if (!IS_INITED) {
        return AHT21_ERRORRESOURCE;
    }

    return AHT21_OK;
}


/**
 * @brief Function for make AHT21 wakeup.
 * 
 * @param[in] p_aht21_instance: The pointer to object of bsp_aht21_driver_t.
 * 
 * @return aht21_status_t : [uint8_t] aht21 status.
 */
static aht21_status_t aht21_wakeup(bsp_aht21_driver_t *p_aht21_instance) {
    log_i("aht21_wakeup");

    if (!IS_INITED) {
        return AHT21_ERRORRESOURCE;
    }

    return AHT21_OK;
}


/**
 * @brief Function for instantiate AHT21 object.
 * 实例化一个 AHT21 对象，挂载 iic 驱动、时基函数、延时函数以及AHT21驱动函数，并初始化 aht21 对象
 * 
 * @param[in] p_aht21_instance: The pointer to object of bsp_aht21_driver_t.
 * @param[in] p_iic_driver_instance: The pointer to object of iic_driver_t.
 * @param[in] p_yield_instance: The pointer to object of yield_driver_t.
 * @param[in] p_timebase_instance: The pointer to object of timebase_driver_t.
 * 
 * @return aht21_status_t : [uint8_t] aht21 status.
 */
aht21_status_t aht21_instantiate(bsp_aht21_driver_t * const p_aht21_instance,
                                 iic_driver_interface_t * const p_iic_driver_instance,
#ifdef OS_SUPPORTING
                                 osal_delay_t * const p_delay_instance,
#endif /* End of OS_SUPPORTING */
                                 osal_time_base_t * const p_timebase_instance) {
    
    log_i("aht21_instantiate");

    if (IS_INITED) {
        // Already instantiated
        return AHT21_ERRORRESOURCE;
    }

    if (NULL == p_aht21_instance) {
    }

    aht21_status_t ret = 0;

    if (NULL == p_aht21_instance || NULL == p_iic_driver_instance) {
        return AHT21_ERROR;
    }

    p_aht21_instance->p_iic_driver_instance = p_iic_driver_instance;
    p_aht21_instance->p_timebase_instance = p_timebase_instance;
    p_aht21_instance->p_delay_instance = p_delay_instance;

    p_aht21_instance->pf_init = (aht21_status_t (*)(void * const)) aht21_init;
    p_aht21_instance->pf_deinit = (aht21_status_t (*)(void * const)) \
                                                                aht21_deinit;
    p_aht21_instance->pf_read_id = (aht21_status_t (*)(void * const)) \
                                                                aht21_read_id;
    p_aht21_instance->pf_read_data = (aht21_status_t (*)(void * const, \
                                                float * const, float * const)) \
                                                                aht21_read_data;
    p_aht21_instance->pf_sleep = (aht21_status_t (*)(void * const)) \
                                                                aht21_sleep;
    p_aht21_instance->pf_wakeup = (aht21_status_t (*)(void * const)) \
                                                                aht21_wakeup;
    
    /* Call the init function */
    ret = aht21_init(p_aht21_instance);
    
    if (ret) {
        log_e("aht21_init failed");
        return AHT21_ERRORRESOURCE;
    }
    log_i("aht21_init success");

    // log_i("aht21_inst end");
    return AHT21_OK;
}