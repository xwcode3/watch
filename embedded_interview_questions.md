# STM32 Watch APP 项目分析 + 嵌入式面试题集

> 适用对象：3-5 年经验的嵌入式软件工程师
> 分析范围：`watch_projects/app` 仓库当前快照（git commit `83affac`）
> 说明：只分析实际参与编译和调用链上的代码；被注释/未启用的模块单独标注；本文档不修改任何项目代码。

## 一、项目结构与编译边界

### 1. 硬件与软件栈

- MCU：STM32F411CEU6，Cortex-M4F，512KB Flash / 128KB RAM
- 外设库：STM32 标准外设库（StdPeriph），不是 HAL
- RTOS：FreeRTOS，heap_4，75KB 堆
- 构建：CMake + arm-none-eabi-gcc
- 日志：EasyLogger，最终经 `printf -> __io_putchar -> USART1_SendByte` 输出
- 中间件：Ymodem、CRC16-XModem、AES-256、SEGGER RTT（RTT 实际只有头文件和编译，未调用）

### 2. Flash 分区与地址

| 地址 | 用途 |
|---|---|
| `0x08000000` | Bootloader（不在此仓库） |
| `0x08010000` | APP 链接起始地址，占 448KB |
| `0x08008000` | `boot_manager.h` 里遗留的 `ApplicationAddress_boot` |
| `0x08020000` | 遗留的 `BackupApplicationAddress_boot` / `BackAppAddress` |

APP 通过 `app_pre()` 把 `SCB->VTOR` 指向 `0x08010000`，这是和 Bootloader 配合的“向量表重定位”。

### 3. 实际参与编译、实际被调用的核心模块

- `Core/Src/main.c`：启动初始化、EEPROM OTA 状态校验、创建任务
- `Core/Src/FreeRTOS_tasks.c`：实际只创建 `led_blink_task` 和 `ota_task`
- `Core/Src/usart.c`：USART1 + DMA + IDLE 中断，OTA 命令接收和 printf 输出
- `Core/Src/spi.c` + `BSP/W25Q64`：外部 Flash W25Q64，OTA 升级包暂存
- `Core/Src/iic.c` + `BSP/AT24Cxx`：软件 I2C 读写 AT24C02，存 OTA 状态和长度
- `APP/OTA/ota.c` + `Middlewares/Ymodem`：OTA 状态机 + Ymodem 接收
- `Middlewares/CRC`、`Middlewares/AES`：Ymodem CRC 校验；AES 目前只在未调用的 `boot_manager` 里使用

### 4. 编译进工程但当前没有调用者的“死代码”

- `BSP/SPEAKER`、`BSP/LCD`、`BSP/AHT21`、`BSP/MPU6050`：CMake 里未编译
- `BSP/KEY/bsp_key.c`：编译了，但 `key_init()` 被注释、`key_task` 未被创建
- `BSP/ADC/bsp_adc.c`：编译了，但 `adc_and_dma_init()` 被注释
- `BSP/USART/bsp_usart_driver.c`、`usart_parse.c`、`mid_circular_buffer.c`：编译了，但 `usart_dma_init()` 被注释
- `Core/Src/boot_manager.c`：编译了，`JumpToApp1/back_to_app/copy_app_to_executable` 在 APP 内无调用者
- `Ymodem_Receive`（单缓冲版）、`Ymodem_Transmit`、common 里大部分菜单函数：未使用
- `SEGGER_RTT`：未实际调用

## 二、启动与主流程

1. `Reset_Handler` 拷贝 `.data`、清零 `.bss`、调用 `__libc_init_array`，最后进 `main`。
2. `app_pre()` 把 VTOR 指到 `0x08010000`。
3. `SystemClock_Config()` 配 HSE + PLL：按代码注释和 `HSE_VALUE=25MHz` 推算为 SYSCLK 100MHz，HCLK 100MHz，APB1 50MHz，APB2 100MHz，Flash 3 等待周期，SysTick 1ms。
4. `gpio_init()`：NVIC 优先级分组 Group 4，只初始化 LED（PC13）；按键、TIM2/TIM5 初始化被注释。
5. `ota_usart1_config()`：USART1 115200 + DMA2 Stream2 + IDLE 中断，同时作为 printf 输出通道。
6. 初始化 EasyLogger、AT24C02、SPI1、W25Q64。
7. 读 EEPROM 地址 0 的 OTA 状态：
   - `0x44`：写回 `0x00` 后 `while(1){}`，等 Bootloader 看门狗复位；
   - 其他非 `0x00`：直接 `SoftReset()`；
   - `0x00`：正常进入系统。
8. `app_task_create()` 创建 `led_blink_task`（优先级 1）和 `ota_task`（优先级 2），然后 `vTaskStartScheduler()`。

任务表（当前真正运行）：

| 任务 | 优先级 | 栈大小 | 行为 |
|---|---|---|---|
| `led_blink_task` | 1 | 128 words | 每 2 秒翻转 PC13 |
| `ota_task` | 2 | 256 words | OTA 命令状态机，Ymodem 接收 |

## 三、核心业务逻辑

### 1. OTA 状态机

