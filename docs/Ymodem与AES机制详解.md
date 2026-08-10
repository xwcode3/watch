# Ymodem 传输协议 与 AES 加解密机制详解（手表 App OTA 工程）

> 适用工程：`watch_projects/app`（STM32F411CEU6 + StdPeriph + FreeRTOS）
> 相关源码：`Middlewares/Ymodem/`、`Middlewares/AES/`、`APP/OTA/ota.c`、`Core/Src/boot_manager.c`、`Middleware/CRC/`

---

## 0. 一句话总览（OTA 端到端链路）

```
PC 端加密固件 ──(UART + Ymodem 协议)──▶ App 接收 ──▶ 写入 W25Q64 外部 Flash
                                                  └─▶ 固件长度写入 AT24C02
        (软复位)
Bootloader 读取 W25Q64 ──▶ AES-256-CBC 解密 ──▶ 写入内部 Flash 运行区 ──▶ 跳转执行
```

- **App 侧（本项目重点）**：只负责「用 Ymodem 把密文收下来、原样存进 W25Q64」，不在 App 里解密。
- **Bootloader 侧（boot_manager.c）**：复位后用同一套 `Middlewares/AES` 算法把密文逐块解密，写到内部 Flash 并跳转。
- 加密发生在**产线 PC 工具**里（不在本工程运行），本工程提供的是配套的 `Aes_IV_key256bit_*` 编解码函数。

---

## 1. Ymodem 协议

### 1.1 它是什么

Ymodem 是一种基于「串行链路（这里是 USART1）」的**批量文件传输协议**，是 Xmodem 的增强版。它的特点是：

- 分包传输，单包 128 字节（`SOH`）或 1024 字节（`STX`，所谓 "Ymodem-1K"）。
- 用 **CRC-16 (XMODEM/CCITT)** 做整包校验，比 Xmodem 的 checksum 更可靠。
- 第一包传输「文件名 + 文件大小」作为会话头，之后才是数据。
- 接收方通过 `C`(0x43) / `ACK`(0x06) / `NAK`(0x15) 控制发送方的节奏与重传。

### 1.2 数据包帧格式

一个 Ymodem 包的头部固定 3 字节，尾部固定 2 字节 CRC，中间是数据。

**128 字节包（SOH）：**

| 偏移 | 字节数 | 字段 | 说明 |
|------|--------|------|------|
| 0 | 1 | `SOH` (0x01) | 起始标志（128 字节包）|
| 1 | 1 | 包序号 `N` | 从 0 开始，每包 +1（mod 256）|
| 2 | 1 | `~N` (序号按位取反) | 校验序号是否正确 |
| 3..130 | 128 | 数据 | 文件名包 / 文件数据 |
| 131..132 | 2 | CRC-16 高/低字节 | 对 128 字节数据计算 |

**1024 字节包（STX）：** 仅把 `SOH` 换成 `STX`(0x02)，数据区扩到 1024 字节，CRC 对 1024 字节计算。

代码中对应宏（`ymodem.h`）：

```c
#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)
#define PACKET_HEADER           (3)     // SOH/STX + seq + ~seq
#define PACKET_TRAILER          (2)     // CRC_H + CRC_L
#define PACKET_OVERHEAD         (5)     // 3 + 2
#define PACKET_SIZE             (128)
#define PACKET_1K_SIZE          (1024)
```

### 1.3 控制字符（`ymodem.h`）

| 宏 | 值 | 含义 |
|----|----|------|
| `SOH` | 0x01 | 128 字节数据包起始 |
| `STX` | 0x02 | 1024 字节数据包起始 |
| `EOT` | 0x04 | 发送方结束传输 |
| `ACK` | 0x06 | 接收方确认 OK |
| `NAK` | 0x15 | 接收方要求重传（否定确认）|
| `CA`  | 0x18 | 连续两个 `CA` 中止传输 |
| `CRC16` | 0x43 | 即字符 `'C'`，请求使用 16 位 CRC 校验 |
| `ABORT1`/`ABORT2` | 0x41/0x61 | 用户按 `A`/`a` 主动中止 |

### 1.4 一次完整传输的时序

