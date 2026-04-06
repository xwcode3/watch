//
// Created by 文武 on 2026/3/26.
//
#include "bsp_speaker_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "elog.h"


/**
 *@brief 给speaker发送数据，需要考虑数据传输的效率，这里采用串行传输数据，即一个字节一个字节传输
 *       根据给出的时序图，需要先将数据线拉低5ms，然后发送16位数据（先发送高字节，再发低字节，先发送低位，在发送高位）
 *       使用高低点评比例来表示每个数据位的值：
 *       高电平维持600us，低电平维持200us，即高低电平比例为3:1，表示数值1
 *       高电平维持200us，低电平维持600us，即高低电平比例为1:3，表示数值0
 *
 *
 */
int8_t bsp_speaker_write_register(speaker_driver_t * speaker_driver, uint8_t data) {
    log_i("bsp_speaker_writer_register");
    log_i("data = %d", data);

    uint8_t j;
    uint8_t b_data;
    uint8_t s_data = data;

    speaker_driver->p_spk_gpio_interface->pf_io_write(0);
    speaker_driver->p_spk_system_interface->pf_delay_ms(5);

    // 通过串口发送数据，首先发送最低位，这里是先获取最低位
    b_data = (s_data & 0x01);

    // 增加临界区进行保护
    taskENTER_CRITICAL();

    for (j = 0; j < 8; j++) {
        log_i("j = %d", j);
        if (b_data == 1) {
            speaker_driver->p_spk_gpio_interface->pf_io_write(1);
            speaker_driver->p_spk_system_interface->pf_delay_us(600);
            speaker_driver->p_spk_gpio_interface->pf_io_write(0);
            speaker_driver->p_spk_system_interface->pf_delay_us(200);
        } else {
            speaker_driver->p_spk_gpio_interface->pf_io_write(1);
            speaker_driver->p_spk_system_interface->pf_delay_us(200);
            speaker_driver->p_spk_gpio_interface->pf_io_write(0);
            speaker_driver->p_spk_system_interface->pf_delay_us(600);
        }

        s_data = s_data >> 1;       // 右移，去掉最低位
        b_data = s_data & 0x01;     // 获取下一位
    }

    // 发送完毕，推出临界区
    taskEXIT_CRITICAL();

    speaker_driver->p_spk_gpio_interface->pf_io_write(1);

    return 0;
}


/**
 *@brief 播放指定索引的音频
 *
 */