- `WaitReqDownload`：`uartx_receive2idle_dma()` 把 4 字节命令收进 DMA 缓冲，IDLE 中断把长度发进 `Q_YmodemReclength`，任务阻塞等队列。收到 `0x11 0x22 0x33` 后进入 `OtaDownload`，并把 EEPROM 状态写成 `0x11`。
- `OtaDownload`：`YmodemUart_EnterRxMode()` 关闭 DMA/IDLE，改轮询模式；`Ymodem_Receive_2buffer()` 把升级包写进 W25Q64；完成后恢复 DMA 模式。成功则进入 `WaitReqUpdate`，回 3 字节 ACK `0x44 0x55 0x66`，EEPROM 状态写 `0x22`，并把升级包长度写到 EEPROM 地址 `0x01`。
- `WaitReqUpdate`：等待 `0x55 0x66 0x77`，收到后进入 `OtaEnd`。
- `OtaEnd`：轮询 PA0 按键 20 秒，按下则 `SoftReset()`，否则回 `WaitReqDownload`。

### 2. UART 两种接收模式

- 命令模式：DMA2 Stream2 正常模式 + USART IDLE 中断。IDLE 时 `rec_len = dma2_init_ndtr - DMA_GetCurrDataCounter()`，通过队列通知 OTA 任务。
- Ymodem 模式：关闭 DMA 和 IDLE，`SerialKeyPressed()` 直接轮询 `USART1->DR`。

### 3. Ymodem + W25Q64

- `Ymodem_Receive_2buffer()` 用两个 1030 字节缓冲交替接收，SOH 128 字节包或 STX 1024 字节包，校验序号/反码和 CRC16-XModem。
- 每个数据包直接调用 `W25Q64_WriteData()` 写入外部 Flash 的 4KB 缓冲；结束调用 `W25Q64_WriteData_End()` 写尾部。
- 应用本身不在内部 Flash 写升级包；真正“拷贝到运行区”的逻辑在未调用的 `copy_app_to_executable()` 里，预期由 Bootloader 侧完成。

## 四、函数作用速查（按调用链）

### main.c / 系统

| 函数 | 作用 |
|---|---|
| `app_pre()` | 设置 `SCB->VTOR = 0x08010000`，开启全局中断，供 Bootloader 跳转后运行 |
| `SystemClock_Config()` | HSE+PLL 配 100MHz 系统时钟、Flash 缓存/等待周期、SysTick 1ms |
| `gpio_init()` | NVIC Group 4 + `led_init()`；按键/TIM 初始化被注释 |
| `ota_usart1_config()` | 复位并配置 USART1 115200，使能 DMA 接收和 IDLE 中断，供 OTA 与 printf 使用 |
| `get_clock_info()` | 用 `RCC_GetClocksFreq` 打印 SYSCLK/HCLK/PCLK1/PCLK2 |
| `easylogger_config()` | `elog_init` + 各级别输出格式 + `elog_start` |
| `__io_putchar()` | 重定向 `printf` 到 `USART1_SendByte` |
| `Delay()` / `TimingDelay_Decrement()` | 基于 SysTick 计数的忙等毫秒延时，ISR 中递减 |
| `SoftReset()` | 置 FAULTMASK 后 `NVIC_SystemReset()` 软复位 |

### OTA / Ymodem

| 函数 | 作用 |
|---|---|
| `ota_task()` | OTA 主状态机：命令校验、Ymodem 接收、EEPROM 状态维护、按键确认复位 |
| `ota_key_scan()` | 20 秒内轮询 PA0，做简单消抖，判断是否按键 |
| `Ymodem_Receive_2buffer()` | 双缓冲接收 Ymodem 文件，CRC/序号校验，数据落 W25Q64 |
| `Receive_Packet()` | 收一帧：SOH/STX/EOT/CA/ABORT，校验序号反码与 CRC16 |
| `Receive_Byte()` / `Send_Byte()` | 轮询式串口收发原语 |
| `SerialKeyPressed()` | 检查 RXNE，读 `USART1->DR` |
| `SerialPutChar()` | 阻塞发送一个字节 |
| `crc_xmodem()` | CRC16-CCITT/XModem 表驱动校验 |
| `W25Q64_WriteData()` | 把数据暂存到 4KB RAM 缓冲，满 4KB 擦除扇区并分 16 页写入 |
| `W25Q64_WriteData_End()` | 把剩余不足 4KB 的数据擦除并写入外部 Flash |

### UART/DMA

| 函数 | 作用 |
|---|---|
| `uartx_receive2idle_dma()` | 停止 DMA，设置 M0AR/NDTR，清标志，重新使能 DMA 和 USART DMA 请求 |
| `ota_usart1_dma_config()` | 把 DMA2 Stream2 配成 Normal 模式的 USART1 RX |
| `USART1_IRQHandler()` | IDLE 时计算接收长度并 `xQueueSendFromISR` 给 OTA 队列 |
| `YmodemUart_EnterRxMode()` | 关闭 DMA/IDLE，清残留标志，切到 Ymodem 轮询模式 |
| `YmodemUart_RestoreDmaMode()` | 恢复 DMA 配置并重新打开 IDLE |
| `USART1_SendByte()` / `usart1_transmit()` | 阻塞等待 TXE 后发送 |
| `DMA2_Stream2_IRQHandler()` | HT/TC 处理，当前 OTA 路径未使能该中断，基本为空 |

### I2C / EEPROM

