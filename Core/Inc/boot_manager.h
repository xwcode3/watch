#ifndef __BOOT_MANAGER_H
#define __BOOT_MANAGER_H 

#include <stdio.h>
#include <stdint.h>

#include "stm32f4xx.h"
#include "flash.h"
#include "iwdg.h"
#include "bsp_w25qxx_handler.h"
#include "bsp_AT24Cxx_Driver.h"
#include "AES.h"

#define APP_FLASH_ADDR 0x8010000    // APP 的起始烧录地址

#define ApplicationAddress          0x08010000
#define NVIC_VectTab_FLASH          ((uint32_t)0x08000000)
#define BackupApplicationAddress_boot    0x08020000

typedef enum {
    OTA_STATE_NO_APP_UPDATE = 0x00,
    OTA_STATE_APP_DOWNLOADING = 0x11,
    OTA_STATE_APP_DOWNLOAD_COMPLETE = 0x22,
    OTA_STATE_APP_FIRST_CHECK_START = 0x33,
    OTA_STATE_APP_FIRST_CHECKING = 0x44,
    OTA_STATE_APP_NEED_UPDATE = 0x55,
} ota_state;

void JumpToApp(void);

void System_SoftwareReset(void);
int8_t ExA_To_ExB_AES(int32_t fl_size);
int8_t ExB_To_App(void);
int8_t ExA_To_App(void);
int8_t App_To_ExA(int32_t fl_size);

/**
 * @brief Disable all peripherals and interrupts
 */
void disable_peripherals(void);

/**
 * @brief Disable all peripherals and interrupts and jump to APP
 */
void JumpToApp(void);


/**
 * @brief  OTA状态机处理函数
 * @param  None
 * @retval None
*/
void OTA_StateManager(void);

#endif