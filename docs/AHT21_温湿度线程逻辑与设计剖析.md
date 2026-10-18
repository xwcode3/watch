# AHT21 温湿度桥接驱动 · 线程逻辑与设计方法剖析

> 适用工程：`watch_projects/app`
> 代码版本：基于当前 `BSP/AHT21/`、`Core/Src/FreeRTOS_tasks.c`、`System/OSAL/` 实际状态
> 目标：① 把温湿度线程的**运行逻辑**从头到尾捋清；② 把**沿途每个函数**讲透；③ 站在**设计方法**角度评价这套驱动；④ 沉淀成可复用的文档。

---

## 0. 结论速览（先讲结论）

- **跑通的本质**：`AHT21` 桥接驱动已经形成一条「生产者-消费者」式的温湿度读取链路——一个低优先级任务每秒投递一次"读事件"到队列，温湿度线程阻塞取事件后，经 `get_temp_humi → pf_read_data → aht21_read_data` 走完一次软件 I2C 事务，把 20-bit 原始值解码成温湿度浮点。
- **桥接模式是真的落了地**：驱动层（`bsp_aht21_driver_t`）只认 `iic_driver_interface_t` 这个抽象 I2C 契约，不关心底层是软件 I2C 还是硬件 I2C、引脚是什么；Handler 层（`bsp_temp_humi_handler_t`）把驱动接到 FreeRTOS 的队列/时基/回调上。两者通过 `instantiate` 时挂载的函数指针表（`pf_*`）解耦。
- **但有几个实打实的坑**：CRC 校验函数写了却没调用；`pf_read_temperature`/`pf_read_humidity` 没挂载（单模式会空指针崩）；全局状态 `g_inited` 破坏"对象化"；`aht21_init` 里有个 `&&`/`&` 优先级 bug；测量等待用的是忙等 `delay_ms` 而非 OSAL 延时；当前 `pf_callback = NULL`，读到的数据只进 elog、没有真正消费者。
- **数据"读得到" ≠ "被使用"**：你现在看到温湿度，是因为 `aht21_read_data` 内部用 `log_i` 打印了；真正要把数据拿去显示/控制，得在 `temp_humi_input_arg.pf_callback` 或 `event.pf_callback` 上挂回调函数。

---

## 1. 架构分层（四层 + 两个任务）

```
┌────────────────────────────────────────────────────────────────────┐
│ App / 集成层   Core/Src/FreeRTOS_tasks.c                            │
│   · 定义具体引脚 (PB13/PB14)、具体 I2C 引擎 (IIC*)、具体 OSAL 实例   │
│   · app_task_create() 创建两个任务                                   │
└───────────────┬───────────────────────────────┬────────────────────┘
                │ xTaskCreate                    │ xTaskCreate
                ▼                                ▼
┌───────────────────────────┐      ┌──────────────────────────────────┐
│ 生产者任务                  │      │ 消费者线程 (本篇主角)             │
│ send_temp_humi_event (prio1)│      │ temp_humi_handler_thread (prio3)  │
│  每 1s → bsp_temp_humi_read │      │   └ bsp_temp_humi_handler_inst    │
│     → queue_put             │      │   └ for(;;) queue_get → get_temp  │
└───────────────┬─────────────┘      │        _humi → pf_read_data      │
                │                     └────────────────┬─────────────────┘
                │               队列 (osal_queue_freertos / xQueue)      │
                │                     ┌────────────────▼─────────────────┘
                ▼                     ▼
┌────────────────────────────────────────────────────────────────────┐
│ Handler 层  BSP/AHT21/bsp_aht21_handler.c/.h                         │
│   · 设备无关的"业务编排"：队列、时基、lifetime 缓存、回调              │
│   · 持有 bsp_aht21_driver_t *（仍直接耦合具体驱动，见 §6）           │
└───────────────────────────────┬────────────────────────────────────┘
                                │ 调用 pf_* 函数指针
                                ▼
┌────────────────────────────────────────────────────────────────────┐
│ Driver 层   BSP/AHT21/bsp_aht21_driver.c/.h                         │
│   · 纯设备逻辑：AHT21 协议（0xAC 触发、状态轮询、6字节解码）          │
│   · 只依赖 iic_driver_interface_t（抽象 I2C）+ OSAL 时基/延时        │
└───────────────────────────────┬────────────────────────────────────┘
                ┌───────────────┴────────────────┐
                ▼                                ▼
┌──────────────────────────┐      ┌──────────────────────────────────────┐
│ 软件 I2C 引擎             │      │ OSAL 抽象  System/OSAL/osal_freertos.c│
│ Core/Src/iic.c (IIC*)     │      │  get_tick_ms / queue_create|put|get   │
│  PB13/SCL, PB14/SDA       │      │  （critical 当前被注释，见 §7）        │
└──────────────────────────┘      └──────────────────────────────────────┘
```

