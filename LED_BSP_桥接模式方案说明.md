# LED BSP 驱动 · 桥接模式（Bridge）方案说明

> 本文仅作分析与方案设计，**未修改任何现有代码**（`bsp_led.c` / `bsp_led.h` 保持原样）。
> 目标：① 看清楚当前 LED 驱动及其“桥接”部分的问题；② 给出一套成熟可用的桥接模式落地方案。

---

## 1. 当前 LED 驱动体检

文件：`BSP/LED/bsp_led.c`、`BSP/LED/bsp_led.h`

### 1.1 它现在的样子

当前是一个**单体文件**：抽象（亮/灭/翻转/闪）和硬件实现（直接操作 `GPIOC Pin13` 寄存器）全部混在一起。

```c
static void led_on(void)  { GPIO_ResetBits(GPIOC, GPIO_Pin_13); } // 直接写寄存器
static void led_off(void) { GPIO_SetBits(GPIOC, GPIO_Pin_13); }
static void led_toggle(void){ GPIO_ToggleBits(GPIOC, GPIO_Pin_13); }

led_status_t led_control(led_operation_t operation) {
    switch (operation) {
        case LED_ON:    led_on();    break;
        case LED_OFF:   led_off();   break;
        case LED_TOGGLE:led_toggle();break;
        case LED_BLINK_3_TIMES:
            for (int i = 0; i < 6; i++) { led_toggle(); vTaskDelay(300); } // 阻塞！
            break;
        ...
    }
}
```

### 1.2 关键问题（为什么它“还不完善”）

1. **没有桥接分层**：`led_control()` 直接调用 `GPIO_ResetBits`，引脚（`GPIOC, GPIO_Pin_13`）硬编码在每个函数里。换一颗灯、换一个引脚、或加第二颗灯，都得改这套函数本体。
2. **blink 实现残缺、且阻塞**：
   - `LED_BLINK_3_TIMES` 用 `vTaskDelay(300)` **阻塞调用者任务**——在 RTOS 里这是严重反模式（占用整个任务时间片）。
   - `LED_BLINK_1_TIMES` / `LED_BLINK_10_TIMES` 只设置了全局变量 `g_blink_times` / `g_blink_order`，但**没有任何定时器/ISR 去消费这两个变量**，`led_off_on_irq()` 是死代码，闪烁状态机从未接线。
3. **`led_task` 是空转**：它创建了队列 `led_queue`，但**没有任何生产者往里发数据**，任务里只是 `printf("led_task run!")` + `vTaskDelay(100)` 死循环。队列、任务、blink 状态机三者之间没有任何连接。
4. **构建残留暴露了“未完成的桥接”**：`build/.../BSP/LED/` 下仍有 `bsp_led_driver.c.obj` 与 `bsp_led_handler.c.su`，但 `BSP/LED/` 目录里**只有 `bsp_led.c/.h`，根本不存在 `bsp_led_driver.c/.h` 和 `bsp_led_handler.c/.h`**。说明项目曾打算把 LED 拆成 `driver`（实现层）+ `handler`（抽象层）两份，但源码只写了一半、且当前构建实际只编了单体 `bsp_led.c`。
5. **小瑕疵**：`printf(".../r/n")` 应为 `\r\n`；`LED_TOGGLE` 后 `get_led_status()` 读的是输出寄存器，逻辑可行但耦合于实现细节。

---

## 2. 什么是桥接模式（Bridge Pattern）

桥接模式的核心一句：**把“抽象”（想让 LED 做什么）与“实现”（具体怎么驱动引脚）脱钩，让两者可以独立变化。**

- **Abstraction（抽象层）**：对外提供语义化 API——`led_on / led_off / led_toggle / led_blink_n`。它**不知道**底层是 GPIO 还是 PWM。
- **Implementor（实现者接口）**：一组函数指针（`init / set_level / toggle / read_level`），是连接两层的“桥”。
- **ConcreteImplementor（具体实现）**：`stm32_gpio_led_driver`（直接翻转 IO）、`pwm_led_driver`（调光）、`mock_led_driver`（PC 上做单元测试的桩）。

好处：
- 加一颗新灯 / 换引脚 → 只加一个 ConcreteImplementor，抽象层一行不改。
- 做 PC 单元测试 → 把 `mock_led_driver` 插进去，无需真实硬件。
- 多灯管理 → 抽象层维护“逻辑灯 ID → 具体驱动”的映射表即可。

---

## 3. 本项目已有的桥接实践（可直接对齐）

这个项目其实已经用过桥接思路，LED 应当对齐同一套约定：

