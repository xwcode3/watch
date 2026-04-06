//
// Created by 文武 on 2026/3/26.
//

#ifndef WATCH_BSP_SPEAKER_DRIVER_H
#define WATCH_BSP_SPEAKER_DRIVER_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void (*pf_init)(void);
    // void (*pf_deinit)(void);
    void (*pf_io_write)(uint8_t level);
} spk_gpio_interface_t;

typedef struct {
    void (*pf_init)(void);
    void (*pf_is_speaker_busy)(void);
} spk_busy_interface_t;

typedef struct system_interface_t {
    void (*pf_delay_us)(uint32_t us);
    void (*pf_delay_ms)(uint32_t ms);
} spk_system_interface_t;

typedef struct speaker_driver {
    spk_gpio_interface_t *p_spk_gpio_interface;
    spk_busy_interface_t *p_spk_busy_interface;
    spk_system_interface_t *p_spk_system_interface;

    int8_t (*pf_start_play)(struct speaker_driver *, uint8_t index);
    int8_t (*pf_stop_play)(struct speaker_driver *);
    int8_t (*pf_set_volume)(struct speaker_driver *, uint8_t vol);
    bool (*pf_is_speaker_busy)(struct speaker_driver *);
} speaker_driver_t;

int8_t spk_driver_inst (
    speaker_driver_t *p_speaker_driver,
    spk_gpio_interface_t *p_spk_gpio_interface,
    spk_busy_interface_t *p_spk_busy_interface,
    spk_system_interface_t *p_spk_system_interface
    );

#endif //WATCH_BSP_SPEAKER_DRIVER_H
