# AHT21 桥接驱动 · IIC 接口挂载问题修改建议（v2：改用 IIC 开头的软件 IIC）

> 适用工程：`watch_projects/app`
> 目标：让 AHT21 桥接驱动的 `iic_driver_interface_t` 正确挂载 **IIC 开头的软件 IIC 函数**并跑通温湿度读取
> 原则：**本文档只给建议与改后代码，不自动改你的源码**，请你照着手动落地。

---

## 0. 修订说明（相对 v1 的关键变化）

v1 误以为可用的是 `iic.c` 里 `i2c_*` 那组（硬编码 PB8/PB9、`void` 无 bus 形参）。你的实际意图是
**用 `IIC` 开头的那组函数**。澄清如下，避免再次踩坑：

- `IICStart / IICStop / IICWaitAck / IICSendAck / IICSendNotAck / IICSendByte / IICReceiveByte / IICInit`
  在代码注释里写的是"硬件 IIC"，**其实仍是软件 IIC（位带）**，只是多了一个 `iic_bus_t *bus` 形参来
  描述 SDA/SCL 引脚，并非 MCU 的硬件 I2C 外设。
- 这组函数的语义与 `i2c_*` **正好相反**，适配器里必须做映射：
  - `IICWaitAck(bus)`：返回 `1` = 收到 ACK（成功），`0` = 超时/NACK；
    而 `i2c_WaitAck()` 是 `0`=ACK、`1`=NACK。所以 `IICWaitAck` 成功对应 `AHT21_OK`。
  - `IICReceiveByte(bus)`：**直接返回字节值**（不是出参指针）；而接口槽位 `pf_iic_receive_byte`
    要求"通过 `uint8_t * const` 出参带回"。所以适配器里要写 `*p_out = IICReceiveByte(bus)`。

> ⚠️ **为什么不能直接把 `IIC*` 塞进结构体**（这是你原 `FreeRTOS_tasks.c:300` 的写法，编译/运行都错）：
> 1. 类型不兼容：`IICInit` 是 `void (iic_bus_t*)`，而槽位是 `void (*)(void*)` → 函数指针类型不符，编译报错；
> 2. 即使强转，AHT21 调用时一律传 `NULL` 作 bus，而 `IIC*` 会拿 `NULL` 去解引用 `bus->IIC_SDA_PIN` → 直接崩溃；
> 3. `IICReceiveByte` 返回字节、`IICSendByte` 收 `iic_bus_t*`+`unsigned char`，与槽位"返回 void + 2 个形参"完全对不上。
> 因此**必须加一层薄适配器**：适配器持有静态 `iic_bus_t`，把 `IIC*` 包成接口要求的一致签名，忽略 AHT21 传来的 `NULL` 形参。

---

## 1. 问题定位（为什么挂载会出问题）

调用链：`temp_humi_handler_thread` → `bsp_temp_humi_handler_inst` → `bsp_temp_humi_handler_init` →
`aht21_instantiate(... handler->iic_driver_interface ...)`。
`iic_driver_interface` 在 `Core/Src/FreeRTOS_tasks.c` 里被填充，最终喂给 AHT21 driver。

### R1. 结构体缺 `pf_critical_enter` / `pf_critical_exit`（编译不过）
`BSP/AHT21/bsp_aht21_driver.h` 的 `iic_driver_interface_t`（软件 IIC 分支）里这两个成员是被注释掉的，
但 `bsp_aht21_driver.c` 在 `#ifndef HARDWARE_IIC` 下大量调用了它们
（`bsp_aht21_driver.c:72, 94, 152, 168, 215, 239, 280, 319, 352, 405`）。
→ **编译错误：结构体没有该成员。**

### R2. 软件分支 `pf_iic_wait_ack` 返回类型错（编译不过）
`bsp_aht21_driver.h` 软件分支把 `pf_iic_wait_ack` 声明为 `void (*)(void *)`，
但 `__read_id` 里用其返回值作了比较：`if (AHT21_OK == pf_iic_wait_ack(NULL))`（`bsp_aht21_driver.c:83`）。
→ **编译错误：`void` 值不能参与比较。**

### R3. 原挂载块直接把 `IIC*` 塞进结构体（类型错 + 漏成员）
`FreeRTOS_tasks.c:300-307` 原写法：
```c
iic_driver_interface_t iic_driver_interface = {
    .pf_iic_init = &IICInit,
    .pf_iic_deinit = NULL,
    .pf_iic_start = IICStart,
    .pf_iic_stop = IICStop,
    .pf_iic_send_byte = IICSendByte,
    .pf_iic_receive_byte = IICReceiveByte,   // ← 返回 unsigned char、只收 1 参，与槽位完全不符
};
```
- `IICInit/IICStart/...` 都是 `void (iic_bus_t*)`，与 `void (*)(void*)` 类型不兼容 → 编译报错；
- **漏填** `pf_iic_wait_ack / send_ack / send_no_ack / critical_enter / critical_exit` → 全 0（NULL），
  AHT21 一调用这几个就跳 NULL 指针崩溃。