> 关于 I2C 引擎的接线演变：早期版本（`docs/AHT21_IIC接口挂载修改建议.md`）曾计划加一层 `bsp_aht21_iic_adapter` 适配器，把 `IIC*` 包成 `void*` 签名。当前代码已直接把 `iic_driver_interface_t` 的 `pf_iic_*` 改成 `iic_bus_t *bus` 签名，并让 `FreeRTOS_tasks.c` 直接填 `IICInit/IICStart/...` + `.iic_bus = &iic_bus_aht21`。**因为签名已经对齐，不再需要独立适配器文件**——这是比旧方案更干净的收敛。

---

## 2. 完整调用链（从开机到一次读数）

```
app_task_create()                              [FreeRTOS_tasks.c:373]
 ├─ xTaskCreate(temp_humi_handler_thread, …, &temp_humi_input_arg, 3)   ← 消费者
 └─ xTaskCreate(send_temp_humi_event, …, NULL, 1)                      ← 生产者

── 生产者（每 1s 一次）─────────────────────────────────────────────
send_temp_humi_event()                         [FreeRTOS_tasks.c:343]
 └─ bsp_temp_humi_read(&event)                [handler.c:359]
     └─ os_queue_interface->queue_put(...)    [osal_freertos.c:72]

── 消费者线程（启动一次，永久循环）─────────────────────────────────
temp_humi_handler_thread(argument)            [handler.c:288]
 ├─ bsp_temp_humi_handler_inst(...)           [handler.c:109]
 │   └─ bsp_temp_humi_handler_init(...)       [handler.c:40]
 │       ├─ os_queue_interface->queue_create(10, sizeof(event), &q)
 │       └─ aht21_instantiate(...)            [driver.c:558]
 │           ├─ 挂载 pf_init/pf_deinit/pf_read_id/pf_read_data/pf_sleep/pf_wakeup
 │           └─ aht21_init(...)               [driver.c:143]
 │               ├─ delay_ms(300)             [timer.c, TIM2 忙等]
 │               ├─ pf_iic_init(p_iic_bus) → IICInit()
 │               ├─ 读状态 + 发 0xBE/0x08/0x00 软复位
 │               └─ g_inited = AHT21_INITED
 ├─ __mount_hanlder(&instance)                [handler.c:24]  全局指针
 ├─ __mount_callback(&event, input_arg->pf_callback)
 │
 └─ for (;;)                                  ← 永久循环
     ├─ queue_get(…, &event, MY_MAX_DELAY)    ← 阻塞等事件 [handler.c:327]
     ├─ get_temp_humi(&instance, event, &temp, &humi)  [handler.c:185]
     │   └─ (lifetime 到期?) → pf_read_data(...)        [driver.c:304]
     │       ├─ 发 0xAC 0x33 0x00 触发测量
     │       ├─ delay_ms(80) + 轮询 status.busy
     │       ├─ 读 6 字节 (status + 5 data)
     │       └─ 20-bit 解码 → *p_humidity / *p_temperature
     └─ event.pf_callback(&temp, &humi)        ← 当前为 NULL，仅 elog 打印
```

---

## 3. 逐函数讲解

### 3.1 集成层 · `Core/Src/FreeRTOS_tasks.c`

