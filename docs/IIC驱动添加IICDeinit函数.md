# IIC 驱动添加 `IICDeinit` 函数

> 适用工程：`watch_projects/app` → `Core/Inc/iic.h` / `Core/Src/iic.c`
> 目的：为 `IIC` 开头的软件 IIC 增加与 `IICInit` 对称的反初始化函数，并在 AHT21 适配器里真正调用它。
> 原则：**本文档只给建议与改后代码，不自动改你的源码**，请照着手动落地。

---

## 0. 背景与定位

- `iic.c` 的 `IIC*` 软件 IIC 现在只有 `IICInit(iic_bus_t *bus)`（`iic.c:565`），负责把 SDA 配成开漏输出、SCL 配成推挽输出并拉高。
- 缺一个对称的 `IICDeinit`：当 handler 卸载 / 进低功耗 / 释放总线时，应当停止 MCU 对 SDA/SCL 的驱动，把引脚交还（回到输入态）。
- 上一版挂载文档 `docs/AHT21_IIC接口挂载修改建议.md` 里的 `adapter_iic_deinit` 只是占位（用 `IICStop` 顶替）。有了真正的 `IICDeinit` 后应改成调用它。
- 接口 `iic_driver_interface_t` 中的 `pf_iic_deinit` 成员**已经存在**（挂载文档 3.1 第 98 行），无需再改接口定义；适配器实例也已接好 `.pf_iic_deinit = adapter_iic_deinit`（挂载文档 3.3 第 211 行）。

---

## 1. 设计要点（先看，避免踩坑）

1. **只改 GPIO 模式，不要关时钟。**
   `IICInit` 本身**没有**使能 GPIOB 时钟（适配器 `adapter_iic_init` 里补了 `RCC_AHB1PeriphClockCmd(GPIOB, ENABLE)`）。
   同理 `IICDeinit` **不要**去 `RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, DISABLE)`——因为同一个 GPIOB 还可能被 EEPROM（走 `i2c_*` 那组）等其他外设使用，关掉时钟会让别人也失效。
2. **释放总线 = 把 SDA/SCL 配成输入、无上下拉（浮空）。**
   软件 IIC 是位带驱动，MCU 主动推挽/开漏。deinit 时停驱动即可，外部上拉电阻会把两条线拉回 I2C 空闲态（高电平）。
   - 若你的板子**没有**外部上拉（那本来就是硬件缺陷，I2C 也跑不通），可把 `GPIO_PuPd_NOPULL` 换成 `GPIO_PuPd_UP`，用内部弱上拉兜底——但优先确认硬件有外部上拉。
3. **加 `NULL` 防御。** `IICInit` 没加，但 deinit 更该加，避免传空指针解引用崩。
4. **与 `IICInit` 风格对齐**：用同一个 `GPIO_InitTypeDef`，引脚逐一配置（IICInit 就是分两条 `GPIO_Init` 配 SDA、SCL 的，这里沿用）。

---

## 2. 改后代码（请手动应用）

### 2.1 `Core/Inc/iic.h` —— 加声明

在 `void IICInit(iic_bus_t *bus);`（第 97 行）**后面**新增一行：

```c
void IICInit(iic_bus_t *bus);
void IICDeinit(iic_bus_t *bus);   /* ← 新增：与 IICInit 对称的反初始化 */
```

### 2.2 `Core/Src/iic.c` —— 加实现

在 `IICInit` 函数体**结尾**（原文件第 591 行 `GPIO_SetBits(bus->IIC_SCL_PORT, bus->IIC_SCL_PIN);` 之后、文件结尾之前）新增：

```c
/**
  * @brief IIC反初始化（释放GPIO，停止驱动总线）
  * @param iic_bus_t *bus: IIC总线结构体指针
  * @retval None
  */
void IICDeinit(iic_bus_t *bus)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    if (bus == NULL)
    {
        return;
    }

    /* 将 SDA / SCL 配置为输入、无上下拉，释放 MCU 对总线的驱动。
       外部上拉电阻会将其拉回 I2C 空闲态（高电平）。
       注意：不关闭 GPIOB 时钟——其他外设（如 EEPROM 用的 i2c_*）可能仍在使用 GPIOB。 */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    /* SDA 线 */
    GPIO_InitStructure.GPIO_Pin = bus->IIC_SDA_PIN;
    GPIO_Init(bus->IIC_SDA_PORT, &GPIO_InitStructure);

    /* SCL 线 */
    GPIO_InitStructure.GPIO_Pin = bus->IIC_SCL_PIN;
    GPIO_Init(bus->IIC_SCL_PORT, &GPIO_InitStructure);
}
```

> 风格说明：与 `IICInit` 一致分两条 `GPIO_Init` 分别配 SDA / SCL，避免对 `GPIO_Pin` 取 `|` 合并
> （因为 SDA / SCL 可能不在同一个 `GPIO_Pin` 位域上，沿用 `IICInit` 的写法最安全）。

### 2.3 `BSP/AHT21/bsp_aht21_iic_adapter.c` —— 让适配器真正调用它

在挂载文档 `docs/AHT21_IIC接口挂载修改建议.md` 的 3.3 节里，`adapter_iic_deinit` 目前是占位：

```c
static void adapter_iic_deinit(void * p) {
    (void)p;
    IICStop(&g_aht21_iic_bus);   /* 软件 IIC 无硬件注销动作，保持总线空闲即可 */
}
```

改为调用真正的 `IICDeinit`（记得 `#include "iic.h"` 已在适配器头文件里包含）：

```c
static void adapter_iic_deinit(void * p) {
    (void)p;
    IICDeinit(&g_aht21_iic_bus);   /* 真正释放总线：SDA/SCL 回到输入态 */
}
```

接口成员 `pf_iic_deinit` 与实例接线 `.pf_iic_deinit = adapter_iic_deinit` 都已存在，**无需改动**。

---

## 3. 调用建议（可选，按需接入）

`IICDeinit` 加好后，可在以下时机调用，把 `&g_aht21_iic_bus` 交给它：

- handler 去初始化 / 删除温湿度任务时；
- 设备进入低功耗（STOP/SLEEP）前，先 `IICDeinit` 释放总线，避免 MCU 引脚在睡眠中误驱动；
- 需要把 I2C 总线让给别的设备（多路复用）时。

> 注意：AHT21 一旦初始化并触发过测量，传感器内部状态机不依赖 MCU 引脚保持——`IICDeinit` 只是让 MCU 侧停止驱动，
> 重新 `IICInit` 即可再通信。若之后还要继续读温湿度，务必先 `IICInit` 再发 START。

---

## 4. 验证

1. **编译**：`IICDeinit` 声明与实现一致，无新增报错；适配器里 `IICDeinit` 能被链接到（它在 `iic.c` 已实现，`iic.h` 已声明，`bsp_aht21_iic_adapter.c` 已 `#include "iic.h"`）。
2. **静态检查**：确认没有去 `RCC_AHB1PeriphClockCmd(GPIOB, DISABLE)`，以免误伤 GPIOB 上的其他外设。
3. **运行时可选验证**：在 `adapter_iic_init` 之后人为调一次 `adapter_iic_deinit`，用万用表/逻辑分析仪确认 PB8/PB9 不再被 MCU 强力驱动（变为外部上拉决定的高电平或浮空）；随后再 `IICInit` 应能正常读 ID（`0x18`）。
