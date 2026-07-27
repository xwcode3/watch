#include "boot_manager.h"

typedef void (*pFunction) (void);   // 声明函数指针
static pFunction JumpToAppLocation;

static void bootloader_led_blink(void)
{
  for (int i = 0; i < 10; i++)
  {
    led_control(LED_TOGGLE);
    Delay(100);
  }
}

void disable_peripherals(void) {
  /* ========== 1. 关 USART1 ========== */
  NVIC_DisableIRQ(USART1_IRQn);
  while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) { }
  USART_Cmd(USART1, DISABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, DISABLE);

  /* ========== 2. 关 SPI1 ========== */
  SPI_Cmd(SPI1, DISABLE);
  NVIC_DisableIRQ(SPI1_IRQn);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE);

  // /* ========== 3. 关 ADC + DMA ========== */
  // ADC_Cmd(ADC1, DISABLE);
  // NVIC_DisableIRQ(ADC_IRQn);

  // DMA_Cmd(DMA2_Stream0, DISABLE);
  // NVIC_DisableIRQ(DMA2_Stream0_IRQn);
  // RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, DISABLE);

  // /* ========== 4. 关 Timer ========== */
  // TIM_Cmd(TIM2, DISABLE);
  // NVIC_DisableIRQ(TIM2_IRQn);
  // RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);

  // TIM_Cmd(TIM3, DISABLE);
  // NVIC_DisableIRQ(TIM3_IRQn);
  // RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, DISABLE);

  /* ========== 5. 关 KEY (PA0 + EXTI0) ========== */
  // 先清 EXTI 屏蔽位，防止关时钟后 EXTI 仍然挂起
  EXTI->IMR &= ~EXTI_Line0;
  EXTI->PR  =  EXTI_Line0;    // 写 1 清除 pending
  NVIC_DisableIRQ(EXTI0_IRQn);
  // GPIOA 引脚恢复默认态
  GPIO_InitTypeDef gpio;
  GPIO_StructInit(&gpio);
  gpio.GPIO_Pin   = GPIO_Pin_0;
  gpio.GPIO_Mode  = GPIO_Mode_AN;   // 模拟模式，最省电最安全
  GPIO_Init(GPIOA, &gpio);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, DISABLE);

  /* ========== 6. 关 LED (PC13) ========== */
  GPIO_StructInit(&gpio);
  gpio.GPIO_Pin   = GPIO_Pin_13;
  gpio.GPIO_Mode  = GPIO_Mode_AN;
  GPIO_Init(GPIOC, &gpio);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, DISABLE);

  /* ========== 7. 关 RTC + PWR ========== */
  RCC_RTCCLKCmd(DISABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, DISABLE);

  /* ========== 8. 全局关中断 ========== */
  __disable_irq();
}