| 函数 | 行 | 职责 | 关键点 |
|------|----|------|--------|
| `app_task_create()` | 373 | 创建温湿度两个任务 | `temp_humi_task` 栈 256、优先级 3；`send_task` 栈 256、优先级 1。**参数 `&temp_humi_input_arg` 必须传对**——旧版传 `NULL` 会导致线程 `argument` 判空直接 `return`，handler 永不初始化（旧文档 R4 已记过）。 |
| `send_temp_humi_event()` | 343 | 生产者 | 先 `vTaskDelay(1000)` 等队列建好，之后 `while(1)` 每 `vTaskDelay(1000)` 调一次 `bsp_temp_humi_read`。事件 `type = TEMP_HUMI_EVENT_BOTH`、`lifetime = 1000`、`pf_callback = NULL`。 |
| (全局对象) `iic_bus_aht21` | 304 | 总线描述 | `{ SCL=PB14, SDA=PB13 }`（注意：与旧适配文档的 PB8/PB9 不同，当前已改引脚）。 |
| (全局对象) `iic_driver_interface` | 312 | I2C 抽象实例 | 直接挂 `IICInit/IICStart/IICStop/IICSendByte/IICReceiveByte/IICWaitAck/IICSendAck/IICSendNotAck` + `.iic_bus` + `.pf_critical_interface = &osal_critical_freertos`。**签名与驱动期望的 `iic_bus_t*` 一致**，所以无需适配器。 |
| (全局对象) `temp_humi_input_arg` | 325 | Handler 输入 | 填齐 `iic_driver_interface / timebase / os_queue / os_delay / pf_callback`。`pf_callback = NULL` 是"数据只打印不消费"的根因。 |

### 3.2 Handler 层 · `BSP/AHT21/bsp_aht21_handler.c`

| 函数 | 行 | 职责 | 关键点 |
|------|----|------|--------|
| `temp_humi_handler_thread(arg)` | 288 | 线程主体 | 在**栈上**建 `bsp_aht21_driver_t` 和 `bsp_temp_humi_handler_t` 局部实例（不是 malloc），`p_aht21_instance` 指向局部驱动对象；然后 `inst` → `__mount_hanlder` → `__mount_callback` → 永久 `for(;;)`。注意：局部对象生命周期 = 线程生命周期，没问题，但全局 `gp_temp_humi_instance` 也指向它，二者必须同步。 |
| `bsp_temp_humi_handler_inst(h, arg)` | 109 | 实例化 Handler | 一堆 `NULL` 校验（input_arg、timebase、iic、queue、delay），把接口搬进 handler 实例，再调 `bsp_temp_humi_handler_init`；成功后置 `is_initated = TEMP_HUMI_INITIATED`。 |
| `bsp_temp_humi_handler_init(h)` | 40 | 建队列 + 实例化驱动 | `queue_create(10, sizeof(temp_humi_event_t), &handle)`；随后 `aht21_instantiate(...)` 把 I2C 接口/时基/延时透传给驱动。**这里把"具体驱动"与"具体 OS 队列"第一次接在一起**——是桥接的"桥墩"。 |
| `get_temp_humi(h, event, *temp, *humi)` | 185 | 读数 + lifetime 缓存 | 用 `timebase_interface->get_tick_ms` 取当前 tick；按 `event.type` 分发：`TEMP`/`HUMI`/`BOTH`。`BOTH` 分支用 `>=` 比较 `(now - last_tick) >= lifetime`（注释里专门纠正过 `>` 会导致读频减半的 bug）。读成功才更新 `last_*_tick`，失败不更新→下次事件立即重试，避免拿旧值当新数据。 |
| `__mount_callback(event, cb)` | 265 | 挂回调 | 把回调存进 event；`cb==NULL` 时把 `event->pf_callback` 置 NULL 并返回 2（仅告警）。 |
| `__mount_hanlder(inst)` | 24 | 全局挂载 | `gp_temp_humi_instance = inst`，供 `bsp_temp_humi_read` 无参访问。 |
| `bsp_temp_humi_read(event)` | 359 | 对外 API（生产者用） | 校验 `gp_temp_humi_instance` 非空且已初始化，再 `queue_put`。这是 APP 层唯一要调用的温湿度入口。 |