- **W25Q64**：`bsp_w25qxx.c` 是实现层（`W25Qx_Init / W25Qx_Write / W25Qx_Read` 直接操作 SPI 与寄存器）；`bsp_w25qxx_handler.c` 是抽象层（扇区缓冲、读写状态机），只调用 `W25Qx_*` 接口，不关心 SPI 细节。
- **SPEAKER**：`bsp_speaker_driver.h` 用**函数指针接口**做桥——
  ```c
  typedef struct {
      void (*pf_init)(void);
      void (*pf_io_write)(uint8_t level);
  } spk_gpio_interface_t;
  typedef struct system_interface_t {
      void (*pf_delay_us)(uint32_t us);
      void (*pf_delay_ms)(uint32_t ms);
  } spk_system_interface_t;
  ```
  驱动本体（`bsp_speaker_driver.c`）完全通过 `p_spk_gpio_interface->pf_io_write(...)` 操作硬件，上层不感知引脚。

LED 缺的就是这层“`led_driver_ops` 接口 + 具体驱动”的桥。

---

## 4. 成熟业界方案：以 Zephyr 的 GPIO 驱动为标杆

在嵌入式界，把“桥接模式”用得最彻底、最广的是 **Zephyr RTOS 的 GPIO 子系统**（也是 Linux 设备模型的简化版）。它的结构与本方案一一对应：

```c
/* Zephyr: 实现者接口（桥） */
struct gpio_driver_api {
    int (*pin_configure)(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags);
    int (*port_get_raw)(const struct device *dev, gpio_port_value_t *values);
    int (*port_set_masked_raw)(const struct device *dev, gpio_port_pins_t mask,
                               gpio_port_value_t value);
    int (*pin_toggle)(const struct device *dev, gpio_pin_t pin);
    ...
};

/* 抽象层：应用只调用统一 API，经 device->api 派发到具体驱动 */
gpio_pin_set_dt(&led_spec, 1);   // 内部：dev->api->port_set_masked_raw(...)
```

- `struct device` = 抽象层句柄（对应我们的 `LED_Handler`）。
- `struct gpio_driver_api` = 实现者接口（对应我们的 `led_driver_ops`）。
- `gpio_stm32`、`gpio_nrfx` 等 = 具体实现（对应 `stm32_gpio_led_driver`）。

**关键设计点（值得直接借鉴）：**
1. 应用永远只调用 `gpio_pin_set()`，永远不直接碰寄存器。
2. 具体驱动通过 `device->api` 函数指针表挂载，**编译期/运行期可切换**。
3. 同一份抽象层代码，能在 STM32、NRF、模拟器等任意硬件上跑。

另一个参考是 **RT-Thread 的 `rt_device` + `rt_device_ops`** 框架，以及 **LVGL 的 `lv_indev_drv` / 显示驱动接口**——本质都是“上层操作统一接口，下层用函数指针表桥接到具体硬件”。

---

## 5. 针对本项目的 LED 桥接重构方案（设计稿，未落地）

> 完全对齐 W25Q64 / SPEAKER 的 `driver + handler` 约定，并补上缺失的“非阻塞 blink 状态机”。

### 5.1 实现者接口（桥）——`bsp_led_driver.h`

```c
#ifndef BSP_LED_DRIVER_H
#define BSP_LED_DRIVER_H
#include <stdint.h>

typedef enum { LED_DRV_ACTIVE_LOW = 0, LED_DRV_ACTIVE_HIGH } led_drv_polarity_t;

/* 实现者接口：抽象层只认这组函数指针 */
typedef struct led_driver_ops {
    int  (*init)(void);
    void (*set_level)(uint8_t on);   /* on: 1=亮 0=灭（已按极性换算）*/
    void (*toggle)(void);
    uint8_t (*read_level)(void);
} led_driver_ops_t;

/* 具体实现：基于 STM32 标准库的 GPIO 驱动 */
extern const led_driver_ops_t stm32_gpio_led_driver;
int  stm32_gpio_led_driver_init(void);          /* GPIO 时钟/引脚初始化 */
void stm32_gpio_led_driver_set(uint8_t on);
void stm32_gpio_led_driver_toggle(void);
uint8_t stm32_gpio_led_driver_read(void);

#endif
```

### 5.2 具体驱动实现 ——`bsp_led_driver.c`（仅封装硬件，不含业务）

