#include "bsp_w25qxx_handler.h"

static st_w25q_handler s_st_W25Q_Handler_1;

/**
 * @brief Initialize the chip's flash area and the handler structure.
 * @return 1 on success, 0 on failure
 */
uint8_t W25Q64_Init(void) {
	if (W25Qx_Init() != W25Qx_OK) {
		// Handle initialization error
		return 0;
	}

	s_st_W25Q_Handler_1.read_index = 0;
	s_st_W25Q_Handler_1.read_sector_index = 0;
	s_st_W25Q_Handler_1.write_databuf_index = 0;
	s_st_W25Q_Handler_1.write_index = 0;
	s_st_W25Q_Handler_1.write_sector_index = 0;
	
	return 1;
}


/**
 * @brief Erase the chip's flash area.
 * @return 0 on success, 1 on failure
 */
uint8_t W25Q64_EraseChip(void) {
	if (0 == W25Qx_Erase_Chip()) {
		s_st_W25Q_Handler_1.read_index = 0;
		s_st_W25Q_Handler_1.read_sector_index = 0;
		s_st_W25Q_Handler_1.write_databuf_index = 0;
		s_st_W25Q_Handler_1.write_index = 0;
		s_st_W25Q_Handler_1.write_sector_index = 0;
		return 0;
	}
	
	return 1;
}



/**
 * @brief Write data to the chip's flash area.
 * @param data: uint8_t*, Pointer to the data to be written
 * @param length: uint32_t, Number of bytes to write
 * @return 0 on success, 1 on failure
 */
uint8_t W25Q64_WriteData(uint8_t *data, uint32_t length) {
	// uint8_t ret = 0;
	uint32_t addr = 0;
	uint16_t index = 0;

	for (uint16_t i = 0; i < length; i++) {
		// 1.数据写入数据缓冲区里面
		index = s_st_W25Q_Handler_1.write_databuf_index;
		s_st_W25Q_Handler_1.databuf[index] = *(data + i);
		s_st_W25Q_Handler_1.write_databuf_index++;

		// 2.判断数据有没有写满4096
		if (s_st_W25Q_Handler_1.write_databuf_index == W25Qx_Para.SUBSECTOR_SIZE) {
			s_st_W25Q_Handler_1.write_databuf_index = 0;
			// 擦除一个sector
			addr = W25Qx_Para.SUBSECTOR_SIZE * s_st_W25Q_Handler_1.write_sector_index;
			W25Qx_Erase_Block(addr);
			W25Qx_WriteEnable();
			// 写满一个sector，执行写入操作， 4096个byte
			for (uint8_t j = 0; j < 16; j++) {
				// 获取当前写入地址
				addr = (W25Qx_Para.SUBSECTOR_SIZE * s_st_W25Q_Handler_1.write_sector_index) + \
					(W25Qx_Para.PAGE_SIZE_W * j);
				
				// 执行写入操作
				index = W25Qx_Para.PAGE_SIZE_W * j;
				W25Qx_Write(&s_st_W25Q_Handler_1.databuf[index], addr, W25Qx_Para.PAGE_SIZE_W);
			}

			s_st_W25Q_Handler_1.write_sector_index++;
			
			// 记录总的写入数据长度
			s_st_W25Q_Handler_1.write_index += W25Qx_Para.SUBSECTOR_SIZE;
		}
	}
	
	return 0;
}



/**
 * @brief write the last remaining data to the flash.
 * @return 0 on success
 */
uint8_t W25Q64_WriteData_End(void) {
	uint32_t addr = 0;
	uint16_t index = 0;
	uint8_t page_size = 0;

	// 判断还有没有剩余数据没有执行写入
	if (0 != s_st_W25Q_Handler_1.write_databuf_index) {
		// 写入次数
		page_size = s_st_W25Q_Handler_1.write_databuf_index / W25Qx_Para.PAGE_SIZE_W;

		// 先执行擦除扇区
		addr = W25Qx_Para.SUBSECTOR_SIZE * s_st_W25Q_Handler_1.write_sector_index;
		W25Qx_Erase_Block(addr);
		W25Qx_WriteEnable();

		for (uint8_t j = 0; j < page_size; j++) {
			// 获取当前写入地址
			addr = (W25Qx_Para.SUBSECTOR_SIZE * s_st_W25Q_Handler_1.write_sector_index) + \
				(W25Qx_Para.PAGE_SIZE_W * j);

			// 执行写入操作
			index = W25Qx_Para.PAGE_SIZE_W * j;
			W25Qx_Write(&s_st_W25Q_Handler_1.databuf[index], addr, W25Qx_Para.PAGE_SIZE_W);
		}

		// 没有小于256的数据了
		if (0 != (s_st_W25Q_Handler_1.write_databuf_index % W25Qx_Para.PAGE_SIZE_W)) {
			// 获取当前写入地址
			addr = (W25Qx_Para.SUBSECTOR_SIZE * s_st_W25Q_Handler_1.write_sector_index) + \
				(W25Qx_Para.PAGE_SIZE_W * page_size);

			// 执行写入操作
			index = W25Qx_Para.PAGE_SIZE_W * page_size;
			W25Qx_Write(&s_st_W25Q_Handler_1.databuf[index], addr, \
				s_st_W25Q_Handler_1.write_databuf_index % W25Qx_Para.PAGE_SIZE_W);
		}
		s_st_W25Q_Handler_1.write_sector_index += s_st_W25Q_Handler_1.write_databuf_index;
	}
	return 0;
}



/**
 * @brief 每次读取一个扇区的数据
 * @param data: 外部接口需要一个4096的buffer
 * @param length: Number of bytes to read.
 * @return 0 on success, 1 读取完毕，没有数据了, 2 读取失败，读取错误
 */
uint8_t W25Q64_ReadData(uint8_t *data, uint16_t *length) {
	// uint8_t ret = 0;
	uint32_t addr = 0;
	// uint16_t index = 0;
	// uint8_t page_size = 0;
	// uint16_t remain_size = 0;

	// 1.先判断是否读取完毕
	if (s_st_W25Q_Handler_1.write_index > s_st_W25Q_Handler_1.read_index) {
		// 判断下数据是否够4K
		if (s_st_W25Q_Handler_1.write_sector_index > s_st_W25Q_Handler_1.read_sector_index) {
			// 读4K数据的操作
			*length = W25Qx_Para.SUBSECTOR_SIZE;
			addr = s_st_W25Q_Handler_1.read_sector_index * W25Qx_Para.SUBSECTOR_SIZE;
			if (0 != W25Qx_Read(data, addr, *length)) {
				return 2;
			}
			s_st_W25Q_Handler_1.read_sector_index++;
		} else {
			// 读4K以内的数据
			*length = s_st_W25Q_Handler_1.write_index - s_st_W25Q_Handler_1.read_index;
			addr = s_st_W25Q_Handler_1.read_sector_index * W25Qx_Para.SUBSECTOR_SIZE;
			if (0 != W25Qx_Read(data, addr, *length)) {
				return 2;
			}
		}

		s_st_W25Q_Handler_1.read_index += *length;
		return 0;
	} else {
		return 1;
	}
}