> ⚠️ **`temp_humi_event_t` 里的 `float *temperature / float *humidity` 指针字段是"死字段"**：从 `send_temp_humi_event` 到 `temp_humi_handler_thread`，这两个指针从未被赋值（保持 NULL），`get_temp_humi` 也完全不用它们——真正的数据走的是函数局部的 `temp/humi` + 回调。这是结构性冗余，容易让人误以为要往 event 里填指针。

### 3.3 Driver 层 · `BSP/AHT21/bsp_aht21_driver.c`

| 函数 | 行 | 职责 | 关键点 |
|------|----|------|--------|
| `aht21_instantiate(...)` | 558 | 实例化驱动（桥接核心） | 先判 `IS_INITED`；再把 `pf_init/pf_deinit/pf_read_id/pf_read_data/pf_sleep/pf_wakeup` 指向具体实现；最后调 `aht21_init`。**这就是"把具体函数挂到 ops 表"的动作**——上层之后只调 `pf_read_data`，不关心里面是谁。 |
| `aht21_init(...)` | 143 | 驱动初始化 | `delay_ms(300)` 上电稳定 → `pf_iic_init(p_iic_bus)`（开 GPIOB 时钟 + 配开漏 + STOP 复位总线）→ 读状态、若校准位未置则发 `0xBE/0x08/0x00` 软复位 → `g_inited = AHT21_INITED`。**注意：末尾的 `__read_id` 被整段注释掉了**，所以现在初始化并不校验 ID，只要 I2C 通就认为 OK。 |
| `aht21_read_data(...)` | 304 | **核心：一次完整测量+读取+解码** | 见 §4 单独拆。 |
| `aht21_read_status(...)` | 248 | 读 1 字节状态 | 发 `0x71`+ACK 后收 1 字节（busy 位在 bit7），收完发 NO_ACK+STOP。被 `aht21_read_data` 轮询 busy 用。 |
| `check_crc8(...)` | 33 | CRC-8 校验 | 多项式 `0x31`、初值 `0xFF`。**定义了但全程没被调用**——数据完整性当前未校验（见 §7）。 |
| `aht21_read_id(...)` / `__read_id(...)` | 122 / 67 | 读 ID `0x18` | 当前不在主路径（init 里的调用被注释）。`__read_id` 是真正的 I2C 读 ID 实现，`aht21_read_id` 只是把全局 `g_device_id` 通过出参回传——且它签名与 `pf_read_id` 槽位不符（见 §7）。 |

### 3.4 软件 I2C 引擎 · `Core/Src/iic.c`（`IIC*` 系列）

这些是"位带"软件 I2C，靠 `delay_us` 拉时序，全部以 `iic_bus_t *bus` 为第一参数（所以能和驱动的 `pf_iic_*` 签名对齐）：

- `IICInit(bus)`：把 SDA/SCL 配成开漏输出（注释里写"推挽"是笔误，实际 `GPIO_OType_OD`）。
- `IICStart / IICStop`：起始/停止条件（SCL 高时 SDA 跳变）。
- `IICWaitAck(bus)`：**返回 `1`=收到 ACK（成功），`0`=NACK/超时**——驱动里 `if (pf_iic_wait_ack(...))` 正是按"非 0 即成功"判断，与 `IICWaitAck` 语义一致（这点早期 `i2c_WaitAck` 是反的，正是旧文档要加适配器的原因）。
- `IICSendByte(bus, c)` / `IICReceiveByte(bus, *out)`：收/发 8 位；接收端经出参带回，与驱动 `pf_iic_receive_byte` 契约吻合。
- `IICSendAck / IICSendNotAck`：主设备给从机的应答/非应答。

### 3.5 OSAL 层 · `System/OSAL/osal_freertos.c`

- `osal_freertos_get_tick_ms`：包 `xTaskGetTickCount` → `get_tick_ms`（Handler 的 lifetime 用）。
- `osal_freertos_queue_create / put / get`：包 `xQueueCreate / xQueueSend / xQueueReceive`，timeout 经 `pdMS_TO_TICKS` 转换（`MY_MAX_DELAY = 0xffffffff` → `portMAX_DELAY` 永久阻塞）。
- `osal_freertos_critical_enter/exit`：包 `taskENTER/EXIT_CRITICAL`——**当前驱动里所有调用点都被注释掉**（见 §7）。