| 函数 | 作用 |
|---|---|
| `i2c_Start()` / `i2c_Stop()` | 产生 I2C 起始/停止时序 |
| `i2c_SendByte()` / `i2c_ReadByte()` | MSB 先发/先收 8 bit |
| `i2c_Ack()` / `i2c_NAck()` / `i2c_WaitAck()` | 主机 ACK/NACK 和等待从机 ACK |
| `i2c_CheckDevice()` | 发设备地址，探测从机是否应答 |
| `ee_CheckOk()` | 检测 AT24C02 是否在线 |
| `ee_ReadBytes()` | 随机地址连续读，最后 NACK + Stop |
| `ee_WriteBytes()` | 页写，页边界处重发地址，用 ACK 轮询等内部写周期 |
| `ee_Erase()` / `ee_Test()` | 全片写 0xFF / 读写自检，当前业务未调用 |

### SPI / W25Q64

| 函数 | 作用 |
|---|---|
| `spi1_init()` | 配 PA5/6/7 为 SPI1，Mode 0，软件 CS，主模式 |
| `spi1_write_byte()` / `spi1_read_byte()` | 全双工阻塞收发，带 OVR 清除和超时检查 |
| `W25Qx_Init()` | 复位外设并读参数（容量/扇区数） |
| `W25Qx_WriteEnable()` | 发 0x06，轮询状态寄存器确认 |
| `W25Qx_Read()` / `W25Qx_Write()` | 读任意地址；按 256B 页编程 |
| `W25Qx_Erase_Block()` / `W25Qx_Erase_Chip()` | 4KB 扇区擦除 / 整片擦除 |
| `W25Q64_Init()` | 初始化外部 Flash 并复位 handler 索引 |
| `W25Q64_WriteData()` / `W25Q64_WriteData_End()` | 4KB 缓冲批量写和尾部写 |
| `W25Q64_ReadData()` | 按 4KB/剩余长度读回升级包 |

### 未启用但值得解释的函数

- `key_led_handle_task()`：设计上是按键事件转 LED 闪烁命令的中间任务，依赖 `key_queue`/`led_queue`，当前未创建。
- `dma_handler()` / `adc_app()`：ADC 双缓冲：DMA 写一块、消费一块，用任务通知位 `PRODUCED/CONSUMING` 和互斥量协调，当前未启动。
- `JumpToApp1()` / `back_to_app()` / `copy_app_to_executable()`：Bootloader 语义函数，含 AES-256-CBC 解密和内部 Flash 编程，当前 APP 内无调用者，且地址与 APP 链接地址存在不一致，属于遗留代码。

## 五、面向 3-5 年嵌入式工程师的面试题与详细答案

### A. 启动、链接、内存布局

#### Q1. 为什么 APP 要改 VTOR？`app_pre()` 做了什么？Bootloader 跳 APP 前必须处理什么？

APP 与 Bootloader 都放在内部 Flash，Bootloader 占 `0x08000000` 起的一段，向量表也在那里。APP 链接到 `0x08010000` 后，中断向量表如果还指在 `0x08000000`，任何中断都会跳到 Bootloader 的异常向量。因此 APP 第一件事就是把 `SCB->VTOR` 改成自己的向量表地址。

Bootloader 跳转前至少要：
1. 校验 APP 起始 4 字节栈顶地址合法（通常在 RAM 范围内）；
2. 取向量表第二个字作为复位向量；
3. `__set_MSP()` 设置主栈指针；
4. 跳转前关闭全局中断、停 SysTick、复位外设和 RCC，防止旧外设中断在跳转瞬间触发；
5. 跳转后 APP 再重新初始化时钟/外设/VTOR。

项目里 `JumpToApp1()` 已有雏形，但当前 APP 内没有调用它，且它把向量表设在 `0x08008000`，和 APP 链接地址 `0x08010000` 不一致。追问点：跳转前为什么不完全关中断、为什么要 `RCC_DeInit`。

#### Q2. 链接脚本里 FLASH ORIGIN=0x08010000 预留的 64KB 是什么？`.data`、`.bss`、堆栈如何排布？

预留的 64KB 是 Bootloader 区域。链接脚本把 APP 代码从 `0x08010000` 开始放。`.data` 有加载地址（Flash 里的初始值）和运行地址（RAM），由启动汇编从 `_sidata` 拷到 `_sdata`；`.bss` 由启动代码清零。

RAM 排布从低到高通常是 `.data` -> `.bss` -> newlib heap（`_sbrk` 管理）-> FreeRTOS heap（heap_4 的静态数组其实也在 BSS 内）-> MSP 栈（`_Min_Stack_Size` 0x400）。`_sbrk()` 会检查堆顶不能越过 `_estack - _Min_Stack_Size`，防止堆栈碰撞。

追问点：为什么 FreeRTOS 的 `pvPortMalloc` 和 `malloc` 是两套堆？因为 heap_4 用自己的 `ucHeap` 数组，而 C 库 `malloc` 走 `_sbrk`；两套堆并存容易浪费 RAM。

#### Q3. Cortex-M 上 FreeRTOS 为什么用 SVC、PendSV、SysTick？PendSV 为什么必须最低优先级？