### R4. 输入参数从未真正接上（链路没通）
- `temp_humi_input_arg` 被 `= {0}` 后，`iic_driver_interface / timebase / os_queue / os_delay / pf_callback` 全 NULL；
- `app_task_create` 里 `xTaskCreate(temp_humi_handler_thread, ..., NULL)` —— **参数传的是 NULL**，
  而线程开头 `if (NULL == argument) { log_e(...); return; }`（`bsp_aht21_handler.c:289`）
  → 线程一启动就直接退出，handler 永远不初始化。

---

## 2. 修复策略（Adapter 适配器模式，底层用 IIC 开头的软件 IIC）

保持 **AHT21 driver 不动**（它是对统一 `iic_driver_interface_t` 契约的"客户"），新增一层**适配器**：
把 `IIC*` 软件 IIC（带 `iic_bus_t`、绑定 PB8/PB9）包成接口要求的一致签名。

- 适配器持有静态 `iic_bus_t g_aht21_iic_bus`（SCL=PB8、SDA=PB9）；
- 每个 wrapper 忽略 AHT21 传来的 `void *p`（恒为 `NULL`），改用 `&g_aht21_iic_bus` 调 `IIC*`；
- `pf_iic_wait_ack` 把 `IICWaitAck` 的 `1`=ACK 映射为 `AHT21_OK`；
- `pf_iic_receive_byte` 把 `IICReceiveByte` 的返回值经出参 `*p_out` 带回；
- 同时把 `bsp_aht21_driver.h` 的软件分支结构体**最小修正**：补 `critical_enter/exit`、把 `wait_ack` 返回类型改为 `aht21_status_t`。

这样 AHT21 driver 的调用点（`pf_iic_*(NULL)`、`if (AHT21_OK == pf_iic_wait_ack(NULL))`、`pf_critical_enter()`）
**无需改动即可编译通过**，真实 IIC 引擎成为适配器背后的实现细节——契合桥接模式"驱动引擎可替换"的初衷。

---

## 3. 修改后的代码（请手动应用）

### 3.1 改 `BSP/AHT21/bsp_aht21_driver.h`（最小修正，仅 2 处）

文件约 34~48 行的软件 IIC 分支：

```c
#ifndef HARDWARE_IIC        // software iic
typedef struct {
    void (*pf_iic_init)           (void *);
    void (*pf_iic_deinit)         (void *);
    void (*pf_iic_start)          (void *);
    void (*pf_iic_stop)           (void *);
    aht21_status_t (*pf_iic_wait_ack)   (void *);   /* 改：原 void，但 __read_id 用其返回值与 AHT21_OK 比较 */
    void (*pf_iic_send_ack)       (void *);
    void (*pf_iic_send_no_ack)    (void *);
    void (*pf_iic_send_byte)      (void *, const uint8_t);
    void (*pf_iic_receive_byte)   (void *, uint8_t * const);

    /* 补：软件 IIC 分支的驱动代码(#ifndef HARDWARE_IIC)会调用，原被注释 → 编译不过 */
    aht21_status_t (*pf_critical_enter) (void);
    aht21_status_t (*pf_critical_exit)  (void);
} iic_driver_interface_t;
#endif /* end of HARDWARE_IIC */
```

> 只动了 `pf_iic_wait_ack` 的返回类型和补了 `critical_enter/exit`，其余保持原样，风险最小。
> AHT21 driver 内部调用点与此签名完全匹配，**无需改 `bsp_aht21_driver.c`**。

### 3.2 新增适配器头文件 `BSP/AHT21/bsp_aht21_iic_adapter.h`

```c
#pragma once
#ifndef BSP_AHT21_IIC_ADAPTER_H
#define BSP_AHT21_IIC_ADAPTER_H

#include "stdint.h"
#include "iic.h"                  /* 提供 iic_bus_t 与 IIC* 软件 IIC 函数 */
#include "bsp_aht21_driver.h"     /* 提供 aht21_status_t / iic_driver_interface_t */

/* AHT21 使用的 IIC 总线实例（软件 IIC，PB8=SCL, PB9=SDA），由适配器持有 */
extern iic_bus_t g_aht21_iic_bus;

/* 适配后的 IIC 驱动接口实例，直接挂给 AHT21 handler 使用 */
extern iic_driver_interface_t g_aht21_iic_driver_interface;

#endif /* BSP_AHT21_IIC_ADAPTER_H */
```