void JumpToApp(void)
{
  uint32_t jumpAddr;
  Delay(2000);

  printf("bootloader running...\r\n");

  Delay(1000);


  printf("delay 1s end\r\n");
  uint32_t app_msp = *(__IO uint32_t*)APP_FLASH_ADDR;
  printf("APP MSP Value: 0x%08X\r\n", app_msp);

  /* 校验 App 向量表首字（初始 MSP）是否落在 SRAM 区
   * F411 SRAM: 0x20000000 ~ 0x2001FFFF，链接脚本 _estack = 0x20020000（末地址+1，合法）
   * 注意：不要再用 0x2FFE0000 这种掩码——它会把合法的 0x20020000 误判为非法
   * （会让 bit17 被保留，结果与 0x20000000 不相等）。仅校验 SRAM 区字节即可。 */
  if ((app_msp & 0xFF000000) == 0x20000000)
  {
    printf("Disable all used peripherals and interrupts, jump to APP\r\n");
    disable_peripherals();

    // APP 的第二个字节是 APP 程序的入口地址
    jumpAddr = *(__IO uint32_t*)(APP_FLASH_ADDR + 4);

    // 类型转换
    JumpToAppLocation = (pFunction)jumpAddr;

    // 初始化 APP 堆栈指针（APP 程序的第一个字节用于保存栈顶地址）
    __set_MSP(*(__IO uint32_t*)APP_FLASH_ADDR);

    JumpToAppLocation();
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


#if 0
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
#endif



uint8_t au8_test[1024];
/**
 * @brief  OTA状态机处理函数
 * @param  None
 * @retval None
*/
void OTA_StateManager(void) {
    ota_state e_ota_state = OTA_STATE_NO_APP_UPDATE;
    uint32_t t_u32_appsize = 0;
    int32_t file_size = 0;

    bootloader_led_blink();
    
    // /* 读取eeprom的OTA状态 */
    // ee_ReadBytes(&e_ota_state, 0x00, 1);
    // printf("bootloader read OTA state: 0x%x\r\n", e_ota_state);

    ee_WriteBytes(&e_ota_state, 0x00, 1);
    printf("bootloader write OTA state: 0x%x\r\n", e_ota_state);

    /* 读取eeprom的OTA状态 */
    ee_ReadBytes(&e_ota_state, 0x00, 1);
    printf("bootloader read OTA state: 0x%x\r\n", e_ota_state);

    switch (e_ota_state) {
        case OTA_STATE_NO_APP_UPDATE:
            if (key_scan_bootloader()) {
                /* press key */
                
                /* 1. 将数据下载到外部flash的A区 */
                file_size = Ymodem_Receive(au8_test);
                
                /* 2. 解密数据，然后拷贝到外部flash的B区 */
                if (0 == ExA_To_ExB_AES(file_size)) {
                    /* 3. 当前APP的数据搬运到外部flash的A区，增加一个代码回退功能 */
                    // 读当前APP的大小
                    ee_ReadBytes((uint8_t *)&t_u32_appsize, 0x05, 4);
                    App_To_ExA(t_u32_appsize);
                    /* 4. 外部flash B区的数据搬运到内部App区域中 */
                    ExB_To_App();
                    
                    printf("copy data from extern flash B to app done! Jump to APP\r\n");

                    e_ota_state = OTA_STATE_APP_FIRST_CHECK_START;
                    ee_WriteBytes(&e_ota_state, 0x00, 1);

                    printf("ee write app first check start...\r\n");
                    printf("system software reset...\r\n");
                    // 执行软件复位
                    System_SoftwareReset();
                } else {
                    printf("boot download failed!\r\n");
                    JumpToApp();

                    // App校验无效
                    printf(" app run failed! Restarting...\r\n");
                    // e_ota_state = OTA_STATE_APP_NEED_UPDATE;
                    e_ota_state = OTA_STATE_NO_APP_UPDATE;
                    ee_WriteBytes(&e_ota_state, 0x00, 1);
                    /* 软件复位 */
                    System_SoftwareReset();
                }
            } else {
                printf("no key press, return app!\r\n");
                JumpToApp();

                // App校验无效
                printf("\r\napp run failed! Restarting...\r\n");
                e_ota_state = OTA_STATE_NO_APP_UPDATE;
                ee_WriteBytes(&e_ota_state, 0x00, 1);
                /* 软件复位 */
                System_SoftwareReset();
            }
            break;

        case OTA_STATE_APP_DOWNLOADING: 
            printf("New app downloading, return app!\r\n");
            JumpToApp();
            
            printf("No valid app, redownload app...\r\n");
            
            file_size = Ymodem_Receive(au8_test);
            /* 2. 解密数据，然后拷贝到外部flash的B区 */
            if (0 == ExA_To_ExB_AES(file_size)) {
                /* 3. 当前APP的数据搬运到外部flash的A区，增加一个代码回退功能 */
                // 读当前APP的大小
                ee_ReadBytes((uint8_t *)&t_u32_appsize, 0x05, 4);
                App_To_ExA(t_u32_appsize);
                /* 4. 外部flash B区的数据搬运到内部App区域中 */
                ExB_To_App();
                printf("copy data from extern flash B to app done! Jump to APP\r\n");
                
                e_ota_state = OTA_STATE_APP_FIRST_CHECK_START;
                ee_WriteBytes(&e_ota_state, 0x00, 1);

                printf(" app run failed! Restarting...\r\n");
                /* 软件复位 */
                System_SoftwareReset();


                // /* 5. 执行跳转逻辑 */
                // JumpToApp();
                // /* 6. 如果执行到这里，说明升级失败，将外部A区的数据搬运到App中 */
                // printf("boot update failed, Roll back the app version...\r\n");
                // ExA_To_App();
                // JumpToApp();
            } else {
                printf("boot download failed!\r\n");
                JumpToApp();
            }
            break;
        
        case OTA_STATE_APP_DOWNLOAD_COMPLETE:
            printf("App download complete! Update app now\r\n");            
            /* 1. 读取升级包大小 */
            ee_ReadBytes((uint8_t *)&t_u32_appsize, 0x01, 4);
            /* 2. App大小更新到内部flash存储数据量的管理结构体中 */
            set_block_parameter(BLOCK_1, t_u32_appsize);
            /* 3. 解密数据拷贝到外部flash的B区中 */
            if (0 == ExA_To_ExB_AES(t_u32_appsize)) {
                /* 4. 当前APP的数据搬运到外部flash的A区，增加一个代码回退功能 */
                // 读当前APP的大小
                ee_ReadBytes((uint8_t *)&t_u32_appsize, 0x05, 4);
                App_To_ExA(t_u32_appsize);
                /* 5. 外部flash B区的数据搬运到内部App区域中 */
                ExB_To_App();
                printf("copy data from extern flash B to app done! Jump to APP\r\n");
                
                e_ota_state = OTA_STATE_APP_FIRST_CHECK_START;
                ee_WriteBytes(&e_ota_state, 0x00, 1);

                /* 软件复位 */
                System_SoftwareReset();

                
                
                // /* 6. 执行跳转逻辑 */
                // JumpToApp();
                // /* 7. 如果执行到这里，说明升级失败，将外部A区的数据搬运到App中 */
                // printf("boot update failed, Roll back the app version...\r\n");
                // ExA_To_App();
                // JumpToApp();
            } else {
                printf("boot download failed!\r\n");
                JumpToApp();
                // App校验无效
                printf(" app run failed! Restarting...\r\n");
                e_ota_state = OTA_STATE_NO_APP_UPDATE;
                ee_WriteBytes(&e_ota_state, 0x00, 1);
                /* 软件复位 */
                System_SoftwareReset();
            }
            break;

        case OTA_STATE_APP_FIRST_CHECK_START:
            printf("App first check start...\r\n");
            e_ota_state = OTA_STATE_APP_FIRST_CHECKING;
            ee_WriteBytes(&e_ota_state, 0x00, 1);
            /* 开看门狗 */
            iwdg_init(IWDG_Prescaler_64, 3000); 
            /* 执行跳转逻辑 */
            JumpToApp();
            break;

        case OTA_STATE_APP_FIRST_CHECKING: 
            // APP校验无效
            /* 到达这一步说明数据无效，把外部A区的数据搬运到APP中 */
            ExA_To_App();
            /* 在执行一次跳转 */
            JumpToApp();        
            break;

        // case OTA_STATE_APP_NEED_UPDATE:
        //     printf("App broken, need update!\r\n");

        //                     /* 1. 将数据下载到外部flash的A区 */
        //         file_size = Ymodem_Receive(au8_test);
                
        //         /* 2. 解密数据，然后拷贝到外部flash的B区 */
        //         if (0 == ExA_To_ExB_AES(file_size)) {
        //             /* 3. 当前APP的数据搬运到外部flash的A区，增加一个代码回退功能 */
        //             // 读当前APP的大小
        //             ee_ReadBytes((uint8_t *)&t_u32_appsize, 0x05, 4);
        //             App_To_ExA(t_u32_appsize);
        //             /* 4. 外部flash B区的数据搬运到内部App区域中 */
        //             ExB_To_App();
                    
        //             printf("copy data from extern flash B to app done! Jump to APP\r\n");

        //             e_ota_state = OTA_STATE_APP_FIRST_CHECK_START;
        //             ee_WriteBytes(&e_ota_state, 0x00, 1);

        //             printf("ee write app first check start...\r\n");
        //             printf("system software reset...\r\n");
        //             // 执行软件复位
        //             System_SoftwareReset();
        //         } else {
        //             printf("boot download failed! Restarting...\r\n");
        //             break;
        //         }


        default:
            break;
    }
}

/**
 * @brief 软件复位单片机
 * 
 * @param None
 * 
 * @return: None
 */
void System_SoftwareReset(void)
{
    NVIC_SystemReset();
}


/**
 * @brief  copy the app to extern flash A area
 * @param  void
 * @retval int8_t: -1:error; 0:ok
*/
int8_t App_To_ExA(int32_t fl_size) {
    uint32_t FlashDestination = ApplicationAddress;

    if ((fl_size > (0x18010 - 1)) || (fl_size < 0)) {
        return -1;
    }

    Erase_Flash_Block(BLOCK_1);
    W25Q64_WriteData(BLOCK_1, (uint8_t *)FlashDestination, fl_size);
    W25Q64_WriteData_End(BLOCK_1);
    return 0;
}


/**
 * @brief  解密外部flash中A区的数据，然后写入外部flash的B区
 * @param  void
 * @retval int8_t: -1:error; 0:ok
*/
int8_t ExA_To_ExB_AES(int32_t fl_size) { 
    uint8_t Temp[16]; // 原密文数据缓存
    uint16_t readTime = 0, readDataCount = 0; //读取数据再解密的次数（每次解密16个字节）
    uint32_t AppSize = 0; // 升级包的大小
    uint16_t Read_Memory_Size = 0;
    uint32_t Read_Memory_index = 0;
    uint8_t *pu8_IV_IN_OUT = IV;
    uint8_t *pu8_key256bit = Key;

    if (fl_size <= 0) {
        return -1;
    }

    if ((app_size > (0x18010 - 1)) || (app_size < 0)) {
        return -1;
    }

    /* 这里是读取了 4k 数据，但是只处理前16字节，这 16 字节包括了 自定义内容+文件大小 */
    /* 先读取第一帧，用来解析头文件格式 */
    W25Q64_ReadData(BLOCK_1, Mem_Read_buffer, &Read_Memory_Size);
    if (Read_Memory_Size >= 16) {
        memcpy(Temp, Mem_Read_buffer, 16);
        // 解析得到自定义内容+文件大小
        Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, Temp, pu8_key256bit); 
        AppSize = (Temp[15] << 24) + (Temp[14] << 16) + (Temp[13] << 8) + Temp[12];
        printf("AppSize = %d\r\n", AppSize);

        // 计算升级包读取次数
        readDataCount = AppSize / 16;
        if (AppSize % 16 != 0) {
            readDataCount += 1;
        }
        Read_Memory_index += 16;
    } else {
        return -1;
    }

    /* 擦除外部flash数据比较耗时 */
    for (readTime = 0; readTime < readDataCount; readTime++) {
        /* 判断当前buffer下的数据是否读取完毕 */
        if (Read_Memory_index == Read_Memory_Size) {
            if (2 == W25Q64_ReadData(BLOCK_1, Mem_Read_buffer, &Read_Memory_Size)) {
                printf("ExA_To_ExB_AES read extern buffer error!\r\n");
                return -1;
            }

            Read_Memory_index = 0;
        }

        /* 拷贝16个数据 */
        memcpy(Temp, Mem_Read_buffer + Read_Memory_index, 16);
        Read_Memory_index += 16;
        /* 解析16个数据 */
        Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, Temp, pu8_key256bit); // 解密数据

        W25Q64_WriteData(BLOCK_2, Temp, 16);
    }

    W25Q64_WriteData_End(BLOCK_2);
    printf("Write flash after aes decode end!\r\n");
    return 0;
}

