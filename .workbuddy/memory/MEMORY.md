# 项目长期记忆：手表 App 工程

## 工程身份
STM32F411CEU6 手表固件（StdPeriph 库 + FreeRTOS），与独立 Bootloader 工程配套。
OTA 链路：App(`ota_task`) 用 Ymodem 把密文固件收进 W25Q64 外部 Flash → 状态/长度存 AT24C02 →
复位交还 Bootloader 解密(AES-256-CBC)写内部 Flash 并跳转。

## 必记约定 / 坑
- **App 运行地址**：链接脚本与 `main.c` 的 VTOR 用 `0x08010000`；但 `boot_manager.h` 的
  `ApplicationAddress_boot=0x08008000`、`BackupApplicationAddress_boot=0x08020000`。
  两份地址表未对齐，改动 Flash 布局时务必同步两边（含 Bootloader 工程）。
- 本工程**未启用**的"框架/死代码"很多（LED 事件框架、KEY 中断状态机、ADC/DMA、USART 解析任务），
  实际只跑 `led_blink_task` + `ota_task`。读代码时按真实调用链走，别被死代码带偏。
- `flash.c::flash_erase` 有数组越界(`i<=12` 访问 12 项数组)；`configCHECK_FOR_STACK_OVERFLOW=0`。
- **头文件 include guard 约定**：全部 BSP 层 `.h` 统一用 `#pragma once` + 传统 guard 组合，
  guard 宏命名为 `BSP_<模块>_H`（如 `BSP_LED_DRIVER_H`）。**禁止**双下划线(`__`) / 单下划线+大写(`_X`) 开头
  （C 标准保留给实现），也禁止 `WATCH_` 前缀。新增 BSP 头文件请沿用此风格。

## 文档产出
- `docs/架构与调用逻辑说明.md`、`docs/嵌入式面试准备_3年经验.md`

## BSP 桥接驱动命名约定（建议，待与 owner 确认后落地）
- **已统一**：头文件 guard = `BSP_<模块>_H` + `#pragma once`；挂载函数指针统一 `pf_` 前缀；设备实例类型 `bsp_<模块>_t`。
- **需收敛**：① ops 结构体字段本模块方法用 `pf_<verb>`（如 `pf_init`/`pf_read_data`），仅内嵌底层引擎才 `pf_<engine>_<verb>`（如 `pf_iic_*`）；
  LED 的 `pf_led_countroler` 应改名（且修正 "countroler" 拼写）。
  ② **handler 层确认为通用 `temp_humi` 子系统**（owner 决策：温湿度传感器可能更换，目标=只改 driver 不动 handler）。
     做法：抽设备无关抽象接口 `temp_humi_ops_t`（`pf_init/pf_read_data/pf_read_id/pf_sleep/pf_wakeup`，不支持的操作用 NULL 表示），
     handler 只依赖它、不持有具体 `bsp_aht21_driver_t*`，用 `const temp_humi_ops_t *ops + void *dev`(不透明上下文) 接线；
     driver 层仍用 `<device>_*`（aht21_*）。即删除 `xxx` 占位 → `bsp_temp_humi_handler_t`。
     当前 handler 仍直接持有 `bsp_aht21_driver_t*`，需做这步解耦才算真正可换芯。
  ③ 返回值统一一套（建议复用 `osal_status_t`，0=OK），别让每个模块各造 status 枚举，尤其 RESERVED 哨兵值不要混用 0xFF / 0x7FFFFFFF。
- **注意真 bug**：AHT21 `pf_read_id` typedef `(void* const)` 与实现 `aht21_read_id(uint8_t*)` 签名不符，强转掩盖，调用会传错参。