- SVC：`vTaskStartScheduler()` 通过 SVC 进入内核，在特权模式下完成第一个任务的启动。
- SysTick：产生 1ms tick，驱动延时、时间片、软件定时器。
- PendSV：负责上下文切换。把切换“压到最低优先级”是为了让高优先级中断不被切换过程延迟；如果切换期间来了 IRQ，PendSV 会被再推迟，等所有中断处理完再切换，避免在中断处理中途切换任务。

FreeRTOS 临界区用 BASEPRI 屏蔽优先级不高于阈值的异常，而不是 PRIMASK 全关，这样 NMI、HardFault 等始终可响应，也缩短关中断时间。`configMAX_SYSCALL_INTERRUPT_PRIORITY` 保证只有“安全”的中断才能调用 ISR 版 API。

#### Q4. 怎么估算任务栈大小？项目里 128/256 words 够不够？

每个任务占用 RAM = TCB + 栈。`xTaskCreate` 的栈参数单位是“word”（4 字节），所以 `led_blink_task` 128 words = 512B，`ota_task` 256 words = 1KB。但 Ymodem 接收路径里调用 `printf`、`log_i`、CRC、W25Q64 写函数，调用链深，1KB 栈可能偏紧。

正规做法：
1. `configCHECK_FOR_STACK_OVERFLOW` 打开（当前为 0）；
2. 用 `uxTaskGetStackHighWaterMark()` 在任务运行后看余量；
3. 压测最坏调用路径（Ymodem 满负荷 + 日志输出）；
4. 避免在任务里放大局部数组（例如把 1030 字节接收缓冲放在任务局部会直接爆栈）。

追问：`printf` 在嵌入式里为什么危险？因为它的格式解析和缓冲可能吃几百字节栈，且非线程安全。

#### Q5. HardFault 之后怎么定位？当前 `HardFault_Handler` 只是死循环，怎么改进？

当前 `HardFault_Handler` 死循环只能让调试器暂停。改进方向：
1. 在 Handler 里读 `SCB->HFSR/CFSR/MMFAR/BFAR`，保存现场到全局结构体；
2. 从 MSP/PSP 恢复发生 fault 时的 PC、LR、SP；
3. 用 J-Link/RTT/串口打印现场并复位；
4. 用 `CMBacktrace` 这类库做栈回溯；
5. 结合 `-fno-omit-frame-pointer` 和 map 文件定位函数。

常见原因：栈溢出、空指针、非法外设访问、Flash 操作期间取指、FreeRTOS 堆耗尽。

### B. 时钟与定时

#### Q6. 请推演 `SystemClock_Config()` 的时钟树。

按 `HSE_VALUE=25MHz` 和代码注释：PLLM=12，PLLN=96，PLLP=2，PLLQ=4。计算：

- PLL VCO 输入 = 25MHz / 12 ≈ 2.083MHz
- PLL VCO 输出 = 2.083 × 96 ≈ 200MHz
- SYSCLK = 200 / 2 = 100MHz
- HCLK = SYSCLK / 1 = 100MHz
- APB1 = 100 / 2 = 50MHz，APB1 定时器时钟因 APB1 分频不为 1 而翻倍为 100MHz
- APB2 = 100MHz

100MHz 需要 Flash 等待周期 3，代码里 `FLASH_SetLatency(FLASH_Latency_3)` 正确；同时使能了指令/数据缓存。

追问点：2.083MHz 的 PLL VCO 输入已经接近/超出 F4 数据手册的 2MHz 上限，如果实际晶振不是 25MHz，整条链路会偏离；面试时可以说“需要核对实际晶振频率和 PLL 范围”。

#### Q7. 如果 HSE 起振失败，代码会怎样？如何增强健壮性？

`RCC_HSEConfig` 后 `while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET);` 会死等。增强方式：加超时计数，超时后回退 HSI 16MHz；用 `RCC_WaitForHSEStartUp()` 的返回值判断；失败时点亮错误 LED 或走应急模式。

#### Q8. `Delay()` 忙等和 `vTaskDelay` 有什么区别？为什么在 RTOS 里要少用 `Delay()`？

`Delay()` 是自旋：任务不释放 CPU，SysTick 中断里 `TimingDelay_Decrement()` 才递减，期间更高优先级任务无法运行。`vTaskDelay` 让任务进入阻塞态，由内核调度，低优先级任务也能运行。

当前 W25Q64 忙等循环里使用了 `Delay(1)`，这会让 `ota_task` 在擦写 Flash 期间独占 CPU，LED 任务等其它任务被饿死。优化：把“等 Flash 不忙”改成带超时的轮询并定期 `vTaskDelay(1)`，或者把 SPI/Flash 操作放到独立低优先级任务，用信号量通知。

### C. FreeRTOS 任务与同步

#### Q9. 为什么 OTA 任务优先级比 LED 高？调度器会怎么调度？

`ota_task` 优先级 2，`led_blink_task` 优先级 1。OTA 只有阻塞等串口命令时 LED 任务才能运行；一旦串口数据到达，ISR 唤醒 OTA，OTA 抢占 LED。这符合“通信协议任务优先保证不丢帧”的设计。代价是 OTA 里任何长忙等都会压住 LED。

追问：如果 OTA 里做 W25Q64 整片擦除（几十秒），其他任务怎么办？这就是为什么需要任务拆分、状态机化、擦写中让出 CPU。