/**
 * @brief  copy the extern flash B area to app
 * @param  void
 * @retval int8_t: -1:error; 0:ok
*/
int8_t ExB_To_App(void) { 
    uint32_t flash_destination = ApplicationAddress;
    uint32_t flash_size = 0;
    uint8_t read_data_state = 0;
    uint16_t read_memory_size = 0;
    uint32_t ram_source = 0;
    uint16_t write_time = 0;    // 写入次数

    /* 擦除本地flash数据 */
    flash_size = Read_BlockSize(BLOCK_2);
    if (1 == flash_erase(flash_destination, flash_size)) {
        return -1;
    } else {
        for (;;) {
            read_data_state = W25Q64_ReadData(BLOCK_2, Mem_Read_buffer, 
                                                    &read_memory_size);
            if (1 == read_data_state) {
                // 数据读取完毕，退出
                break;
            } else if (2 == read_data_state) {
                // 数据读取错误
                printf("ExB_To_App read extern buffer error!\r\n");
                break;
            } else {
                ram_source = (uint32_t)Mem_Read_buffer;
                // 循环搬运flash数据
                for (write_time = 0; write_time < (read_memory_size / 4); write_time++) {
                    flash_write(flash_destination, *(uint32_t *)ram_source);
                    flash_destination += 4;
                    ram_source += 4;
                }
            }  
        }

        return flash_size;
    }
}

