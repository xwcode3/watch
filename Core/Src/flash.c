//
// Created by 文武 on 2026/6/27.
//

#include "flash.h"

#define FLASH_SECTOR_NUM 12
static uint32_t Flash_Sectorsize[FLASH_SECTOR_NUM] = {
    FLASH_Sector_0, // Sector1
    FLASH_Sector_1, // Sector2
    FLASH_Sector_2, // Sector3
    FLASH_Sector_3, // Sector4
    FLASH_Sector_4, // Sector5
    FLASH_Sector_5, // Sector6
    FLASH_Sector_6, // Sector7
    FLASH_Sector_7, // Sector8
    FLASH_Sector_8, // Sector9
    FLASH_Sector_9, // Sector10
    FLASH_Sector_10, // Sector11
    FLASH_Sector_11  // Sector12
};


/**
 * @brief 通过地址获取扇区位置
 *
 * @param[in] uint32_t addr : 地址
 *
 * @return uint16_t : 扇区地址
 */
static uint16_t STMFLASH_GetFlashSector(uint32_t addr)
{
    if (addr < ADDR_FLASH_SECTOR_1) return FLASH_Sector_0;
    else if (addr < ADDR_FLASH_SECTOR_2) return FLASH_Sector_1;
    else if (addr < ADDR_FLASH_SECTOR_3) return FLASH_Sector_2;
    else if (addr < ADDR_FLASH_SECTOR_4) return FLASH_Sector_3;
    else if (addr < ADDR_FLASH_SECTOR_5) return FLASH_Sector_4;
    else if (addr < ADDR_FLASH_SECTOR_6) return FLASH_Sector_5;
    else if (addr < ADDR_FLASH_SECTOR_7) return FLASH_Sector_6;
    else if (addr < ADDR_FLASH_SECTOR_8) return FLASH_Sector_7;
    else if (addr < ADDR_FLASH_SECTOR_9) return FLASH_Sector_8;
    else if (addr < ADDR_FLASH_SECTOR_10) return FLASH_Sector_9;
    else if (addr < ADDR_FLASH_SECTOR_11) return FLASH_Sector_10;
    return FLASH_Sector_11;
}

// Forward declaration
FLASH_Status erease_app_sector(uint32_t FLASH_Sector);


/**
 * @brief This function is used to erase the flash area.
 *
 * @param[in] None
 *
 * @return FLASH_Status : 返回操作状态, 0 : Success, 1 : Fail
 */
uint8_t flash_erase(uint32_t addr, uint32_t size) {
    // 将APP代码所有涉及到的扇区全部删除

    /* 判断size在几个扇区内 */
    // FLASH_Status ret = 1;
    uint32_t flash_start_sector = 0;
    uint32_t flash_end_sector = 0;
    flash_start_sector = (uint32_t)STMFLASH_GetFlashSector(addr);
    flash_end_sector = (uint32_t)STMFLASH_GetFlashSector(addr + size);

    for (uint8_t i = 0; i <= 12; i++) {
        if ((Flash_Sectorsize[i]) >= flash_start_sector &&
        (Flash_Sectorsize[i]) <= flash_end_sector) {
            if (erease_app_sector(Flash_Sectorsize[i]) != FLASH_COMPLETE) {
                return 1;
            }
        }
    }

    return 0;
}

void flash_unlock(void) {
    FLASH_Unlock();
    while (FLASH_GetStatus() == FLASH_BUSY);
}

void flash_lock(void) {
    FLASH_Lock();
}

/**
 * @brief 擦除APP区域的数据
 *
 * @param[in] flash_sector : 扇区号
 *
 * @return FLASH_Status : 返回操作状态
 *
 */
FLASH_Status erease_app_sector(uint32_t FLASH_Sector) {
    flash_unlock();

    FLASH_Status flash_status = FLASH_EraseSector(FLASH_Sector, VoltageRange_3);

    flash_lock();

    return flash_status;
}


void flash_write(uint32_t address, uint32_t data) {
    flash_unlock();

    // FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR);

    FLASH_Status status = FLASH_ProgramWord(address, data);
    if (status == FLASH_COMPLETE) {
        // 写入数据成功
        // printf("flash write success\r\n");
    } else {
        // 数据写入失败
        // printf("flash write failed\r\n");
    }

    flash_lock();
}