#### Q10. FreeRTOS 队列是拷贝语义，项目里为什么传指针？有什么风险？

`key_queue`、`led_queue` 创建为 `sizeof(指针)`，这是“零拷贝”手法：队列只搬指针，不搬数据。风险是：
1. 指针指向的对象生命周期必须比队列消费晚；
2. 如果多个发送者写同一个静态变量，后写会覆盖前写；
3. ISR 里发指针时，目标对象不能是 ISR 局部变量；
4. 需要保证消费者拿到后、写者再次修改前完成读。

`bsp_key.c` 里 `key_queue` 存 `key_press_status_t*`，而指向的是静态 `key_result`，多次按键事件会互相覆盖，这是典型面试陷阱。

#### Q11. 互斥量和二值信号量有什么区别？优先级继承在项目中怎么体现？

互斥量支持优先级继承、递归持有、由持有者释放；二值信号量只是计数 0/1，没有继承。优先级继承解决的是：低优先级任务持锁时，系统临时把它的优先级提升到等待者的优先级，避免中优先级任务长期抢占导致高优先级任务饿死。

项目 `dma_handler` 的注释解释了这种用法：DMA 任务短暂 take/give 同一个互斥量，如果消费任务被抢占，它会因互斥量继承机制临时升到 DMA 任务优先级，保证 ADC 数据尽快被消费、DMA 能及时开启下一轮传输。追问：这个做法只提升“持有者”，不能保证绝对实时；真正的硬实时要关调度或用 DMA 双缓冲直接交给处理线程。

#### Q12. 任务通知、事件组、队列、信号量怎么选？ADC 双缓冲代码里 `PRODUCED/CONSUMING` 位图为什么用任务通知？

- 队列适合“搬运数据”；
- 信号量适合“资源可用/事件计数”；
- 事件组适合“多条件组合，等任意/全部位”；
- 任务通知是单任务轻量同步，速度最快，适合一个任务通知另一个任务。

ADC 双缓冲用通知值低 8 位当位图：`PRODUCED(0/1)` 表示哪块缓冲有数据，`CONSUMING(0/1)` 表示正在消费哪块，`BIT_DMA` 表示 DMA 完成。生产者/消费者各维护自己的掩码，避免传统“生产者等消费者”需要额外队列。追问：`xTaskNotifyWait(0,0,...)` 不清位是什么意思？只读不消费，可以让消费者一次处理多块缓冲。

#### Q13. 从 ISR 调 FreeRTOS API 有什么约束？`xHigherPriorityTaskWoken` 是什么？

ISR 里只能用 `...FromISR` 版本，且要传 `pxHigherPriorityTaskWoken` 指针。API 内部如果因为这次发送/通知唤醒了更高优先级任务，会把这个变量置 `pdTRUE`，ISR 末尾用 `portYIELD_FROM_ISR(xHigherPriorityTaskWoken)` 决定是否触发 PendSV 上下文切换。这样既不在 ISR 里直接切换，又保证高优先级任务及时运行。

项目 `USART1_IRQHandler` 用 `xQueueSendFromISR(Q_YmodemReclength, ...)` 通知 OTA 任务，符合这个模式。

#### Q14. FreeRTOS 堆为什么选 heap_4？75KB 够不够？

heap_1 只能创建不能释放；heap_2 能释放但不合并碎片；heap_4 能合并相邻空闲块，适合动态创建/删除任务和队列；heap_5 支持多块不连续 RAM。

当前 `configTOTAL_HEAP_SIZE=75KB`。任务、队列、互斥量都会从这里分配。`configUSE_MALLOC_FAILED_HOOK=0` 意味着分配失败只会返回 NULL，代码里大部分检查了返回值，但少数没检查。面试答法：上线前应打开 malloc failed hook，统计 `xPortGetFreeHeapSize`/`xPortGetMinimumEverFreeHeapSize`，确认峰值余量。

#### Q15. `vTaskStartScheduler()` 为什么不返回？

正常情况它永远不会返回，因为它把当前执行上下文变成 idle 任务并启动调度。只有 FreeRTOS 初始化失败（比如堆不够创建 idle/timer 任务）时才会返回。项目 main 里循环打印 `Scheduler exited unexpectedly` 是兜底提示。

### D. UART / DMA / 中断

#### Q16. 为什么用“DMA + 空闲中断”接收不定长串口帧？NDTR 怎么算长度？

DMA 搬运不占 CPU，适合连续收流。但 DMA 不知道“一帧结束”，所以配 USART IDLE 中断：总线空闲一个字节时间后置 IDLE。DMA 初始 NDTR 是缓冲区长度，中断时 `NDTR - DMA_GetCurrDataCounter()` 就是已接收字节数。

关键细节：清除 IDLE 标志必须“读 SR 再读 DR”；如果不清，IDLE 会反复触发。还要处理 ORE（overrun），因为 DMA 没及时搬走时 USART 会丢字节。

#### Q17. `USART1_IRQHandler` 里 `rec_len` 是 `uint16_t`，但 `Q_YmodemReclength` 创建为 1 字节队列，会怎样？

`xQueueCreate(1, sizeof(uint8_t))` 每次入队只拷贝 1 字节，`xQueueSendFromISR(..., &rec_len, ...)` 会截断成低 8 位。当前命令长度固定为 3，所以“碰巧”能用。如果以后支持大于 255 字节的一帧，长度会错。正确做法：队列项大小用 `sizeof(uint16_t)` 或直接收进定长结构体。这是很好的代码审查题。