/**
 * @brief  copy the extern flash A area to app
 * @param  void
 * @retval int8_t: -1:error; 0:ok
*/
int8_t ExA_To_App(void) {
    uint32_t flash_destionation = ApplicationAddress;
    uint32_t flash_size = 0;
    uint8_t read_data_state = 0;
    uint16_t read_memory_size = 0;
    uint32_t ram_source = 0;
    uint16_t write_time = 0;

    /* 擦除本地flash数据 */
    flash_size = Read_BlockSize(BLOCK_1);
    if (1 == flash_erase(flash_destionation, flash_size)) {
        return -1;
    } else {
        for (;;) {
            read_data_state = W25Q64_ReadData(BLOCK_1, Mem_Read_buffer, 
                                                    &read_memory_size);
            if (1 == read_data_state) {
                // 数据读取完毕，退出
                break;
            } else if (2 == read_data_state) {
                // 数据读取错误
                printf("ExA_To_App read extern buffer error!\r\n");
                break;
            } else {
                ram_source = (uint32_t)Mem_Read_buffer;
                // 循环搬运flash数据
                for (write_time = 0; write_time < (read_memory_size / 4); write_time++) {
                    flash_write(flash_destionation, *(uint32_t *)ram_source);
                    flash_destionation += 4;
                    ram_source += 4;
                }
            }
        }

        return flash_size;
    }
}