```
Sender(PC)                         Receiver(App)
    |                                   |
    |            <--- 'C' (0x43) -------|   请求进入 CRC 模式
    |--- SOH 包0(文件名,大小) --------->|
    |            <--- ACK --------------|
    |            <--- 'C' --------------|   继续 CRC 模式
    |--- STX/SOH 数据包(N=1) ---------->|
    |            <--- ACK --------------|
    |--- 数据包(N=2) ------------------>|
    |            <--- ACK --------------|
    |   ...                              |
    |--- EOT -------------------------->|
    |            <--- ACK --------------|
    |--- EOT 再次 --------------------->|   标准的"双 EOT"收尾
    |            <--- ACK --------------|
    |--- SOH 包0(空文件名) ------------>|   空包头 = 会话结束
    |            <--- ACK --------------|
```

> 本项目 `Receive_Packet` 对 `EOT` 直接 `ACK` 后标记 `file_done`；`CA` 收到两个则中止。

### 1.5 校验：CRC-16/XMODEM

接收端在 `Receive_Packet` 中做的事：

```c
uint16_t u16_crc_check_data = crc_xmodem((data + PACKET_HEADER), packet_size);
uint16_t u16_crc_receive_data = *(data + PACKET_HEADER + packet_size) << 8;
u16_crc_receive_data += *(data + PACKET_HEADER + packet_size + 1);
if (u16_crc_check_data != u16_crc_receive_data) return -1;  // CRC 不符 → 整包丢弃
```

- 多项式：`0x1021`（`CRC_POLY_CCITT`），初值 `0x0000`（`CRC_START_XMODEM`）。
- 实现来自 `Middleware/CRC/crccitt.c` 的 `crc_xmodem()`，使用 256 项查表法加速。
- 同时校验序号补码：`data[1] != (~data[2] & 0xFF)` 也判为错误。

---

## 2. Ymodem 相关函数逐解（`ymodem.c`）

### 2.1 字节收发（最底层，依赖串口）

```c
static int32_t Receive_Byte(uint8_t *c, uint32_t timeout);  // 轮询 SerialKeyPressed，超时返回 -1
static uint32_t Send_Byte(uint8_t c);                       // 调 SerialPutChar 发送一个字节
```
`SerialKeyPressed` / `SerialPutChar` 来自 `common.h`/`usart`，是串口收发的最小接口。

### 2.2 `Receive_Packet(data, length, timeout)`

解析**单个** Ymodem 包，是收端的核心。流程：
1. 读第一个字节判断类型：`SOH`/`STX` → 确定包大小；`EOT` → 返回 0（结束）；连续两个 `CA` → 中止；`A`/`a` → 用户中止。
2. 按 `(包大小 + 5)` 把整包读完。
3. 校验序号补码与 **CRC-16**。
4. 通过 `*length` 返回包长（正常为 128/1024；`-1` 表示发送方中止；`0` 表示 EOT）。

### 2.3 `Ymodem_Receive(buf)` —— 旧版单缓冲接收

主接收循环：发送 `'C'` 等待 → 收包头解析文件名/大小 → 每收一包 `memcpy` 到 `buf` 再 `W25Q64_WriteData` 落盘 → 收完返回文件大小。内含对 `size > FLASH_SIZE-1` 的保护（超界发 `CA CA` 中止）。

### 2.4 `Ymodem_Receive_2buffer(buf1, buf2)` —— **当前 OTA 实际调用**

`ota_task` 在 `OtaDownload` 状态调用它。与单缓冲版的区别：
- 用 **双缓冲 ping-pong**（`g_au8_YmodemRec_A` / `g_au8_YmodemRec_B`）：收完一包就把 `packet_data` 指针切到另一块缓冲，便于理论上交给另一个任务消费（原 `DownloadAppData_task` 已注释，现在是同步写 W25Q64）。
- 包头解析文件名后直接 `Send_Byte(ACK); Send_Byte(CRC16);` 进入数据流。
- 数据包含 `'0' != packet_data[PACKET_HEADER]` 之外都按数据写入 W25Q64：
  ```c
  packet_data += PACKET_HEADER;
  W25Q64_WriteData(packet_data, packet_length);  // 原样写入密文
  packet_data -= PACKET_HEADER;
  ```
- 结尾调用 `W25Q64_WriteData_End()` 把不足 4096 字节的残余缓冲刷入 Flash，返回 `(int32_t)size`。