#### Q18. RXNE 回显分支和 DMA 接收会不会冲突？

`USART1_IRQHandler` 里有 `USART_GetITStatus(USART1, USART_IT_RXNE)` 后读 DR 回显的分支，但 OTA 模式只使能了 IDLE 中断，没有使能 RXNE 中断，所以实际不触发。一旦有人把 RXNE 中断打开，ISR 会抢在 DMA 之前读走数据，DMA 收到不完整帧，同时回写 TX 还可能污染主机侧协议。面试答法：接收要么 DMA、要么 RXNE 中断，不能同时抢同一个 DR；模式切换必须显式关掉另一边。

#### Q19. `YmodemUart_EnterRxMode` / `YmodemUart_RestoreDmaMode` 为什么这么设计？有什么隐患？

Ymodem 轮询 `SerialKeyPressed()` 读 `USART1->DR`，如果 DMA 还开着，会两边抢数据；如果 IDLE 还开着，会误触发命令队列。所以进 Ymodem 前必须关 DMA/IDLE、清残留标志，收完再恢复。

隐患：`YmodemUart_RestoreDmaMode` 最后把 `uartx_receive2idle_dma()` 注释掉了，只重配了 DMA 但没设置 M0AR/NDTR。当前能工作是因为下一个状态 `WaitReqDownload` 会先调用 `uartx_receive2idle_dma()` 重新武装；但如果未来改成“恢复后立刻等数据”，就会踩空。面试题可以问“这段注释掉以后为什么还能跑”。

#### Q20. 阻塞式 `printf`/`usart1_transmit` 在任务里有什么问题？

`USART1_SendByte` 等 TXE、`usart1_transmit` 逐字节等 TXE，都是忙等。115200 波特率下 1 字节约 87us，一屏日志几十字节就是几 ms 阻塞。如果日志量大、多个任务同时 printf，还会交错、丢行。更严重的是：OTA 协议复用同一根串口，日志输出会插进 Ymodem 数据流，直接破坏协议。这也是代码里注释提示“输出走 RTT”的原因。

#### Q21. 中断优先级 5 和 `configMAX_SYSCALL_INTERRUPT_PRIORITY=5` 是什么关系？

NVIC 用 Group 4，4 位抢占优先级，数值越小优先级越高。FreeRTOS 约定：能调用 ISR 版 API 的中断，其优先级数值必须 >= `configMAX_SYSCALL_INTERRUPT_PRIORITY`（5），即数字 5 到 15。USART1 配成 5，正好在安全边界上，可以调用 `xQueueSendFromISR`。如果把串口中断优先级配成 0-4，FreeRTOS 临界区屏蔽不到它，ISR 里调用 FreeRTOS API 可能破坏内核数据结构。

### E. I2C / EEPROM

#### Q22. 为什么用 GPIO 模拟 I2C？和硬件 I2C 比有什么取舍？

项目 `iic.c` 是纯软件 I2C，优点是引脚任意、不依赖硬件 I2C 状态机、调试直观、容易移植；缺点是占用 CPU、无法硬件时钟拉伸/超时、速率靠空循环估算、容易受中断影响。

硬件 I2C 优点是不占 CPU、有错误标志和超时；缺点是状态机复杂、需处理 NACK/BUSY 等状态。面试加分：说明软件 I2C 需要关中断或用 RTOS 锁保护时序，否则被打断会破坏位时序。

#### Q23. AT24C02 为什么写一页 8 字节？`ee_WriteBytes` 为什么要“等 ACK”？

AT24C02 页大小 8 字节，跨页连续写地址会自动回卷，所以跨页必须重新发地址。写操作后芯片进入内部写周期（典型 5ms），期间不应答任何命令；软件用“反复发设备地址直到收到 ACK”来探测写周期结束，比固定延时更高效。

读操作最后必须 NACK + Stop：NACK 告诉芯片“不要再发了”，否则芯片会继续把下一个字节推上总线。

#### Q24. 如果 EEPROM 应答异常，代码会怎样？软件 I2C 有哪些常见坑？

`ee_CheckOk()` 失败后会 Stop，避免总线被锁住。但 `i2c_WaitAck()` 本身没有超时，只有 `ee_WriteBytes` 里循环 1000 次兜底；如果 SDA 被从机拉死，`ee_ReadBytes` 可能卡死。

常见坑：上电时 SDA/SCL 时序未初始化、漏掉停止信号导致总线状态错乱、引脚没有外部上拉、开漏配置错误、位延时被更高优先级中断打断、主从速率不匹配、地址 A0/A1/A2 接法不同。恢复手段：连发 9 个时钟脉冲让从机释放 SDA，或者直接复位总线。

### F. SPI / 外部 Flash

#### Q25. 怎么判断 SPI 是 Mode 0？项目里 W25Q64 的时序对不对？

SPI Mode 由 CPOL 和 CPHA 决定。项目配置 `SPI_CPOL_Low`（空闲时钟低）和 `SPI_CPHA_1Edge`（第一个边沿采样），所以是 Mode 0，W25Q64 默认支持 Mode 0。采样沿与数据稳定顺序是 W25Q64 手册的硬要求，配错 Mode 通常表现为“第一个字节对、后面全错”。