### 3.3 新增适配器实现 `BSP/AHT21/bsp_aht21_iic_adapter.c`

```c
#include "bsp_aht21_iic_adapter.h"
#include "stm32f4xx.h"   /* RCC_AHB1PeriphClockCmd / __disable_irq / __enable_irq (CMSIS) */

/* ---------- 总线实例：把软件 IIC 绑定到 PB8(SCL) / PB9(SDA) ---------- */
iic_bus_t g_aht21_iic_bus = {
    .IIC_SDA_PORT = GPIOB,
    .IIC_SCL_PORT = GPIOB,
    .IIC_SDA_PIN  = GPIO_Pin_9,   /* SDA = PB9 */
    .IIC_SCL_PIN  = GPIO_Pin_8,   /* SCL = PB8 */
};

/* ---------- 临界区：保护软件 IIC 位带不被中断打断 ----------
 * 简单稳妥方案：关/开全局中断。位带期间很短（微秒级），且 80ms 测量等待在临界区之外，
 * 所以关中断不会阻塞系统太久。如需 FreeRTOS 临界区，把下面两函数体替换为
 *   taskENTER_CRITICAL();   /   taskEXIT_CRITICAL();   （需 #include "FreeRTOS.h" "task.h"） */
static aht21_status_t adapter_critical_enter(void) {
    __disable_irq();
    return AHT21_OK;
}
static aht21_status_t adapter_critical_exit(void) {
    __enable_irq();
    return AHT21_OK;
}

/* ---------- 各 IIC 原语适配器（忽略 void* 形参，固定使用 g_aht21_iic_bus） ---------- */
static void adapter_iic_init(void * p) {
    (void)p;
    /* IICInit 内部没有使能 GPIOB 时钟（F4 的 GPIO 在 AHB1），这里补上，否则 PB8/PB9 不动 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    IICInit(&g_aht21_iic_bus);
    /* IICInit 不会发 STOP 复位总线，补一个停止态保证总线空闲 */
    IICStop(&g_aht21_iic_bus);
}
static void adapter_iic_deinit(void * p) {
    (void)p;
    IICStop(&g_aht21_iic_bus);   /* 软件 IIC 无硬件注销动作，保持总线空闲即可 */
}
static void adapter_iic_start(void * p) {
    (void)p;
    IICStart(&g_aht21_iic_bus);
}
static void adapter_iic_stop(void * p) {
    (void)p;
    IICStop(&g_aht21_iic_bus);
}
/* IICWaitAck 返回 1=收到ACK(成功), 0=超时/NACK —— 映射到 aht21_status_t */
static aht21_status_t adapter_iic_wait_ack(void * p) {
    (void)p;
    return IICWaitAck(&g_aht21_iic_bus) ? AHT21_OK : AHT21_ERRORTIMEOUT;
}
static void adapter_iic_send_ack(void * p) {
    (void)p;
    IICSendAck(&g_aht21_iic_bus);
}
static void adapter_iic_send_no_ack(void * p) {
    (void)p;
    IICSendNotAck(&g_aht21_iic_bus);
}
static void adapter_iic_send_byte(void * p, const uint8_t byte) {
    (void)p;
    IICSendByte(&g_aht21_iic_bus, (unsigned char)byte);
}
/* IICReceiveByte 直接返回字节，按接口契约经出参带回 */
static void adapter_iic_receive_byte(void * p, uint8_t * const p_out) {
    (void)p;
    *p_out = (uint8_t)IICReceiveByte(&g_aht21_iic_bus);
}

/* ---------- 接口实例（11 个成员一个都不能少，否则为 NULL 调用即崩） ---------- */
iic_driver_interface_t g_aht21_iic_driver_interface = {
    .pf_iic_init          = adapter_iic_init,
    .pf_iic_deinit        = adapter_iic_deinit,
    .pf_iic_start         = adapter_iic_start,
    .pf_iic_stop          = adapter_iic_stop,
    .pf_iic_wait_ack      = adapter_iic_wait_ack,
    .pf_iic_send_ack      = adapter_iic_send_ack,
    .pf_iic_send_no_ack   = adapter_iic_send_no_ack,
    .pf_iic_send_byte     = adapter_iic_send_byte,
    .pf_iic_receive_byte  = adapter_iic_receive_byte,
    .pf_critical_enter    = adapter_critical_enter,
    .pf_critical_exit     = adapter_critical_exit,
};
```

> 依赖说明：`IIC*` 内部调用 `delay_us()`，它在 `Core/Src/timer.c` 已有实现、`Core/Inc/timer.h` 已声明，
> 且 `iic.h` 已 `#include "timer.h"`，所以无需额外提供。