> 关键认知：**这里写入 W25Q64 的是「密文」，且不做任何解密**。解密推迟到 Bootloader 复位后。

### 2.5 发送端函数（用于上传/调试，OTA 下载路径不调用）

- `Ymodem_Transmit(buf, name, size)`：完整发送流程（包头 → 数据 → EOT → 空包头），带重传（错误满 10 次返回错误）。
- `Ymodem_PrepareIntialPacket` / `Ymodem_PreparePacket`：组装文件名包 / 数据包（填 SOH/STX、序号、`~序号`、补 0 或 `0x1A`）。
- `Ymodem_SendPacket(data, len)`：逐字节 `Send_Byte`。

### 2.6 CRC 计算函数（发送端用）

```c
uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte);   // 单字节迭代
uint16_t Cal_CRC16(const uint8_t* data, uint32_t size); // 整包 CRC（含补两个 0）
uint8_t  CalChecksum(const uint8_t* data, uint32_t size); // 8 位累加和（非 CRC 模式）
```

---

## 3. AES 加解密机制

### 3.1 AES 基础

AES（Advanced Encryption Standard）是**分组密码**，本工程实现的要点：
- **分组大小固定 16 字节（128 位）**，不足 16 字节需填充（本工程由首包 16 字节头 + 后续数据自然对齐）。
- **密钥长度三档**：128/192/256 位 → 对应轮数 **10/12/14**。函数名 `Aes_IV_key128/192/256bit_*`。
- 每轮变换：字节代替(SubBytes) → 行移位(ShiftRows) → 列混淆(MixColumns) → 轮密钥加(AddRoundKey)。
- 最后一轮**不做列混淆**。

### 3.2 本工程用的是「CBC 模式」

函数里的 `IV` 参数就是 CBC（Cipher Block Chaining，密码分组链接）的"初始化向量"。代码用「IV 就地链式」实现：

**加密 `Aes_IV_key256bit_Encrypt`：**
```
明文 ^ IV  →  AES 核心加密  →  密文
然后把 IV 更新为「本块密文」传给下一块
```
**解密 `Aes_IV_key256bit_Decode`：**
```
密文 → AES 核心解密  →  ^ IV  →  明文
然后把 IV 更新为「本块原始密文」传给下一块
```
即第 N 块的 IV = 第 N-1 块的密文；第一块用固定初始 IV。

> 注意命名 `IV_IN_OUT`：输入是"上一块密文/初始 IV"，输出被改写成"当前块密文"，供下一块复用——这就是 CBC 链式解密的精髓。`boot_manager.c` 里持续把同一个 `IV[16]` 缓冲传进去，就自动完成了链路串联。

### 3.3 核心原函数（`AES.c`）

| 函数 | 作用 |
|------|------|
| `Aes_SubBytes` / `Aes_invSubBytes` | 用 S 盒 / 逆 S 盒做 16 字节非线性替换 |
| `ShiftByteNumRows` / `Aes_ShiftRows` / `Aes_invShiftRows` | 4×4 状态矩阵按行循环左移（解密反向）|
| `Get_Calculate_GF28` | 在 GF(2⁸) 域做乘法（列混淆的基础运算）|
| `Aes_MixColumns` / `Aes_invMixColumns` | 列混淆（矩阵乘法），用 `aes_MixColumns` / 逆矩阵 |
| `Aes_G_function` | 密钥扩展的 G 函数：字循环左移 + S 盒 + 轮常数 Rcon 异或 |
| `Aes_Bytes_Xor` | 两段字节按位异或（既用于 AddRoundKey，也用于 CBC 链式）|
| `Key_Schedule` / `Aes_Key_Schedule_Create` | 把主密钥扩展成各轮轮密钥（存入全局 `Round_Key[256]`）|

### 3.4 加解密入口（OTA 用 256 位那一档）

```c
void Aes_IV_key256bit_Encrypt(IV_IN_OUT, State_IN_OUT, key256bit); // 加密单块(16B)
void Aes_IV_key256bit_Decode(IV_IN_OUT, State_IN_OUT, key256bit); // 解密单块(16B)
```
- `State_IN_OUT`：加密时输入明文、输出密文；解密时输入密文、输出明文（**原地修改**）。
- 每调用一次只处理 **16 字节**，外部循环按块推进；密钥扩展每次调用都会重算（`Aes_Key_Schedule_Create` 在内部），代码正确但偏慢，对固件升级场景可接受。