---

## 4. `aht21_read_data` 单步拆解（驱动心脏）

这是"实时读到温湿度"真正发生的地方，按 AHT21 协议：

1. **触发测量（写阶段）**
   - `START → 0x70(写地址) → ACK → 0xAC → ACK → 0x33 → ACK → 0x00 → ACK → STOP`
   - `0xAC 0x33 0x00` 是 AHT21 标准"触发测量"命令。
2. **等测量完成**
   - `delay_ms(80)` 粗等（AHT21 典型 ~80ms）；随后 `while (status.busy && cnt--) { delay_ms(5); 重读状态 }` 精细轮询。`cnt` 用尽返回 `AHT21_ERRORTIMEOUT`。
3. **读数据（读阶段）**
   - `START → 0x71(读地址) → ACK`，连续收 **6 字节**：
     - `byte_1th` = 状态字节（解码未用，仅 busy 判断已在前步用 `aht21_read_status` 完成）
     - `byte_2th..byte_6th` = 5 个数据字节（湿度 20-bit 占 byte2/3/4 高4位；温度 20-bit 占 byte4 低4位 / byte5 / byte6）
   - 前 5 字节收完发 `ACK`，第 6 字节发 `NO_ACK` 再 `STOP`。
4. **20-bit 解码**（见附录推导）
   - 湿度：`(byte2<<16 | byte3<<8 | byte4) >> 4`，再 `*1000>>20` 后 `/10` → 即 `(S/2^20)×100` %RH。
   - 温度：`(byte4&0x0F)<<16 | byte5<<8 | byte6` 取低 20 位，再 `*2000>>20 -500` 后 `/10` → 即 `(S/2^20)×200 - 50` ℃。
   - 解码公式与 AHT2x 数据手册一致，**算法正确**；只是用移位 trick 写成，可读性偏弱（见 §7 改进建议）。

> 注意：代码只收 6 字节（状态 + 5 数据），AHT21 某些型号在 6 数据字节后还会跟 1 个 CRC 字节——本驱动不读也不校验它，所以 `check_crc8` 完全空闲。

---

## 5. 数据流与 lifetime 缓存机制

```
生产者 send_temp_humi_event
   │ 每 1s 构造 event{type=BOTH, lifetime=1000}
   ▼
queue_put(event)  ──────────────►  [FreeRTOS 队列, 深 10]
                                        │
消费者 temp_humi_handler_thread         │
   │ queue_get (阻塞) ◄─────────────────┘
   ▼
get_temp_humi:
   now = get_tick_ms()
   if (now - last_tick) >= lifetime:      ← lifetime 闸门
        pf_read_data(&temp, &humi)        ← 真正打 I2C
        if OK: last_tick = now            ← 成功才刷新缓存时间戳
   ▼
event.pf_callback(&temp, &humi)           ← 当前 NULL → 仅 elog
```

- **为什么要 lifetime**：传感器测量慢（~80ms+），不宜被高频触发；用"距上次成功读数是否超过 lifetime"做节流，生产者可以高频投事件，消费者自行决定要不要真读。
- **`>=` 而非 `>`**：当发送周期(1000ms) == lifetime(1000ms) 时，`1000 > 1000` 恒假会使"每两次事件才读一次"，减半采样率；`>=` 修正为每次都读（代码注释已说明）。
- **失败不刷时间戳**：读失败时不更新 `last_tick`，下次事件立刻重试，不会把上一次旧值当"新数据"上报——这是个**正确且值得保留**的细节。

---

## 6. 设计方法分析（重点）

### 6.1 桥接模式（Bridge Pattern）—— 这是本驱动的主骨

- **"抽象"维度** = Handler 的业务逻辑（队列、时基、lifetime、回调）。
- **"实现"维度** = 底层驱动（AHT21）与传输引擎（软件 I2C）。
- 两者通过 `instantiate` 时挂载的 **函数指针 ops 表（`pf_*`）** 接起来：Handler 只调 `pf_read_data`，完全不关心里面是 AHT21 还是 SHT30、是软件 I2C 还是硬件 I2C。
- 两个变化轴可独立替换：换传感器 → 只换 driver 实现；换传输 → 只换 `iic_driver_interface_t` 填充。这就是桥接模式"抽象与实现解耦"的本意。