追问：为什么读 Flash 时主机也要发 0xFF？SPI 全双工，从机输出需要一个移位时钟，主机发的字节就是时钟源。

#### Q26. W25Q64 为什么写前要 `Write Enable`？页、扇区、块擦除有什么区别？

NOR Flash 只能 1->0，不能 0->1，所以写之前必须擦成 0xFF。W25Q64 的 WEL（write enable latch）机制要求每次写/擦除前先发 0x06，防止误写。粒度：页 256B（编程单位）、4KB 扇区、64KB 块、整片。擦除必须按扇区/块，不能只擦一个字节。

项目 `W25Q64_WriteData` 用 4KB RAM 缓冲，攒满一个扇区才擦写，减少擦除次数和耗时。这是典型的“写缓冲合并”设计。

#### Q27. `SysTickUpTime` 从未递增，会导致什么问题？

`stm32f4xx_it.c` 只定义了 `volatile uint32_t SysTickUpTime = 0`，全工程没有任何地方对它 `++`。SPI 和 W25Q64 都用它做超时源，所以 `get_tick()` 永远是 0，超时判断永远不会成立：如果 Flash 忙死或 SPI 无响应，代码会无限等待。正确做法是在 `SysTick_Handler` 里统一 `SysTickUpTime++`，或直接使用 `xTaskGetTickCount()`。这是 3-5 年面试里很好的“读代码找 bug”题。

#### Q28. `flash_erase()` 里 `for (i = 0; i <= 12; i++)` 有什么问题？

`Flash_Sectorsize` 数组大小是 12，合法下标 0-11；`i <= 12` 会越界读 `Flash_Sectorsize[12]`。虽然大概率读到相邻内存，但属于未定义行为，可能误判扇区或触发 HardFault。另外 `STMFLASH_GetFlashSector(addr + size)` 当 `addr+size` 正好落在扇区边界时，可能多擦一个扇区。正确做法是 `i < FLASH_SECTOR_NUM`，并明确“包含结束地址”还是“不含”。

#### Q29. 在 APP 运行中擦写内部 Flash 有哪些限制？为什么升级拷贝通常由 Bootloader 做？

不能擦除“正在执行代码所在扇区”，因为 CPU 从 Flash 取指，擦除会让取指失败。当前 `copy_app_to_executable` 写 `0x08008000` 而 APP 运行在 `0x08010000`，扇区不重叠时可以运行，但仍有风险：中断向量、中断处理函数如果落在被擦扇区，一次中断就崩。可靠做法：擦写代码放 RAM 执行、临时禁用中断、或干脆由 Bootloader 在跳回 APP 前完成拷贝与校验。

#### Q30. 外部 Flash 频繁 OTA 会不会损坏？如何做磨损均衡？

W25Q64 每个扇区擦写寿命通常约 10 万次。手表升级次数不多，固定扇区也可接受；但如果做日志存储，就要做环形扇区分配和磨损均衡，例如每次写下一个扇区、记录当前扇区号。当前项目只在 OTA 时写升级包，风险低。

### G. OTA / 安全 / 可靠性

#### Q31. 这个项目的 OTA 整体架构是什么？如果升级途中断电会怎样？

架构是：APP 通过串口/Ymodem 把升级包写入外部 W25Q64，EEPROM 记录状态（0x11 下载中、0x22 下载完成、0x44 首次校验中），随后软复位让 Bootloader 把外部 Flash 内容拷贝到内部运行区，再跳 APP。APP 上电先读 EEPROM 状态，非 0 就复位或等待看门狗，避免带着半成品运行。

现状弱点：W25Q64 写满 4KB 才擦扇区，如果中途断电，扇区擦除可能没完成，需要下次重新下载；EEPROM 状态只有单字节，没有校验和备份，写一半断电可能读到任意值。面试答法：应增加状态双备份、写入校验和、升级包头带 CRC/签名、Bootloader 侧对拷贝结果做读回校验，失败回滚旧 APP。

#### Q32. Ymodem 协议具体是怎么工作的？

接收方先发 `C`（请求 CRC16），发送方先发 0 号文件名包（SOH + 文件名 + 长度 + 填充），双方 ACK/C 后发数据包。128 字节用 SOH，1024 字节用 STX；每个包有 1 字节序号 + 1 字节反码，接收方校验序号和 CRC16。收到 EOT 回 ACK，再收空文件名包结束会话；错误回 NAK 重传，最多 3 次；`CA CA` 表示中止。

项目里 `Receive_Packet` 同时做了序号、反码和 CRC 校验，丢一帧会 NAK 重传。追问：为什么文件大小写在文件名包而不是数据包头？因为接收方要先知道总大小，才能提前规划 Flash 空间和擦除。

#### Q33. CRC16 能防篡改吗？AES 在这里起什么作用？

CRC16 只能检随机错误，不能防恶意篡改，也没有抗碰撞能力。`boot_manager.c` 里的 AES-256-CBC 解密（IV 链式更新）用于把加密固件解开再写内部 Flash，能防“明文固件被直接读走/篡改后仍能跑”，但当前 Key/IV 硬编码在源码里，且没有消息认证码（MAC），攻击者拿到固件即可离线推出 Key 或直接重打包。