```c
#include "bsp_led_driver.h"
#include "stm32f4xx.h"

#define LED_GPIO_PORT  GPIOC
#define LED_GPIO_PIN   GPIO_Pin_13
#define LED_POLARITY   LED_DRV_ACTIVE_LOW

static uint8_t to_pin_level(uint8_t on) {
    return (on ^ (LED_POLARITY == LED_DRV_ACTIVE_LOW)) ? Bit_SET : Bit_RESET;
}
int stm32_gpio_led_driver_init(void) {
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef g = {0};
    g.GPIO_Pin = LED_GPIO_PIN; g.GPIO_Mode = GPIO_Mode_OUT;
    g.GPIO_OType = GPIO_OType_PP; g.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(LED_GPIO_PORT, &g);
    return 0;
}
void stm32_gpio_led_driver_set(uint8_t on){
    GPIO_WriteBit(LED_GPIO_PORT, LED_GPIO_PIN, to_pin_level(on));
}
void stm32_gpio_led_driver_toggle(void){
    GPIO_ToggleBits(LED_GPIO_PORT, LED_GPIO_PIN);
}
uint8_t stm32_gpio_led_driver_read(void){
    return (GPIO_ReadOutputDataBit(LED_GPIO_PORT, LED_GPIO_PIN) == to_pin_level(1));
}
const led_driver_ops_t stm32_gpio_led_driver = {
    .init       = stm32_gpio_led_driver_init,
    .set_level  = stm32_gpio_led_driver_set,
    .toggle     = stm32_gpio_led_driver_toggle,
    .read_level = stm32_gpio_led_driver_read,
};
```

### 5.3 抽象层（handler）——`bsp_led_handler.c`（业务 + 非阻塞 blink 状态机）

```c
#include "bsp_led_handler.h"
#include "bsp_led_driver.h"
#include "FreeRTOS.h"
#include "task.h"

static const led_driver_ops_t *s_drv = &stm32_gpio_led_driver; /* 默认挂 GPIO 驱动 */
static uint32_t s_blink_cnt = 0;   /* 剩余闪烁半周期数 */
static uint32_t s_blink_on  = 0;   /* 当前应处的亮灭 */
static uint16_t s_blink_ticks = 0; /* 每个半周期 tick 数 */

void led_handler_init(void) { s_drv->init(); }
void led_handler_on(void)  { s_blink_cnt = 0; s_drv->set_level(1); }
void led_handler_off(void) { s_blink_cnt = 0; s_drv->set_level(0); }
void led_handler_toggle(void){ s_drv->toggle(); }

/* 非阻塞闪烁：每 tick 由定时器/任务调用一次，不阻塞任何任务 */
void led_handler_blink(uint8_t times, uint16_t period_ms) {
    s_blink_cnt   = (uint32_t)times * 2;
    s_blink_on    = 1;
    s_blink_ticks = period_ms / portTICK_PERIOD_MS / 2;
    s_drv->set_level(1);
}
/* 由 1ms/节拍定时器或专用任务周期性调用 */
void led_handler_tick(void) {
    static uint16_t t = 0;
    if (s_blink_cnt == 0) return;
    if (++t >= s_blink_ticks) {
        t = 0;
        s_blink_on ^= 1;
        s_drv->set_level(s_blink_on);
        if (--s_blink_cnt == 0) s_drv->set_level(0); /* 结束时灭 */
    }
}
```

要点：`led_handler_blink()` 立刻返回，亮灭节奏交给 `led_handler_tick()`（挂在系统节拍或软定时器上），**彻底消除 `vTaskDelay` 阻塞**，也补上了原先“全局变量无人消费”的空洞。

### 5.4 多灯扩展（桥接模式的价值直观体现）

```c
typedef struct { const led_driver_ops_t *drv; uint32_t blink_cnt; ... } led_dev_t;
static led_dev_t g_leds[LED_ID_MAX];   /* 逻辑灯 ID -> 具体驱动 */
led_handler_on(LED_ID_STATUS);         /* 调用方只认逻辑 ID */
```
加一颗新灯（如 PWM 呼吸灯），只需新增一个 `pwm_led_driver` 并挂到 `g_leds[LED_ID_BREATH].drv`，抽象层与调用方零改动。

---

## 6. 迁移步骤与收益

1. **新增** `bsp_led_driver.c/.h`（硬件封装，见 5.1/5.2），把 `GPIO_*` 调用从 `bsp_led.c` 迁过去。
2. **新增** `bsp_led_handler.c/.h`（语义 API + 非阻塞 blink，见 5.3）。
3. 让 `led_task` 真正驱动 `led_handler_tick()`，或接到系统节拍定时器；删除空转与死代码。
4. 调用方改为 `led_handler_on/off/toggle/blink`，不再直接碰 `bsp_led.c` 的 `GPIO`。

收益：引脚/极性变更只动 `bsp_led_driver.c`；blink 不再阻塞任务；可插 `mock_led_driver` 做 PC 单测；多灯只需扩展映射表。完全对齐本项目 W25Q64 / SPEAKER 既有的 `driver+handler` 桥接约定。

> 注：以上为**设计方案与示例**，按你的要求**未改动任何现有源文件**。如确认要落地，我可以在新分支上按此结构实现并补齐 blink 状态机的接线。
