# MPU6050 桥接模式驱动包（STM32F411CEU6 + FreeRTOS + SPL 硬件 I2C）

与 AHT21 驱动同构的桥接式驱动。四层：集成层 → Handler 层 → Driver 层 → 硬件 I2C 引擎层。

## 快速开始（3 步）

1. 把 `Core/Src/iic_interface.h`、`Core/Src/hw_iic.c/.h` 放入工程 `Core/Src/`；`BSP/MPU6050/` 整目录放入 BSP；`System/OSAL/osal_interface.h` 放入 OSAL 目录；`integration/mpu6050_app_example.c` 放入 `Core/Src/`。
2. 3 个新 `.c` 加入编译，include 路径加入 `BSP/MPU6050`。
3. `main()` 中调度器启动前调用 `mpu6050_app_task_create();`，接线 GY-521：VCC→3V3、SCL→PB6、SDA→PB7、AD0→GND。

详细步骤、寄存器序列、错误处理与 FAQ：`docs/MPU6050_设计文档.md`（网页版 `MPU6050_设计文档.html`）。

## 改哪里的速查

| 想改什么 | 位置 |
|---|---|
| 换 I2C2(PB10/11) / I2C3(PA8/PC9) | `integration/mpu6050_app_example.c` 的 `iic_bus_mpu6050` 结构体（唯一改动点） |
| 量程 / DLPF / 采样率 | `imu_input_arg.cfg`（mpu6050_config_t），或保持 NULL 用默认 ±2g/±250°/s/44Hz/100Hz |
| 采样周期 | `send_imu_event` 里的 `vTaskDelay(100)` 与事件 `lifetime_ms` |
| 数据消费者 | `imu_data_consumer()`（当前 printf，可接 LCD/控制逻辑） |
| 日志开关 | 编译宏 `MPU6050_USE_ELOG`（接 EasyLogger） |

## 验证

- 语法级交叉编译（arm-none-eabi-gcc 14.3.1，-Wall -Wextra）：4 个源文件 0 error / 0 warning；
- 实机验证按 `docs/MPU6050_测试台账.md` 的 V1–V8 逐项回填。
