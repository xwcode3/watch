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

## 文档产出
- `docs/架构与调用逻辑说明.md`、`docs/嵌入式面试准备_3年经验.md`