> 工程同时带有 `Drivers/.../stm32f4xx_cryp_aes.c`（STM32 硬件 CRYP 外设驱动），但 **OTA 实际走的是这份纯软件实现**，不依赖硬件加密引擎。

---

## 4. AES 在 OTA 中的协同（加密 → 传输 → 存储 → 解密）

### 4.1 数据包的"首块头"

`boot_manager.c` 解密第一块后，从明文末 4 字节取出固件大小（**大端**）：

```c
AppSize = (pTemp[15] << 24) + (pTemp[14] << 16) + (pTemp[13] << 8) + pTemp[12];
```

说明产线工具在加密前，会在固件头部插入一个 16 字节自定义头，其**最后 4 字节是固件长度（大端）**，前 12 字节为自定义内容。解密后首块既要还原这个头，也要用它定位后续数据长度。

### 4.2 Bootloader 侧解密落盘（`boot_manager.c`）

`back_to_app()` / `copy_app_to_executable()` 流程：
1. 读 W25Q64 第一块（16 字节），`Aes_IV_key256bit_Decode` 解密得到首块头，解析 `AppSize`。
2. 计算需要解密的块数 `readDataCount = AppSize/16 (+1 若有余数)`。
3. 擦除内部 Flash 运行区（`flash_erase`）。
4. 逐块：从 W25Q64 读 16 字节 → `Aes_IV_key256bit_Decode`（IV 自动链式）→ `flash_write` 写到运行区（每 16 字节分 4 次、每次 4 字节）。
5. 校验写入正确性后，`JumpToApp1()` 跳转到 `ApplicationAddress_boot` 运行新固件。

### 4.3 三处要特别留意的"坑"

1. **地址不一致**：`common.h` 里 `BackAppAddress = 0x08020000`（备份/下载区），`AppLicationAddress = 0x08008000`；而 `boot_manager.h` 的 `ApplicationAddress_boot = 0x08008000`、`BackupApplicationAddress_boot = 0x08020000`。两份地址**含义对齐但命名不同**，改 Flash 布局时务必同步。
2. **首块头大小端**：`AppSize` 是大端存储，产线加密工具必须一致，否则长度解析错位。
3. **W25Q64 写缓冲**：`W25Q64_WriteData` 按 4096 字节（`SUBSECTOR_SIZE`）为一块攒够才擦/写扇区，`Ymodem_Receive_2buffer` 收完必须调用 `W25Q64_WriteData_End()` 把残余数据刷盘，否则末尾固件丢失。

---

## 5. 关键函数速查表

| 函数 | 文件 | 角色 |
|------|------|------|
| `Ymodem_Receive_2buffer` | ymodem.c | OTA 接收主入口（双缓冲、写 W25Q64）|
| `Receive_Packet` | ymodem.c | 收单包 + 序号/CRC 校验 |
| `Receive_Byte` / `Send_Byte` | ymodem.c | 串口字节收发 |
| `crc_xmodem` | crccitt.c | CRC-16/XMODEM 校验计算 |
| `W25Q64_WriteData` / `_End` | bsp_w25qxx_handler.c | 外部 Flash 落盘 |
| `Aes_IV_key256bit_Decode` | AES.c | Bootloader 端逐块 CBC 解密 |
| `Aes_IV_key256bit_Encrypt` | AES.c | 产线端逐块 CBC 加密（对称配对）|
| `Key_Schedule` / `Aes_Key_Schedule_Create` | AES.c | 轮密钥扩展 |
| `back_to_app` / `copy_app_to_executable` | boot_manager.c | Bootloader 解密→写内部 Flash |
| `JumpToApp1` | boot_manager.c | 跳转到新固件 |

---

*附：本工程 OTA 状态机见 `APP/OTA/ota.c` 的 `E_Ota_State`：`WaitReqDownload → OtaDownload → WaitReqUpdate → OtaEnd`，由串口命令 `0x11 0x22 0x33` 触发下载、`0x55 0x66 0x77` 触发复位升级。*
