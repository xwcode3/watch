#ifndef __BOOT_MANAGER_H
#define __BOOT_MANAGER_H 

#include <stdio.h>
#include <stdint.h>

#include "stm32f4xx.h"
#include "flash.h"
#include "bsp_w25qxx_handler.h"
#include "AES.h"


#define ApplicationAddress_boot          0x08008000
#define NVIC_VectTab_FLASH          ((uint32_t)0x08000000)
#define BackupApplicationAddress_boot    0x08020000


void JumpToApp1(void);
// int8_t back_to_app(void);
int8_t back_to_app(int32_t fl_size);
int8_t copy_app_to_executable(uint32_t fl_size);

#endif