### 3.4 改 `Core/Src/FreeRTOS_tasks.c` —— 正确接线

**(a) 替换原 299~309 行的 iic 初始化块：**
```c
#if 1   // temp_humi_thread
#include "bsp_aht21_iic_adapter.h"
#include "osal_freertos.h"      /* 提供 osal_time_base_freertos / osal_queue_freertos / osal_delay_freertos */

/* AHT21 的 IIC 接口实例由适配器提供（内部用 IIC* 软件 IIC + PB8/PB9） */
extern iic_driver_interface_t g_aht21_iic_driver_interface;

/* 填充 handler 所需的全部接口。
   原代码 = {0} 后从未给字段赋值 → iic 接口等全为 NULL，导致 handler 初始化直接返回。 */
temp_humi_handler_all_input_arg_t temp_humi_input_arg = {
    .iic_driver_interface = &g_aht21_iic_driver_interface,
    .timebase_interface   = &osal_time_base_freertos,
    .os_queue_interface   = &osal_queue_freertos,
    .os_delay_interface   = &osal_delay_freertos,
    .pf_callback          = NULL,   /* 需要结果回调时填你的函数 */
};
#endif
```

**(b) 在 `app_task_create()` 中，把线程参数从 `NULL` 改为 `&temp_humi_input_arg`：**
```c
    task_create_ret = xTaskCreate(temp_humi_handler_thread, "temp_humi_task",
                                  256, &temp_humi_input_arg, 3, NULL);
    if (task_create_ret != pdPASS) {
        printf("temp_humi_task create failed\r\n");
    }
```
> 注：原 `xTaskCreate(..., NULL)` 传了 NULL 参数，线程开头 `if (NULL == argument) return;`
> 会直接退出，handler 永远不初始化。256 栈深若不够（elog + IIC 位时序 + 队列），可酌情加大到 512。

### 3.5 CMake 注意
若 `CMakeLists.txt` 对 BSP 源码是**显式列举**，请把新增的
`BSP/AHT21/bsp_aht21_iic_adapter.c` 加进源文件列表；
若用 `file(GLOB ...)` 通配，则自动包含，无需改动。

---

## 4. 验证步骤

1. **编译**：原来的两类错误应消失——
   - `void value not ignored as it ought to be` / comparison error（旧 `pf_iic_wait_ack` 返回 void 被比较）；
   - `no member named 'pf_critical_enter' in 'iic_driver_interface_t'`。
   同时不应再出现 `IICInit` / `IICReceiveByte` 等"函数指针类型不兼容"的报错。
2. **运行时**：上电后 `temp_humi_handler_thread` → `aht21_instantiate` → `aht21_init` →
   `adapter_iic_init`（开 GPIOB 时钟 + `IICInit` + `IICStop`）→ `__read_id` 向 `0x71` 发读 ID 命令。
   用逻辑分析仪/串口看 PB8(SCL)/PB9(SDA) 是否有 START + 地址字节 + ACK。
3. **结果**：`aht21_init` 日志打印 `aht21_init success`、ID 应为 `0x18`；
   之后 `get_temp_humi(..., TEMP_HUMI_EVENT_BOTH, ...)` 回调里能看到 `temp/humi` 数值。
4. **排查**：若读不到 ACK，优先确认 `adapter_iic_init` 里的 `RCC_AHB1PeriphClockCmd(GPIOB)` 是否生效、
   以及 PB8/PB9 外部是否有上拉电阻（软件开漏必须配上拉）。

---

## 5. 顺带提醒（非本次挂载阻塞，但建议一并修）

- **`pf_read_id` 签名 bug**：`bsp_aht21_driver.h:84` 声明 `aht21_status_t (*pf_read_id)(void * const)`，
  但实现 `aht21_read_id(uint8_t *p_id)` 用出参回传 ID，强转 `aht21_status_t (*)(void * const)` 把出参掩盖了。
  应改成 `aht21_status_t (*pf_read_id)(void * const, uint8_t * const p_id_out)` 并同步实现。
- **`pf_read_humidity` 签名漂移**：声明 `(void *, float * const)`，实现 `aht21_read_humidity(bsp_aht21_driver_t*, int32_t*)`，
  `float*` vs `int32_t*`、且缺 self 参数，目前是空实现，建议统一。
- **多套状态枚举**：`osal_status_t` / `led_status_t` / `aht21_status_t` / `temp_humi_status_t`
  各自定义一套，且 `RESERVED` 哨兵值混用 `0xFF` 与 `0x7FFFFFFF`。建议统一收敛到 `osal_status_t`（`0=OK`）。
