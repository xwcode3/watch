#include "boot_manager.h"


typedef void (*pFunction)(void);

/**
 * @brief  juump to application
 * @param  void
 * @retval void
 */
void JumpToApp1(void) {
    /* 检查栈顶地址是否合法 */
    uint32_t JumpAddress;
    pFunction Jump_To_Application;

    // printf("app = 0x%x\r\n", *(__IO uint32_t *)ApplicationAddress_boot);

    /* 检查栈顶地址是否合法 */
    if(((*(__IO uint32_t *)ApplicationAddress_boot) & 0x2FFE0000) == 0x20000000)
    {
        /* 屏蔽所有中断，防止在跳转过程中，中断干扰出现异常 */
        // __disable_irq();
        NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x8000);
		// 仅禁止SysTick中断，计数器继续运行
		SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
        RCC_DeInit();
        /* 用户代码区第二个 字 为程序开始地址(复位地址) */
        JumpAddress = *(__IO uint32_t *) (ApplicationAddress_boot + 4);

        /* Initialize user application's Stack Pointer */
        /* 初始化APP堆栈指针(用户代码区的第一个字用于存放栈顶地址) */
        __set_MSP(*(__IO uint32_t *) ApplicationAddress_boot);

        /* 类型转换 */
        Jump_To_Application = (pFunction) JumpAddress;

        /* 跳转到 APP */
        Jump_To_Application();
    }
}

int32_t app_size = 0;
/**
 * @brief  backup the app 
 * @param  void
 * @retval int8_t: -1:error; 0:ok
*/
// int8_t back_to_app(void) {
// 	int32_t j = 0;
// 	uint32_t FlashDestination = ApplicationAddress_boot;
//     uint32_t BackflashSource;

//     if ((app_size > (0x18000 - 1)) || (app_size < 0)) {
//         printf("app size error!\r\n");
//         return -1;
//     }

//     BackflashSource = BackupApplicationAddress_boot;
//     for (j = 0; j < app_size; j += 4) {
//         flash_write(FlashDestination, *(uint32_t *)BackflashSource);
//         if (*(uint32_t *)FlashDestination != *(uint32_t *)BackflashSource) {
//             printf("flash write error!\r\n");
//             return -1;
//         }

//         FlashDestination += 4;
//         BackflashSource += 4;
//     }

//     return 0;
// }


unsigned char IV[16] = {0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
                        0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32};
unsigned char Key[32] = {0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
                         0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
                         0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
                         0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32};

uint8_t Mem_Read_buffer[4096];

/**
 * @brief  backup the app 
 * @param  void
 * @retval int8_t: -1:error; 0:ok
*/
int8_t back_to_app(int32_t fl_size) {
    uint32_t AppRunFlashDestination = ApplicationAddress_boot;
    uint8_t *pu8_IV_IN_OUT = IV;
    uint8_t *pu8_key256bit = Key;
    uint8_t *pu8_temp = (uint8_t *)BackupApplicationAddress_boot;
    uint8_t Temp[16];   // 原密文数据缓存
    uint8_t *pTemp = Temp;
    uint16_t readTime = 0, readDataCount = 0;   //读取数据再解密
    u32 AppSize = 0;  // 升级包的大小

    if (fl_size <= 0) {
        return -1;
    }

    // 0x18010 = 98K + 16B;就是APP最大支持98K，然后加上16B的APP大小信息
    if ((app_size > (0x18010 - 1)) || (app_size < 0)) {
        return -1;
    }

    // 擦除运行区数据
    memcpy(pTemp, pu8_temp, 16);
    pu8_temp += 16;
    Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, pTemp, pu8_key256bit); // 解析得到自定义内容+文件大小
    AppSize = (pTemp[15] << 24) + (pTemp[14] << 16) + (pTemp[13] << 8) + pTemp[12];

    // 计算需要解密多少次
    readDataCount = AppSize / 16;
    if (AppSize % 16 != 0) {
        readDataCount += 1;
    }

    // 擦除运行区数据
    if (1 == flash_erase(ApplicationAddress_boot, AppSize)) {
        return -1;
    }

    // 读数据的总次数
    for (readTime = 0; readTime < readDataCount; readTime++) {
        // 加密原文读取16个字节到临时区中
        pTemp = Temp;
        memcpy(pTemp, pu8_temp, 16);
        pu8_temp += 16;
        Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, pTemp, pu8_key256bit); // 解密数据

        // 解密后的数据存入APP运行区
        for (uint8_t j = 0; j < 16; j += 4) {
            flash_write(AppRunFlashDestination, *(uint32_t *)pTemp);
            if (*(uint32_t *)AppRunFlashDestination != *(uint32_t *)pTemp) {
                return -1;
            }
            AppRunFlashDestination += 4;
            pTemp += 4;
        }
    }
    
    return 0;
}

int8_t copy_app_to_executable(uint32_t fl_size) {
    uint8_t Temp[16];   // 原密文数据缓存
    uint8_t writeTime = 0; // 一个解析包写入次数
    uint16_t readTime, readDataCount = 0; // 读取数据再解密的次数（每次解密16个字节）
    uint32_t AppSize = 0; // 升级包的大小
    uint32_t FlashDestination = ApplicationAddress_boot;
    uint16_t Read_Memory_Size = 0;
    uint32_t Read_Memory_index = 0;
    uint8_t *pu8_IV_IN_OUT = IV;
    uint8_t *pu8_key256bit = Key;
    uint32_t RamSource = 0;

    if (fl_size <= 0) {
        return -1;
    }

    if ((app_size > (0x18010 - 1)) || (app_size < 0)) {
        return -1;
    }

    // 先读一帧，用来解析头文件格式
    W25Q64_ReadData(Mem_Read_buffer, &Read_Memory_Size);
    if (Read_Memory_Size >= 16) {
        memcpy(Temp, Mem_Read_buffer, 16);
        Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, Temp, pu8_key256bit);
        AppSize = (Temp[15] << 24) + (Temp[14] << 16) + (Temp[13] << 8) + Temp[12];
        printf("AppSize: %d\r\n", AppSize);

        // 计算升级包读取次数
        readDataCount = AppSize / 16;
        if (AppSize % 16 != 0) {
            readDataCount += 1;
        }
        Read_Memory_index += 16;
    }

    // 数据帧
    // 将待写入区的内容擦除
    uint8_t flash_erase_state = flash_erase(FlashDestination, AppSize);
    if (flash_erase_state == 0) {
        for (readTime = 0; readTime < readDataCount; readTime++) {
            // 判断当前buffer下的数据是否读取完毕
            if (Read_Memory_index == Read_Memory_Size) {
                if (2 == W25Q64_ReadData(Mem_Read_buffer, &Read_Memory_Size)) {
                    printf("Write_Flash_After_AES_Decode read extern buffer error!\r\n");
                    return -1;
                }

                Read_Memory_index = 0;
            }

        

            // 拷贝16个数据
            memcpy(Temp, Mem_Read_buffer + Read_Memory_index, 16);
            Read_Memory_index += 16;

            // 解析16个数据
            Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, Temp, pu8_key256bit); // 解析

            RamSource = (uint32_t)Temp;
            for (writeTime = 0; writeTime < 4; writeTime++) {
                flash_write(FlashDestination, *(uint32_t *)RamSource);
                FlashDestination += 4;
                RamSource += 4;
            }
        }   
        printf("Write_Flash_After_AES_Decode end!\r\n");
        return 0;
    } else {
        printf("Write_Flash_After_AES_Decode error!\r\n");
        return -1;
    }
}

