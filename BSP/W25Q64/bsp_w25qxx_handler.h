#ifndef WATCH_BSP_W25QXX_HANDLER_H
#define WATCH_BSP_W25QXX_HANDLER_H

#include "stm32f4xx.h"
#include "bsp_w25qxx.h"


#define EXTERN_FLASH

#define BLOCK_1     0
#define BLOCK_2     1
#define BLOCK_SIZE  0x10000 // 64KB

/*
最小读写单元 Pag = 256 byte
一个扇区 = 16个Pag = 4096 byte
一个块 = 16个扇区 = 64KB
*/
typedef struct {
    uint8_t databuf[4096]; // 4KB buffer for data 
    uint16_t write_databuf_index;   
    uint32_t write_index;
    uint8_t write_sector_index;
    uint32_t read_index;
    uint8_t read_sector_index;
} st_w25q_handler;

void set_block_parameter(uint8_t block_index, uint32_t app_size);
uint32_t Read_BlockSize(uint8_t block_index);
uint8_t W25Q64_Init(void);
void Erase_Flash_Block(uint8_t block_index);
uint8_t W25Q64_EraseChip(void);
uint8_t W25Q64_WriteData(uint8_t block_index, uint8_t *data, uint32_t length);
uint8_t W25Q64_WriteData_End(uint8_t block_index);
uint8_t W25Q64_ReadData(uint8_t block_index, uint8_t *data, uint16_t *length);


#endif // WATCH_BSP_W25QXX_HANDLER_H