### 6.2 函数指针 ops 表 = "面向对象 C"

`bsp_aht21_driver_t` 里一坨 `pf_*` 函数指针 + `iic_driver_interface_t` 里一坨 `pf_iic_*`，是嵌入式里最常见的"用结构体模拟类/虚函数表"手法，等价于 Linux 内核的 `file_operations` / `device_driver` 注册回调。**优点**：driver 注册即插即用、无 `if (sensor==AHT21)` 之类的分支；**代价**：函数指针调用无法内联、调试时调用栈多一层、签名强转容易掩盖 bug（见 §7）。

### 6.3 生产者-消费者解耦（队列）

`send_task` 与 `temp_humi_handler_thread` 通过队列解耦：
- 生产者只投"请求"，不碰 I2C；消费者同步读取并回调。
- 读取是阻塞型（~80ms+），放在独立任务里做，**不阻塞生产者周期**，也不阻塞其它业务。
- 队列深度 10，允许多生产者/突发。

### 6.4 OSAL 抽象层

把 FreeRTOS 的 `xQueue*` / `xTaskGetTickCount` / `taskENTER_CRITICAL` 包成 `osal_queue_t` / `osal_time_base_t` / `osal_critical_t`。好处是 driver/handler 不直接 `#include "FreeRTOS.h"`，理论上可换 RT-Thread/裸机。**但当前有泄漏**：driver 测量等待直接 `#include "timer.h"` 用 `delay_ms`（TIM2 忙等），并没有走传入的 `osal_delay_t`（见 §7）。

### 6.5 当前最大的设计债务：Handler 仍直接持有具体驱动

按项目既定目标（温湿度传感器可能更换，理想是"只改 driver 不动 handler"），`bsp_temp_humi_handler_t` 里仍是 `bsp_aht21_driver_t *p_aht21_instance`——Handler 直接依赖 AHT21 这个具体类型，而非一个设备无关的 `temp_humi_ops_t`（含 `pf_init/pf_read_data/pf_read_id/pf_sleep/pf_wakeup`，不支持的操作填 NULL）。**要做到"换芯不改 handler"，应把 Handler 改为持有 `const temp_humi_ops_t *ops + void *dev`（不透明上下文）**，driver 在 `instantiate` 时把 `ops` 和 `dev` 接线进去。这是下一步最有价值的重构。

---

## 7. 已知问题 / 改进建议（按严重度排序）