生产级方案：固件签名（RSA/ECDSA）+ 固件密文（AES-GCM/CTR+CMAC）+ 密钥存 OTP/安全元件/读保护；校验顺序是先验签，再解密，再 CRC/哈希校验，最后才允许写运行区。

#### Q34. 为什么 EEPROM 状态机要在 `main()` 里做“等看门狗复位”这种奇怪流程？

从代码看，Bootloader 写完新 APP 后希望 APP 自检一次：把状态设为 `0x44`（首次检查中），APP 起来后如果代码能跑到 main，说明向量表/基础运行 OK，就写回 `0x00` 并停在原地，让 Bootloader 的看门狗复位后跳新 APP。这样“APP 是否能运行”由 APP 自己证明，比 Bootloader 盲目跳转更稳。

但这里也有风险：`while(1){}` 是纯空转，如果看门狗没有喂/复位配置错误，设备会永远卡住；应该加超时后主动复位。另外，把“首次运行验证”做成独立任务并点亮 LED 会更可靠。

#### Q35. 内部 Flash 扇区大小和擦写流程是怎样的？为什么 `flash_write` 前必须擦除？

STM32F411 内部 Flash 扇区：前 4 个 16KB，接着 1 个 64KB，后面都是 128KB。擦除粒度是扇区，编程粒度是 32 位字（或 16 位半字/字节）。`flash_erase()` 先解锁、按地址算扇区、`FLASH_EraseSector`、再上锁；`flash_write()` 先解锁、`FLASH_ProgramWord`、再上锁。写前必须擦除，因为 Flash 位只能 1->0。

追问：为什么每写一个字都解锁/上锁？正确但低效；批量写入时应一次解锁、多次编程、最后锁。还要检查 `FLASH_GetStatus` 和清除 EOP/OPERR/WRPERR 标志。

#### Q36. 如何设计一个“不会变砖”的 OTA？

核心是 A/B 双 bank 或“暂存区 + 校验 + 回滚”：

1. 新固件先完整写入暂存区（外部 Flash 或内部第二分区）；
2. 对整包做 CRC/哈希/签名校验；
3. 把“新固件待应用”标志写入可靠存储（双备份）；
4. Bootloader 拷贝新固件到运行区并读回校验；
5. 跳新 APP，APP 自检（跑通关键外设/任务），超时未确认则标记失败；
6. 失败回滚旧固件，成功后清除标志；
7. 全程有看门狗，任何一步超时自动复位重试。

当前项目的 EEPROM 状态机是其中一部分，但缺签名、读回校验、明确的成功确认和回滚逻辑。

### H. 工程实践与代码评审

#### Q37. printf/easylogger 与 OTA 协议共用串口，为什么是设计缺陷？怎么改？

日志和协议共用一个物理串口会互相污染：Ymodem 接收时任何 `printf` 都会插进数据流，主机端 CRC 必然失败；命令模式下日志也可能被当成命令。改法：
1. 日志输出到独立调试串口或 RTT；
2. OTA 协议专用串口，协议收发期间关闭日志输出；
3. 日志加互斥锁，避免多任务交错；
4. 需要保留日志时，用带时间戳的 RAM 环形缓冲，空闲时再冲刷。

#### Q38. 如何验证一个嵌入式项目的稳定性和内存占用？

- 打开 FreeRTOS 栈溢出检测和 malloc failed hook；
- 用 `uxTaskGetStackHighWaterMark()`、`xPortGetFreeHeapSize()`、`xPortGetMinimumEverFreeHeapSize()` 做长稳统计；
- 串口协议压测：断帧、粘帧、超时、拔线、断电；
- Flash 擦写掉电测试；
- 静态分析：`-Wall -Wextra -Werror`、clang-tidy、Coverity；
- 硬件在环 + 自动化测试脚本；
- 长时间跑 `HardFault`/`stack overflow` 监控任务，异常时上报并复位。

#### Q39. 给这个项目按优先级列改进建议，怎么排？

1. 修复 `SysTickUpTime` 不递增，否则 SPI/Flash 超时全部失效；
2. 修复 `Q_YmodemReclength` 队列元素大小与发送类型不一致；
3. 修复 `flash_erase()` 数组越界和扇区边界判断；
4. 串口日志与 OTA 协议隔离，避免 Ymodem 被日志污染；
5. OTA 状态写 EEPROM 前增加备份/校验，加看门狗超时；
6. 打开 FreeRTOS 栈溢出检查，用 HighWaterMark 校准任务栈；
7. 把 W25Q64/SPI 阻塞操作拆出 OTA 任务或用状态机让出 CPU；
8. 补上 APP 升级包的签名校验，再考虑密钥管理。

## 六、一句话总结

这个 APP 项目的实际骨架是“FreeRTOS 两个任务 + 串口 DMA/IDLE 命令接收 + Ymodem 下载到外部 Flash + EEPROM 状态机 + 上电自检复位”，其余 ADC/按键/环形缓冲/内部 Flash 拷贝大多是编译在内但未启用的功能模块。面试准备时，重点把“Bootloader 跳转契约、FreeRTOS 同步原语、DMA+IDLE 不定长接收、Ymodem 校验、Flash 擦写约束、OTA 掉电与安全”这六条主线吃透，就能覆盖这个项目 80% 的知识点。