| # | 位置 | 问题 | 严重度 | 建议 |
|---|------|------|--------|------|
| 1 | `driver.c:33` `check_crc8` | CRC-8 函数定义却**从未调用**，测量数据无完整性校验 | 中 | 读完 6 字节后若传感器有 CRC 字节则读第 7 字节并校验；或确认无 CRC 则删除死函数 |
| 2 | `driver.c:585-596` | `pf_read_temperature` / `pf_read_humidity` **未挂载**（NULL）。`get_temp_humi` 的 `TEMP`/`HUMI` 单模式会调 NULL 指针崩溃 | 高(潜在) | 在 `aht21_instantiate` 里补挂载；或删除这两个未实现分支，仅保留 `pf_read_data` |
| 3 | `driver.c:91/122/588` | `pf_read_id` 槽位是 `(void * const)`，实现 `aht21_read_id(uint8_t*)` 用出参回传 ID，强转掩盖出参 | 中 | 改为 `(*pf_read_id)(void * const, uint8_t * const p_id_out)` 并同步实现 |
| 4 | `driver.c:16-17` | `g_inited`/`g_device_id` 是**全局单例状态**，非 per-instance | 中 | 移到 `bsp_aht21_driver_t` 实例内（或 `aht21_instantiate` 时分配私有数据），支持多实例并存 |
| 5 | `driver.c:167` | `(aht21_read_status(...) && 0x08) != 0x08` —— `&` 优先级高于 `&&`，本意是 `& 0x08` 判校准位，语义错（当前效果恒真→每次都发复位，功能无害） | 低(功能) | 改为 `(aht21_read_status(p) & 0x08) != 0x08` |
| 6 | `driver.c:150/381/386` | 测量等待用 `delay_ms`（timer.c TIM2 **忙等**），非 `osal_delay_t`；OSAL delay 接口虽传入却未被 driver 使用 | 中 | driver 改走 `p_delay_instance->delay_ms`；或明确"driver 仅依赖 timer 层"，从接口里删掉 os_delay 以消歧义 |
| 7 | `driver.c` 多处 | 临界区 `pf_critical_interface->enter/exit` 全部被注释（TEMP-EXP），软件 I2C 位时序可能被任务调度打断 | 中 | 用 `osal_critical_freertos`（已是 `taskENTER_CRITICAL`）仅包裹最短 I2C 事务；确认中断延迟可接受 |
| 8 | `driver.c:312-314` | `aht21_read_data` 开头 `IS_INITED` 检查被注释，未初始化也能读，不稳定 | 低 | 恢复 init 检查（或保留注释并补注释说明为何跳过） |
| 9 | `handler.c` event 结构 | `temp_humi_event_t.temperature/humidity` 指针字段**从未填充/使用**（死字段） | 低 | 删除指针字段或真正用起来，避免误用 |
| 10 | `FreeRTOS_tasks.c:330/351` | `pf_callback = NULL`，读到的温湿度只 elog 打印，无消费者 | 中(功能) | 在 `temp_humi_input_arg.pf_callback` 挂回调，把数据送去 LCD/控制逻辑 |
| 11 | 多枚举 `osal_status_t`/`aht21_status_t`/`temp_humi_status_t` | 各定义一套 status，`RESERVED` 哨兵混用 `0xFF` 与 `0x7FFFFFFF` | 低 | 收敛到统一 `osal_status_t`（0=OK） |

---

## 8. 附录：AHT21 数据格式与解码推导

AHT2x 触发 `0xAC` 后返回 **1 状态字节 + 5 数据字节**（本驱动读取量）：

```
byte_1th : 状态 (bit7=busy, bit6=校准使能)      ← 解码未用
byte_2th : RH[19:12]
byte_3th : RH[11:4]
byte_4th : RH[3:0]<<4 | T[19:16]
byte_5th : T[15:8]
byte_6th : T[7:0]
```

**湿度（20-bit）**
```
S_rh = (byte_2th<<16 | byte_3th<<8 | byte_4th) >> 4
     = byte_2th<<12 | byte_3th<<8 | (byte_4th>>4)        // 正好 20 bit
RH%  = S_rh / 2^20 × 100
代码: (S_rh * 1000 >> 20) / 10  ==  S_rh × 100 / 1048576   ✓
```

**温度（20-bit）**
```
S_t  = (byte_4th & 0x0F)<<16 | byte_5th<<8 | byte_6th        // 取低 20 bit
T℃   = S_t / 2^20 × 200 − 50
代码: ((S_t * 2000 >> 20) - 500) / 10  ==  S_t × 200/1048576 − 50   ✓
```

推导结论：**当前解码公式与手册一致，数值正确**；只是移位写法较绕，建议抽成 `aht21_decode_rh()/aht21_decode_temp()` 两个具名函数，可读性会好很多。

---

## 9. 一句话总结

这套 AHT21 桥接驱动**桥接模式用对了**（driver 只认抽象 I2C 契约、Handler 负责编排、OSAL 屏蔽 RTOS），**线程模型也对了**（生产者-消费者 + 队列 + lifetime 节流）；但目前仍是"能读到、没用好"的状态——CRC 没校验、`pf_read_temperature/humidity` 没挂载、临界区被注释、OSAL 延时被旁路、回调为空，且 Handler 还直接耦合具体 AHT21 类型。下一步最有性价比的工作是：**① 挂好回调把数据真正用起来；② 把 Handler 收敛到设备无关的 `temp_humi_ops_t` 实现"换芯不改 Handler"；③ 清掉上面 #1/#2/#6/#7 几个实打实的坑。**
