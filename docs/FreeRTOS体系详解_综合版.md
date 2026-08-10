# FreeRTOS 体系详解：从底层到应用（综合版）

> **综合版**：本文以"七层体系"为主线，层层递进，把《FreeRTOS 核心机制源码级讲解（第 2-11 点）》《直观化补充讲义》《体系分层导览》三份文档整合为一份。每一层按 **宏观职责 → 数据结构 → 关键函数逐个拆解（源码 + 实现步骤）** 展开。
> 源码版本：FreeRTOS Kernel V11.1.0，Cortex-M4F 单核移植。行号为当前工作区文件行号。
> 阅读对象：具备 5-8 年经验的嵌入式工程师。建议打开源码对照阅读。

---

## 目录

- 阅读指南
- **第 0 层** 硬件抽象（port 层）—— 地基
- **第 1 层** 基础设施（链表 / 内存 / TCB）—— 骨架
- **第 2 层** 调度核心 —— 心脏
- **第 3 层** 任务服务 —— API 层
- **第 4 层** 队列体系 —— 水管
- **第 5 层** 高级组件 —— 三个独立附件
- **第 6 层** 应用层 —— 配置与整机
- 附录 A 常见误区速查
- 附录 B 调试器实验建议
- 附录 C 文档地图

## 阅读指南

- **第一次读**：通读 L0→L3，重点理解 §2.5"事件列表机制"——它是理解上层所有组件的钥匙。
- **第二次读**：L4→L5 对照源码，动手改改参数看行为变化。
- **面试前**：只看每层的"一句话"和附录 C 的依赖总表。
- **函数讲解格式**：每个关键函数按「一句话 → 源码 → 实现步骤 → 关键点/类比 → 被谁用」组织。

---

# 第 0 层 硬件抽象（port 层）—— 地基

## 0.1 一句话

**port 层就干三件事：把寄存器现场当数据存/取（栈帧）、提供 tick 心跳、提供"该切换了"的执行点（PendSV）。** 内核其余部分完全不关心具体芯片。

**核心文件**：`OS/port/portmacro.h`（宏与类型）、`OS/port/port.c`（含汇编的异常处理器）。

## 0.2 类型与宏：让内核代码与硬件位数解耦

```c
/* portmacro.h */
typedef uint32_t StackType_t;   /* 栈单元宽度（M4 为 32 位） */
typedef uint32_t TickType_t;    /* 若 configUSE_16_BIT_TICKS=0 则为 32 位 */
typedef long BaseType_t;        /* 返回布尔/错误码的类型 */
```

- `TickType_t` 的位数决定了 tick 计数回绕周期：32 位约 49 天（1ms tick），16 位约 65 秒。
- **如何实现"开关中断"**：不用 `CPSID I`（关总中断），而是操作 BASEPRI：

```c
__asm volatile (
    "   mov %0, %1    \n"
    "   msr basepri, %0 \n"
    : "=r"(ulNewBASEPRI) : "i"(configMAX_SYSCALL_INTERRUPT_PRIORITY) : "memory" );
```

- BASEPRI 掩蔽**数值 ≥ 阈值**的中断（数值越小逻辑优先级越高）。所以真正的高优先级中断（数值小于阈值）在临界区内仍可响应——这是"临界区里还能响应高优先级中断"的机制根源。
- **关键点**：`configMAX_SYSCALL_INTERRUPT_PRIORITY` 不能设为 0，否则等于不掩蔽任何中断。

## 0.3 pxPortInitialiseStack：伪造一个"刚被打断的现场"

**一句话**：任务第一次运行看起来就像一次普通的异常返回——因为它本来就是。

```c
/* port.c:204，从高地址往低地址填 */
*pxTopOfStack = portINITIAL_XPSR;                      /* xPSR（置 Thumb 位） */
*pxTopOfStack = pxCode & portSTART_ADDRESS_MASK;       /* PC = 任务函数入口 */
*pxTopOfStack = portTASK_RETURN_ADDRESS;               /* LR = prvTaskExitError */
pxTopOfStack -= 5;                                     /* 跳过 R12, R3, R2, R1 */
*pxTopOfStack = pvParameters;                          /* R0 = 任务参数 */
*pxTopOfStack = portINITIAL_EXC_RETURN;                /* EXC_RETURN = 0xFFFFFFFD */
pxTopOfStack -= 8;                                     /* 跳过 R11-R4，返回栈顶 */
```

**实现步骤**：栈布局自上而下为「硬件 8 寄存器（xPSR/PC/LR/R12/R3-R0）→ EXC_RETURN → R11-R4」。返回的 `pxTopOfStack` 指向 R4（最低地址）。

**关键点**：
- `0xFFFFFFFD` 表示"返回线程模式、使用 PSP"，硬件和 PendSV 恢复寄存器后直接跳进任务函数；
- 布局顺序**必须**与 Cortex-M 异常返回时硬件自动弹出的顺序一致，否则现场错乱。

## 0.4 vPortStartScheduler + vPortSVCHandler：启动第一个任务

**一句话**：设置异常优先级 → 启动 SysTick → 用 SVC 触发异常返回，任务从此"活"起来。

```c
/* port.c:308 */
portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;    /* PendSV 设为最低优先级 */
portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;   /* SysTick 设为最低优先级 */
portNVIC_SHPR2_REG = 0;                       /* SVC 设为最高优先级 */
/* ...配置 SysTick 周期... */
vPortStartFirstTask();                        /* 触发 SVC */
```

```asm
/* vPortSVCHandler，port.c:262 */
ldr r3, pxCurrentTCBConst2
ldr r1, [r3]
ldr r0, [r1]             ; r0 = 第一个任务的 pxTopOfStack（TCB 第一个成员！）
ldmia r0!, {r4-r11, r14} ; 恢复 R4-R11 和 EXC_RETURN
msr psp, r0              ; PSP = 栈中硬件 8 寄存器处
mov r0, #0
msr basepri, r0          ; 开中断
bx r14                   ; 异常返回 → 硬件弹 8 寄存器 → 进入任务函数
```

**关键点**：SVC 设为最高优先级、PendSV/SysTick 设为最低——这保证"应用中断可以嵌套在 tick 之上，而 PendSV 永远在所有应用中断之后执行"。

## 0.5 xPortPendSVHandler：上下文切换的全部秘密

**一句话**：保存"旧任务剩下的寄存器"→ 选新任务 → 恢复"新任务的寄存器"，全部发生在异常返回边界。

```asm
/* port.c:507 */
mrs r0, psp                  ; r0 = 当前任务栈顶
tst r14, #0x10               ; 判断任务是否用过 FPU
it eq
vstmdbeq r0!, {s16-s31}      ; 用过才保存 FPU 扩展寄存器
stmdb r0!, {r4-r11, r14}     ; 保存 R4-R11 + EXC_RETURN（硬件已存了 8 个）
str r0, [r2]                 ; 新栈顶写回 pxCurrentTCB->pxTopOfStack

msr basepri, r0              ; 关中断（保护就绪链表）
bl vTaskSwitchContext        ; 选新任务 → pxCurrentTCB 更新
mov r0, #0
msr basepri, r0              ; 开中断

ldr r1, [r3]
ldr r0, [r1]                 ; 新任务栈顶
ldmia r0!, {r4-r11, r14}     ; 恢复新任务的 R4-R11 + EXC_RETURN
tst r14, #0x10
it eq
vldmiaeq r0!, {s16-s31}
msr psp, r0
bx r14                       ; 异常返回 → 新任务开始运行
```

**实现步骤**：
1. **硬件已自动完成**：进异常时压入 xPSR/PC/LR/R12/R3-R0（8 个）；
2. PendSV 只补压 R4-R11 和 EXC_RETURN（以及可选的 FPU 寄存器）；
3. 栈顶写回 TCB → 旧任务现场封存完毕；
4. `vTaskSwitchContext` 期间用 BASEPRI 关中断，防止其他中断篡改就绪链表；
5. 恢复新任务现场，`bx r14` 完成"换人"。

**关键点**：
- 为什么 `pxTopOfStack` 必须是 TCB 第一个成员——汇编直接 `ldr r0, [r1]` 零偏移读取；
- PendSV 运行在**主栈 MSP**，任务现场在**进程栈 PSP**，两者分离（这是任务栈溢出检测要看 PSP 的原因）；
- FPU 采用 lazy stacking：只有 `EXC_RETURN` bit4=0（任务真用过 FPU）才保存 S16-S31，S0-S15 由硬件 lazy save 处理。

## 0.6 xPortSysTickHandler：心跳

```c
/* port.c:564 */
if( xTaskIncrementTick() != pdFALSE )
{
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;  /* 写 PENDSVSET */
}
```

**实现逻辑**：SysTick 只负责"决定要不要切换"，真正切换统一交给 PendSV。多个中断可以"攒"到所有高优先级中断处理完后再统一切一次——这就是 PendSV 存在的意义（避免切换抖动）。

## 0.7 临界区与中断优先级约束

```c
/* port.c:478 */
void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();   /* BASEPRI = 阈值 */
    uxCriticalNesting++;        /* 嵌套计数 */
}
void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting );
    uxCriticalNesting--;
    if( uxCriticalNesting == 0 )
    {
        portENABLE_INTERRUPTS();
    }
}
```

- **嵌套计数归零才真正开中断**——保证嵌套临界区最外层退出前中断始终被掩蔽；
- ISR 里必须用 `taskENTER_CRITICAL_FROM_ISR()`（返回旧 BASEPRI 由配对宏恢复）；
- `vPortValidateInterruptPriority`（port.c:855）在每个 FromISR API 入口检查：NVIC 数值必须 ≥ `configMAX_SYSCALL_INTERRUPT_PRIORITY`，且优先级分组必须全部分配为抢占位。本项目 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`，即数值 0-4 的中断只能做纯硬件处理，5-15 才能调 FromISR API。

## 0.8 本层小结

| 为上层提供 | 说明 |
|---|---|
| 任务的"肉身" | `pxPortInitialiseStack` 构造的初始栈帧 |
| tick 心跳 | `xPortSysTickHandler` → `xTaskIncrementTick` |
| 切换执行点 | PendSV 汇编 + `vTaskSwitchContext` |
| 临界区 | BASEPRI 掩蔽 + 嵌套计数 |
| 启动能力 | SVC 启动第一个任务 |

**一句话**：L0 之后，内核看到的"CPU"就只剩一个黑盒接口——能存现场、能给 tick、能切任务。

---

# 第 1 层 基础设施 —— 骨架

## 1.1 一句话

**链表、内存、TCB 是调度器的积木。嵌入式没有标准库可用，FreeRTOS 全自研。**

## 1.2 链表（list.c）：O(1) 插入删除 + 轮转游标

**数据结构**：

```c
typedef struct xLIST_ITEM
{
    TickType_t xItemValue;    /* 排序键值 */
    struct xLIST_ITEM * pxNext;
    struct xLIST_ITEM * pxPrev;
    void * pvOwner;           /* 指向 TCB */
} ListItem_t;

typedef struct xLIST
{
    UBaseType_t uxNumberOfItems;
    ListItem_t * pxIndex;     /* 轮转游标：停在"上次选中"的任务上 */
    MiniListItem_t xListEnd;  /* 哨兵节点（空链表也存在的"地板"） */
} List_t;
```

**四个关键操作**（这就是链表层的全部 API）：

| 函数 | 如何实现 | 用途 |
|---|---|---|
| `vListInsert` | 从哨兵开始向前遍历，找到第一个 `xItemValue` 不大于自己的节点，插到它后面（升序） | 延时列表（按唤醒时刻）、事件列表（按优先级） |
| `vListInsertEnd` | 插到 `pxIndex` 前面（即链表尾部，O(1)） | 就绪链表（同优先级 FIFO） |
| `uxListRemove` | 直接改前后节点的指针，O(1) 摘除 | 状态迁移 |
| `listGET_OWNER_OF_NEXT_ENTRY` | `pxIndex=pxIndex->pxNext`；若到哨兵则跳回队头；返回 `pvOwner` | **时间片轮转的核心** |

**关键点**：
- 哨兵节点让"空链表"也有确定的遍历边界；
- `listGET_OWNER_OF_NEXT_ENTRY` 每次取"pxIndex 的下一个"而不是队头——这就是同优先级任务轮流获得 CPU 的机制（见 §2.3）。

## 1.3 内存（heap_x.c）：heap_4 的空闲链表

**一句话**：heap_4 = 首地址对齐的空闲块链表 + 分配时分裂 + 释放时合并相邻块。

**如何实现 `pvPortMalloc`（heap_4）**：
1. 挂起调度器（`vTaskSuspendAll`）；
2. 遍历空闲块链表，找第一个 `xWantedSize` 能放下的块（first-fit）；
3. 若块剩余足够大（≥ `heapMINIMUM_BLOCK_SIZE`），**分裂**成"分配块 + 新空闲块"，空闲块插回链表；
4. 返回块用户区起始地址（块头内嵌 `xBlockLink` 记录大小与状态）；
5. 恢复调度器。

**如何实现 `vPortFree`**：把块标记为空闲，插入空闲链表，然后**尝试与前后相邻空闲块合并**（减少碎片）。

**关键点**：
- 块头就放在内存块内部（零额外开销的元数据）；
- `xPortStartScheduler` 里会 `prvHeapInit` 建立初始空闲块；
- 5 个堆策略对比：heap_1 只分配不释放 / heap_2 释放不合并 / **heap_4 合并（默认）** / heap_5 多段内存。

## 1.4 TCB（task.h）：任务的身份证

```c
typedef struct tskTaskControlBlock
{
    volatile StackType_t * pxTopOfStack;   /* 必须是第一个成员（PendSV 直接读） */
    ListItem_t xStateListItem;             /* 挂在"状态链表"：决定任务状态 */
    ListItem_t xEventListItem;             /* 挂在"事件链表"：决定等什么 */
    UBaseType_t uxPriority;
    StackType_t * pxStack;
    char pcTaskName[ configMAX_TASK_NAME_LEN ];
    UBaseType_t uxBasePriority;            /* 优先级继承用 */
    UBaseType_t uxMutexesHeld;
    volatile uint32_t ulNotifiedValue[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
    volatile uint8_t ucNotifyState[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
} TCB_t;
```

**关键点**：
- 一个任务"同时在两条链表里"是常态：阻塞在队列上时，`xStateListItem` 在延时列表（决定何时醒），`xEventListItem` 在队列等待列表（决定等谁）；
- **任务状态 = xStateListItem 挂在哪条链**：就绪链表→Ready、延时列表→Blocked、挂起列表→Suspended、等待终止列表→Deleted。`eTaskGetState` 就是查链表归属推导状态的（tasks.c:2495）。

## 1.5 本层小结

| 为上层提供 | 说明 |
|---|---|
| 通用容器 | 双向循环链表（排序插入 / 轮转游标） |
| 内存来源 | heap_4 空闲链表分配 / 合并 |
| 任务实体 | TCB（栈顶、状态、优先级、通知值） |

---

# 第 2 层 调度核心 —— 心脏

## 2.1 一句话

**调度核心管四件事：谁在跑（就绪链表+位图）、谁先醒（延时列表）、怎么等事件（事件列表）、怎么原子改（调度器挂起）。** 全部在 `tasks.c`。

## 2.2 就绪链表与位图

```c
/* tasks.c:459 */
static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];
static List_t xDelayedTaskList1, xDelayedTaskList2;
static List_t * volatile pxDelayedTaskList;          /* 当前延时列表 */
static List_t * volatile pxOverflowDelayedTaskList;  /* 溢出延时列表 */
static List_t xPendingReadyList;                     /* 挂起期间被唤醒的任务 */
```

**如何实现"记录最高就绪优先级"**（`prvAddTaskToReadyList`，tasks.c:265）：

```c
taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );
listINSERT_END( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) );
```

- `taskRECORD_READY_PRIORITY` → `portRECORD_READY_PRIORITY`：在 `uxTopReadyPriority` 位图上**置位**（bit = 优先级）；
- `listINSERT_END`：尾部插入，同优先级 FIFO。

**关键点**：优化模式下 `uxTopReadyPriority` 是**位图**而不是数值——这样可以用一条 CLZ 指令算出最高置位（O(1) 选任务）。例：就绪 P0、P1、P3 → 位图 `0b1011` → CLZ 得最高 = 3。

## 2.3 选任务：vTaskSwitchContext

```c
/* tasks.c:215 */
#define taskSELECT_HIGHEST_PRIORITY_TASK()                                 \
    do {                                                                    \
        UBaseType_t uxTopPriority;                                          \
        portGET_HIGHEST_PRIORITY( uxTopPriority, uxTopReadyPriority );      \
        listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
    } while( 0 )
```

**如何实现**：
1. `portGET_HIGHEST_PRIORITY`（portmacro.h:136，CLZ 位图优化）：`32 - __clz(uxTopReadyPriority) - 1` 一步算出最高优先级；
2. `listGET_OWNER_OF_NEXT_ENTRY`：在该优先级链表上取 **pxIndex 的下一个**任务；
3. 若 `configMAX_PRIORITIES > 32`（本项目 ≤32 不用），退化为通用代码从高位往下逐个找非空链表（tasks.c:178）。

**时间片轮转的真相**：pxIndex 每次都停在"上次选中的任务"上，下次从它后面选——同优先级任务轮流获得 CPU。如果只有一个任务，取到的还是它自己（自我轮转，等价于不切换）。

## 2.4 tick 与延时列表

### xTaskIncrementTick：tick 中断的核心（tasks.c:4671）

```c
xTickCount = xConstTickCount;
if( xConstTickCount == ( TickType_t ) 0U )
{
    taskSWITCH_DELAYED_LISTS();   /* tick 回绕：交换两条延时列表 */
}
```

**如何实现**：
1. `xTickCount` 递增（由硬件 SysTick 周期决定）；
2. 若 tick 回绕到 0 → `taskSWITCH_DELAYED_LISTS()`：交换当前/溢出列表，`xNumOfOverflows++`，`prvResetNextTaskUnblockTime()`；
3. 循环：取延时列表队头任务，若 `xConstTickCount < 唤醒时刻` → 更新 `xNextTaskUnblockTime` 并 break（**队头没到，后面一定没到**）；
4. 到期则摘出该任务、移除事件列表项、`prvAddTaskToReadyList`；
5. 若唤醒了更高优先级任务 → 置 `xSwitchRequired = pdTRUE`（调用方据此置 PendSV）。

### prvAddCurrentTaskToDelayedList：任务如何入睡（tasks.c:8416）

```c
xTimeToWake = xConstTickCount + xTicksToWait;         /* 允许加法溢出 */
listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );
if( xTimeToWake < xConstTickCount )                   /* 溢出 → 唤醒点落在回绕之后 */
    vListInsert( pxOverflowDelayedList, ... );
else
{
    vListInsert( pxDelayedList, ... );
    if( xTimeToWake < xNextTaskUnblockTime )
        xNextTaskUnblockTime = xTimeToWake;
}
```

**关键点**：
- **双列表应对 tick 回绕**：唤醒时刻"看起来很小"的任务其实是溢出后的未来，放溢出列表；回绕瞬间两表交换；
- `xNextTaskUnblockTime` 是"最近闹钟"，tick 中断只检查它，避免遍历整条链表；
- `xTicksToWait == portMAX_DELAY` 且允许无限阻塞 → 直接进挂起列表（不受回绕影响）。

### 相对延时 vs 绝对延时

- `vTaskDelay`（tasks.c:2435）：唤醒点 = **本次调用时刻** + 延时 → 被抢占会漂移；
- `xTaskDelayUntil`（tasks.c:2343）：唤醒点 = `*pxPreviousWakeTime + xTimeIncrement`（绝对）→ 固定周期不累积误差；源码专门处理 tick 回绕的四种情形。

## 2.5 事件列表：★★ 理解上层一切组件的钥匙 ★★

**任务阻塞的本质** = 把自己的 `xEventListItem` 挂到某个"事件源"的等待列表上，然后进延时列表。

### vTaskPlaceOnEventList：挂到事件源上（tasks.c:5244）

```c
listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), configMAX_PRIORITIES - uxPriority );
vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );  /* 按值升序 = 按优先级降序 */
prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
```

**如何实现**：事件列表项的值是 `configMAX_PRIORITIES - uxPriority`，值越小优先级越高，`vListInsert` 升序插入后**队头就是最高优先级等待者**。随后进延时列表等待唤醒或超时。

### xTaskRemoveFromEventList：从事件源唤醒（tasks.c:5342）

```c
pxTCB = listGET_OWNER_OF_HEAD_ENTRY( pxEventList );   /* 只取队头（最高优先级） */
...
if( uxSchedulerSuspended == 0 )                       /* 调度器未挂起 */
{
    listREMOVE_ITEM( &( pxTCB->xStateListItem ) );
    prvAddTaskToReadyList( pxTCB );                   /* 直接进就绪链表 */
}
else
{
    listINSERT_END( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
}                                                     /* 挂起期间先放 pending */
```

**关键点**：
- 唤醒顺序天然"高优先级优先"（队头）；
- 调度器挂起期间不能动就绪链表 → 放 `xPendingReadyList`，恢复时统一搬；
- 若唤醒的任务优先级更高，返回 `pdTRUE`（调用方据此 yield）。

**这就是"等 X"的通用机制**：队列的等接收、事件组的等位、定时器守护任务的等命令，全部是 `vTaskPlaceOnEventList` + `xTaskRemoveFromEventList` 的不同实例。看懂本节 = 看懂一半上层组件。

## 2.6 调度器挂起：原子地操作一组链表

```c
/* vTaskSuspendAll（tasks.c:3811）——只做计数 */
uxSchedulerSuspended++;
```

```c
/* xTaskResumeAll（tasks.c:3957）——计数归零时 */
1. 把 xPendingReadyList 里的任务搬进就绪链表；
2. 若有更高优先级任务，置 xYieldPendings；
3. 处理挂起期间累积的 xPendedTicks，逐个调用 xTaskIncrementTick（补 tick，不丢时间）。
```

**关键点**：这就是"挂起调度器期间不能调用带非零阻塞时间的 API"的原因——`xTaskCheckForTimeOut` 依赖 tick 推进，而 `prvAddCurrentTaskToDelayedList` 改链表时不能被打断。

## 2.7 优先级继承（供互斥量使用）

### xTaskPriorityInherit（tasks.c:6581）

```c
if( pxMutexHolderTCB->uxPriority < pxCurrentTCB->uxPriority )
{
    if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ pxMutexHolderTCB->uxPriority ] ),
                                 &( pxMutexHolderTCB->xStateListItem ) ) )
    {
        uxListRemove( &( pxMutexHolderTCB->xStateListItem ) );
        portRESET_READY_PRIORITY( pxMutexHolderTCB->uxPriority, uxTopReadyPriority );
    }
    pxMutexHolderTCB->uxPriority = pxCurrentTCB->uxPriority;  /* 持有者临时升到等待者高度 */
    prvAddTaskToReadyList( pxMutexHolderTCB );
}
```

**实现步骤**：持有者优先级低 → 从旧优先级就绪链表摘出 → 改 `uxPriority` → 按新优先级插回。`uxBasePriority` 保持不变（作为恢复基准）。若持有者已继承过（优先级不低了），只更新数值不挪链表。

### xTaskPriorityDisinherit（tasks.c:6684）

```c
( pxTCB->uxMutexesHeld )--;
if( pxTCB->uxPriority != pxTCB->uxBasePriority )
{
    if( pxTCB->uxMutexesHeld == 0 )          /* 不再持有任何互斥量才恢复 */
    {
        uxListRemove( ... );
        pxTCB->uxPriority = pxTCB->uxBasePriority;
        prvAddTaskToReadyList( pxTCB );
    }
}
```

**关键点**：`uxMutexesHeld` 检查防止"先给 A 再给 B"导致优先级提前跌落；超时路径用 `vTaskPriorityDisinheritAfterTimeout` 恢复为 `max(基础优先级, 剩余等待者最高优先级)`——持有者的优先级不能简单恢复，要看"全场"。

## 2.8 超时机制：TimeOut_t 抗 tick 回绕

```c
typedef struct xTIME_OUT
{
    BaseType_t xOverflowCount;   /* 进入时内核回绕过几次 */
    TickType_t xTimeOnEntering;  /* 进入时的 tick */
} TimeOut_t;
```

`xTaskCheckForTimeOut`（tasks.c:5535）：

```c
xElapsedTime = xConstTickCount - pxTimeOut->xTimeOnEntering;
if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) &&
    ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) )
{
    xReturn = pdTRUE;                          /* 完整回绕过一圈 → 必超时 */
}
else if( xElapsedTime < *pxTicksToWait )
{
    *pxTicksToWait -= xElapsedTime;            /* 未超时：扣掉已等时间，重新计时 */
    vTaskInternalSetTimeOutState( pxTimeOut );
    xReturn = pdFALSE;
}
```

**实现要点**：阻塞 API 的多轮循环中，每次被唤醒都重新计算剩余时间——避免"每次重新开始计时"。类比：电梯里手机日历翻了一天（tick 回绕）电梯还没到 → 必超时。

## 2.9 本层小结

| 机制 | 为上层提供 |
|---|---|
| 就绪链表 + 位图 | `prvAddTaskToReadyList`（进入就绪态） |
| CLZ 选任务 | O(1) 切换决策 |
| 双延时列表 | "睡多久、何时醒"（抗回绕） |
| **事件列表** | **"等 X"通用机制（所有组件的地基）** |
| 调度器挂起 | 原子批量改链表 + 补 tick |
| 优先级继承 | 互斥量防饿死能力 |
| TimeOut_t | 超时计算（抗回绕） |

---

# 第 3 层 任务服务 —— API 层

## 3.1 一句话

**所有任务 API 都是同一个模式：改 TCB 字段 + 搬链表 + 必要时请求调度。没有任何新机制，全是 L2 原语的薄封装。**

## 3.2 xTaskCreate：任务的诞生

**一句话**：分配 TCB+栈 → 初始化字段 → 构造初始栈帧 → 挂就绪链表 → 若优先级更高则请求调度。

**实现步骤**（tasks.c:1164）：
1. 从 heap 分配 TCB（`pvPortMalloc`），再分配任务栈（对齐到 8 字节边界）；
2. `prvInitialiseNewTask`：填 TCB 字段（名字、优先级、`uxBasePriority`）、初始化两条链表项、调 `pxPortInitialiseStack`（L0，见 §0.3）构造初始栈帧；
3. `prvAddNewTaskToReadyList`（tasks.c:90 附近）：`prvAddTaskToReadyList` 挂就绪链表；若调度器已运行且新任务优先级更高 → 请求调度（`portYIELD`）。

**关键点**：新任务从"第一次被调度"到"跑起来"的现场，全部来自 §0.3 伪造的栈帧——所以任务函数看起来像"一直就在跑"。

## 3.3 vTaskDelay / xTaskDelayUntil：睡觉的两种方式

**vTaskDelay**（tasks.c:2435）：

```c
vTaskSuspendAll();
{
    prvAddCurrentTaskToDelayedList( xTicksToDelay, pdFALSE );  /* 相对唤醒点 */
}
xAlreadyYielded = xTaskResumeAll();
```

**实现要点**：相对延时。唤醒点 = 调用时刻 + 延时，**若执行中被更高优先级任务抢占，实际唤醒会推迟**（周期漂移）。

**xTaskDelayUntil**（tasks.c:2343）：

```c
xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;   /* 绝对唤醒点 */
...
prvAddCurrentTaskToDelayedList( xTimeToWake - xConstTickCount, pdFALSE );
```

**实现要点**：源码处理 tick 回绕的四种情形（tick 已回绕/未回绕 × 唤醒点已回绕/未回绕），只要唤醒点仍在未来就延时，否则直接执行并更新 `*pxPreviousWakeTime`。**周期任务用它可以做到零累积漂移**。

**关键点**：`vTaskDelay(1)` 实际可能睡 1~2 个 tick——醒来点永远是 tick 边界，取决于调用时点在周期内的位置。

## 3.4 vTaskSuspend / vTaskResume

**vTaskSuspend**（tasks.c:3412）：

```c
if( uxSchedulerSuspended == 0 )  /* 调度器未挂起 → 直接进挂起列表 */
{
    uxListRemove( &( pxCurrentTCB->xStateListItem ) );
    vListInsertEnd( &xSuspendedTaskList, &( pxCurrentTCB->xStateListItem ) );
}
else
{
    /* 挂起中：挂 pending ready 列表，恢复时再处理 */
    vListInsertEnd( &( xPendingReadyList ), &( pxCurrentTCB->xEventListItem ) );
}
```

**实现要点**：从当前状态链表摘出 → 插入挂起列表。注意"调度器已挂起时挂起自己"的特殊路径。

**vTaskResume**：从挂起列表摘出 → 若调度器未挂起则加入就绪链表，若优先级更高则请求调度。

**关键点**：无限阻塞（`portMAX_DELAY`）的任务也住在挂起列表——它们不受 tick 回绕影响，只能被 `vTaskResume` 或事件唤醒。

## 3.5 vTaskDelete：任务的死亡

**一句话**：自删任务不能立刻释放内存——**它的栈和 TCB 正被 CPU 使用**，必须先切走，再由空闲任务回收。

**实现步骤**（tasks.c:2114）：
1. `vTaskDelete(NULL)`：当前任务 TCB 放进 `xTasksWaitingTermination`，请求调度切走；
2. 空闲任务在 `prvCheckTasksWaitingTermination`（tasks.c:6044）里逐个释放 TCB 和栈（`vPortFree`）。

**关键点**：这就是"自删任务后必须保证空闲任务有机会运行"的根源（IDLE 优先级不能全部被长期阻塞的任务饿死）。删除**其他**任务时，若它不在运行，可直接摘链表并在堆支持时立即释放。

## 3.6 本层小结

| API | 底层用的什么 |
|---|---|
| `xTaskCreate` | heap + `pxPortInitialiseStack` + 就绪链表 |
| `vTaskDelay` / `xTaskDelayUntil` | `prvAddCurrentTaskToDelayedList` |
| `vTaskSuspend` / `vTaskResume` | 挂起列表 + 就绪链表 |
| `vTaskDelete` | `xTasksWaitingTermination` + 空闲任务 |

**一句话**：L3 证明了 RTOS 的本质——**API 绝大多数是"改 TCB + 搬链表 + 请求调度"**。

---

# 第 4 层 队列体系 —— 水管

## 4.1 一句话

**一种通用机制（队列）+ 两个特化壳（信号量、互斥量）。** 队列负责传数据；信号量是"零新增代码"的特化；互斥量是唯一跨层组件（借 L2 的优先级继承）。

## 4.2 Queue_t：数据结构

```c
typedef struct QueueDefinition
{
    int8_t * pcHead;               /* 存储区头 */
    int8_t * pcWriteTo;            /* 下一个写入位置 */
    union {
        QueuePointers_t xQueue;      /* pcTail / pcReadFrom */
        SemaphoreData_t xSemaphore;  /* xMutexHolder / uxRecursiveCallCount */
    } u;
    List_t xTasksWaitingToSend;    /* 因队列满而等待的任务 */
    List_t xTasksWaitingToReceive; /* 因队列空而等待的任务 */
    volatile UBaseType_t uxMessagesWaiting;  /* 当前元素数 */
    UBaseType_t uxLength;
    UBaseType_t uxItemSize;
    volatile int8_t cRxLock;       /* 接收锁计数 */
    volatile int8_t cTxLock;       /* 发送锁计数 */
} Queue_t;
```

**关键点**：联合体让同一个结构既能当队列又能当信号量/互斥量——这就是"信号量 = 队列特化"的物理基础（`uxItemSize=0`、`uxLength=1 或 N`）。

## 4.3 xQueueGenericSend：阻塞发送的 8 步流程（queue.c:939）

**一句话**：先试一次（临界区内），不行就"挂起调度器+锁队列+挂事件列表+进延时列表"阻塞等待。

**实现步骤**：
1. **临界区内先试一次**：有空间 → `prvCopyDataToQueue` 写数据，唤醒等待接收的最高优先级任务，成功返回 `pdPASS`；
2. 队列满且 `xTicksToWait == 0` → 返回 `errQUEUE_FULL`（非阻塞失败）；
3. 否则 `vTaskInternalSetTimeOutState` 记录超时起点，**退出临界区**；
4. `vTaskSuspendAll()` + `prvLockQueue()` 锁队列（`cTxLock=queueLOCKED`）；
5. `xTaskCheckForTimeOut` 检查是否超时（见 §2.8）；
6. 未超时且仍满 → `vTaskPlaceOnEventList(&xTasksWaitingToSend, xTicksToWait)`：**把自己挂到"等待发送"事件列表并进延时列表**（见 §2.5）；
7. `prvUnlockQueue()` 补处理锁定期内 ISR 送来/取走的数据；
8. `xTaskResumeAll()` 返回后决定是否 yield；循环回到第 1 步重试。

**关键点**：步骤 3 先退临界区再挂起调度器——因为阻塞操作需要允许被唤醒，不能全程关中断；步骤 6 是"事件列表机制"的队列实例。

## 4.4 xQueueGenericReceive：对称的 8 步流程（queue.c:1504）

与发送完全对称，区别只在：
- 挂在 `xTasksWaitingToReceive` 事件列表；
- 成功路径调用 `prvCopyDataFromQueue`（见下）；
- 队列空且非阻塞 → 返回 `errQUEUE_EMPTY`。

## 4.5 环形缓冲与按值拷贝

**写入**（`prvCopyDataToQueue`，queue.c:2394）：

```c
memcpy( pxQueue->pcWriteTo, pvItemToQueue, pxQueue->uxItemSize );
pxQueue->pcWriteTo += pxQueue->uxItemSize;
if( pxQueue->pcWriteTo >= pxQueue->u.xQueue.pcTail )
    pxQueue->pcWriteTo = pxQueue->pcHead;    /* 回绕 */
```

**读出**（`prvCopyDataFromQueue`，queue.c:2477）：

```c
pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize;
if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail )
    pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;   /* 先移再读！ */
memcpy( pvBuffer, pxQueue->u.xQueue.pcReadFrom, pxQueue->uxItemSize );
```

**关键点**：
- 存储区 = `uxLength * uxItemSize + 1`（多一个字节做尾标记 `pcTail`）；
- 读是**先移动 pcReadFrom 再读**（它总指向"上一次读出的位置"）；
- **按值拷贝而非传指针**：发送方栈变量函数返回后失效，拷贝杜绝悬垂指针；代价是一次 memcpy（4 字节 ≈ 几条 LDR/STR，可忽略）。**所以队列别传大结构体，传指针/句柄**。

## 4.6 FromISR 路径与锁计数

**一句话**：ISR 里不能阻塞，只做"尝试"；如果队列正被锁，就只记录，等 `prvUnlockQueue` 补唤醒。

```c
/* xQueueGenericSendFromISR（queue.c:1159） */
( void ) prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );  /* 先写 */
if( cTxLock == queueUNLOCKED )
{
    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
    {
        if( xTaskRemoveFromEventList( ... ) != pdFALSE )
            *pxHigherPriorityTaskWoken = pdTRUE;   /* 唤醒更高优先级 → 调用方 yield */
    }
}
else
{
    prvIncrementQueueTxLock( pxQueue, cTxLock );   /* 被锁：只记次数，稍后补唤醒 */
}
```

**实现要点**：
- `cTxLock/cRxLock` 记录"锁定期内 ISR 改写了几次"，`prvUnlockQueue`（queue.c:2498）在恢复调度时补处理；
- 这保证三条并发路径安全：任务正在进入阻塞 / ISR 正在收发 / 恢复调度器时统一处理 pending ready。

## 4.7 信号量：零新增代码的特化

**如何创建**：

```c
/* 二进制信号量：xQueueGenericCreate(1, 0) */
/* 计数信号量：  xQueueGenericCreate(uxMaxCount, 0) */
```

**如何操作**：`xSemaphoreTake` = `xQueueSemaphoreTake`（queue.c:1705）= `xQueueGenericReceive` 的封装；`xSemaphoreGive` = `xQueueGenericSend` 的封装。区别只在创建参数（长度、`uxItemSize=0`、`uxQueueType`）。

**关键点**：信号量**没有** `xMutexHolder` 逻辑，不会触发优先级继承——这也是它比互斥量快的结构性原因。

## 4.8 互斥量：队列 + L2 优先级继承

**如何创建**（`xQueueCreateMutex`，queue.c:644）：长度 1、`uxItemSize=0`、`uxQueueType = queueQUEUE_IS_MUTEX`，启用 `SemaphoreData_t`（`xMutexHolder` / `uxRecursiveCallCount`）。

**如何实现"拿到锁"**（`xQueueSemaphoreTake` 成功路径，queue.c:1705）：

```c
pxQueue->u.xSemaphore.xMutexHolder = pvTaskIncrementMutexHeldCount();
```

**如何实现"锁被占时的继承"**（queue.c:1790）：准备阻塞时调 `xTaskPriorityInherit(pxQueue->u.xSemaphore.xMutexHolder)` → 持有者临时升到等待者优先级（见 §2.7）。

**如何实现"释放与去继承"**（`prvCopyDataToQueue` 的互斥量分支，queue.c:2412）：

```c
xReturn = xTaskPriorityDisinherit( pxQueue->u.xSemaphore.xMutexHolder );
pxQueue->u.xSemaphore.xMutexHolder = NULL;
```

**完整剧情**（H 高优先想要锁 / L 低优先持锁 / M 中优先无关）：
1. H 拿锁失败 → 阻塞等待，触发继承：**L 升到 P4，插队到 M 前面**；
2. L 以高优先级尽快用完共享资源；
3. L 释放锁 → 去继承恢复基础优先级，H 被唤醒拿到锁；
4. 若 H 等超时放弃，`vTaskPriorityDisinheritAfterTimeout` 把 L 恢复为 `max(基础优先级, 剩余等待者最高优先级)`——不能简单恢复，要看全场。

**关键点**：互斥量是**唯一跨层**的组件（壳在 queue.c，继承逻辑在 tasks.c）——这正是它比二进制信号量慢但防饿死的原因。

## 4.9 递归互斥量：互斥量 + 计数

```c
/* xQueueTakeMutexRecursive（queue.c:815） */
if( pxMutex->u.xSemaphore.xMutexHolder == xTaskGetCurrentTaskHandle() )
{
    ( pxMutex->u.xSemaphore.uxRecursiveCallCount )++;
    return pdPASS;          /* 自己持锁 → 只加计数 */
}
```

**如何实现**：同一任务重复 take 只加 `uxRecursiveCallCount`；`xQueueGiveMutexRecursive`（queue.c:756）减到 0 才真正释放锁。用于"一个函数内部递归调用同一个临界资源"。

## 4.10 本层小结

| 组件 | 与队列的关系 | 源码 |
|---|---|---|
| 队列 | 本体（环形缓冲 + 双等待列表） | `queue.c` |
| 信号量 | `uxItemSize=0` 的特化（零新增代码） | `queue.c` |
| 互斥量 | 特化 + **L2 优先级继承**（跨层） | `queue.c` + `tasks.c` |
| 递归互斥量 | 互斥量 + 计数 | `queue.c` |

---

# 第 5 层 高级组件 —— 三个独立附件

## 5.1 一句话

**三个附件三种架构范式：任务通知无对象最快、事件组位图广播、软件定时器是"队列+任务"的拼装品。** 它们都不属于队列体系，但都依赖 L2 的调度核心。

## 5.2 任务通知：无对象，直接改 TCB（tasks.c 内）

**数据结构**（TCB 内嵌）：

```c
volatile uint32_t ulNotifiedValue[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
volatile uint8_t ucNotifyState[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
#define taskNOT_WAITING_NOTIFICATION   0
#define taskWAITING_NOTIFICATION       1
#define taskNOTIFICATION_RECEIVED      2
```

**发送**（`xTaskGenericNotify`，tasks.c:7833）：

```c
taskENTER_CRITICAL();
{
    ucOriginalNotifyState = pxTCB->ucNotifyState[ uxIndexToNotify ];
    pxTCB->ucNotifyState[ uxIndexToNotify ] = taskNOTIFICATION_RECEIVED;
    switch( eAction )   /* eSetBits / eIncrement / eSetValueWithOverwrite / eSetValueWithoutOverwrite / eNoAction */
    { ... 修改 ulNotifiedValue ... }
    if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
    {
        listREMOVE_ITEM( &( pxTCB->xStateListItem ) );   /* 接收方在等 → 直接唤醒 */
        prvAddTaskToReadyList( pxTCB );
        taskYIELD_ANY_CORE_IF_USING_PREEMPTION( pxTCB );
    }
}
taskEXIT_CRITICAL();
```

**实现要点**：
- 先改状态再改值，`eSetValueWithoutOverwrite` 用原始状态判断"上一次通知是否已被消费"；
- 接收方正在等 → 摘延时链表进就绪链表（复用 L2 原语）；
- 整个操作在临界区内完成，避免与接收方"先检查再置等待标志"竞争丢通知。

**接收**（`ulTaskGenericNotifyTake`，tasks.c:7615）：

```c
vTaskSuspendAll();
{
    if( pxCurrentTCB->ulNotifiedValue[ uxIndexToWaitOn ] == 0U )
    {
        pxCurrentTCB->ucNotifyState[ uxIndexToWaitOn ] = taskWAITING_NOTIFICATION;
        xShouldBlock = ( xTicksToWait > 0 );
    }
    ...
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
xAlreadyYielded = xTaskResumeAll();
/* 唤醒后：清零或减一消费通知值 */
```

**关键点**："检查值"和"标记自己在等"必须原子（挂起调度器内完成），否则 ISR 在两行之间发通知就会丢。`xTaskGenericNotifyWait`（tasks.c:7718）是位掩码版本，支持 `ulBitsToClearOnEntry/Exit`。

**限制**：接收方是单个任务，不能广播；`eSetValueWithoutOverwrite` 在未消费时会失败。

## 5.3 事件组：位图 + 无序事件列表（event_groups.c）

**数据结构**：

```c
typedef struct EventGroupDef_t
{
    EventBits_t uxEventBits;                 /* 32 位事件位 */
    List_t xTasksWaitingForBits;             /* 等待链表 */
} EventGroup_t;
/* 控制位（高 8 位被内核占用，用户只有低 24 位） */
#define eventCLEAR_EVENTS_ON_EXIT_BIT    0x01000000UL
#define eventUNBLOCKED_DUE_TO_BIT_SET    0x02000000UL
#define eventWAIT_FOR_ALL_BITS           0x04000000UL
```

**等待**（`xEventGroupWaitBits`，event_groups.c:312）：
1. 条件已满足（`prvTestWaitCondition`：`ANY` 用 `& != 0`，`ALL` 用 `& == 目标`）→ 直接返回，可选清位；
2. 不满足且 `xTicksToWait == 0` → 返回当前值；
3. 否则把 `uxBitsToWaitFor | xClearOnExit | xWaitForAllBits` **写进 xEventListItem**，用 `vTaskPlaceOnUnorderedEventList`（tasks.c:5273）挂起——**注意是无序列表**，因为控制位"借用"了事件列表项的值，不能按优先级排序（置 `taskEVENT_LIST_ITEM_VALUE_IN_USE` 防止优先级变化时覆盖）；
4. 唤醒后取回事件项值，若没有 `eventUNBLOCKED_DUE_TO_BIT_SET` 说明是超时；最后清掉控制位只返回真正的位值。

**设置位**（`xEventGroupSetBits`，event_groups.c:547）：

```c
pxEventBits->uxEventBits |= uxBitsToSet;     /* 先置位 */
while( pxListItem != pxListEnd )             /* 再遍历等待者 */
{
    pxNext = listGET_NEXT( pxListItem );     /* 用 pxNext！被唤醒者可能已被摘出 */
    ...按控制位检查条件是否满足...
    if( xMatchFound )
    {
        if( uxControlBits & eventCLEAR_EVENTS_ON_EXIT_BIT )
            uxBitsToClear |= uxBitsWaitedFor;
        vTaskRemoveFromUnorderedEventList( pxListItem,
                                           pxEventBits->uxEventBits | eventUNBLOCKED_DUE_TO_BIT_SET );
    }
    pxListItem = pxNext;
}
pxEventBits->uxEventBits &= ~uxBitsToClear;
```

**实现要点**：用 `pxNext` 而不是 `pxListItem->pxNext`（唤醒的任务可能已被摘出事件列表）；**多个等待者可能同时满足 → 一次性全部唤醒**——比通知的"一个事件唤醒一个任务"更接近广播语义。

**同步屏障**（`xEventGroupSync`，event_groups.c:185）：先设置自己的 `uxBitsToSet`，再等 `uxBitsToWaitFor` 全部就绪，唤醒后无条件清位——"多人到齐后一起继续"的 rendezvous。

**ISR 限制**：ISR 不能直接遍历等待链表，`xEventGroupSetBitsFromISR`（event_groups.c:814）只是把回调挂到软件定时器守护任务（`xTimerPendFunctionCallFromISR`）——所以它依赖 `configUSE_TIMERS`，延迟取决于定时器任务优先级。

## 5.4 软件定时器：队列 + 守护任务（timers.c）

**组件组成**：`Timer_t`（周期、回调、`ucStatus`）、`xTimerQueue`（命令队列）、`xActiveTimerList1/2`（按到期排序的双列表）、`prvTimerTask`（守护任务）。

**守护任务主循环**（timers.c:748）：

```c
for( ; configCONTROL_INFINITE_LOOP(); )
{
    xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );
    prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );
    prvProcessReceivedCommands();
}
```

**如何实现"到期执行回调"**（`prvProcessExpiredTimer`，timers.c:718）：

```c
uxListRemove( &( pxTimer->xTimerListItem ) );
if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0U )
    prvReloadTimer( pxTimer, xNextExpireTime, xTimeNow );   /* 自动重载 */
else
    pxTimer->ucStatus &= ~tmrSTATUS_IS_ACTIVE;
pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );   /* 回调在守护任务上下文 */
```

**如何实现"错过周期补回调"**（`prvReloadTimer`，timers.c:699）：若守护任务被高优先级任务拖住错过多个周期，会**连续补调用错过的周期回调**，而不是把时间折叠到一次。

**如何实现"命令传递"**（`prvProcessReceivedCommands`，timers.c:938）：任务/ISR 调 `xTimerStart/Stop/Reset/ChangePeriod` 时把命令结构体发到 `xTimerQueue`；守护任务按 `xMessageID` 分派，并处理"启动命令到达时已到期"的边界。

**tick 回绕**（`prvSampleTimeNow`，timers.c:871）：发现 `xTimeNow < xLastTime` 时调 `prvSwitchTimerLists` 交换两条活跃列表——和 L2 的延时列表回绕处理同构。

**关键点**：
- 回调运行在**守护任务上下文，不是中断**——回调里长时间阻塞会拖死所有定时器；
- 软件定时器是全书**唯一的"多组件拼装"案例**：队列（传命令）+ 任务（守护进程）+ L2 延时列表（定时）。看懂它 = 串起 L1-L4。

## 5.5 本层小结

| 组件 | 架构范式 | 关键源码 | 代价 |
|---|---|---|---|
| 任务通知 | 无对象（直接改 TCB） | `tasks.c` | 单播 |
| 事件组 | 位图 + 无序事件列表（广播） | `event_groups.c` | 高 8 位被控制位占用 |
| 软件定时器 | 队列 + 守护任务（消息驱动） | `timers.c` | 回调不能阻塞 |

---

# 第 6 层 应用层 —— 配置与整机

## 6.1 一句话

**FreeRTOSConfig.h 的开关 = 条件编译裁剪掉整棵依赖树，决定系统最终形态。**

## 6.2 裁剪矩阵

| 配置项 | 关掉后的效果 | 影响 |
|---|---|---|
| `configUSE_TIMERS` | 守护任务不创建，`xTimer*` API 消失 | L5 定时器（及事件组 FromISR 回调） |
| `configUSE_TASK_NOTIFICATIONS` | TCB 不含通知字段 | L5 通知 |
| `configUSE_MUTEXES` / `configUSE_COUNTING_SEMAPHORES` | 对应创建 API 消失 | L4 |
| `configMAX_PRIORITIES` | 就绪链表数组大小 | L2 |
| `configUSE_PORT_OPTIMISED_TASK_SELECTION` | 位图 O(1) → 通用逐级扫描 | L2 |
| `configTICK_RATE_HZ` | tick 周期 → 定时精度与开销的权衡 | L2 |

## 6.3 依赖总表（面试背诵用）

| 组件 | 直接依赖 | 源码位置 |
|---|---|---|
| 队列 | L2 事件列表 + 调度挂起；L1 链表/内存 | `queue.c` |
| 信号量 | 队列（零新增） | `queue.c` |
| 互斥量 | 队列 + **L2 优先级继承** | `queue.c` + `tasks.c` |
| 事件组 | L2 无序事件列表 + 调度挂起 | `event_groups.c` |
| 任务通知 | TCB + L2 调度器 | `tasks.c` |
| 软件定时器 | 队列 + 任务 + L2 定时列表 | `timers.c` + `queue.c` + `tasks.c` |
| 空闲任务 | L2 就绪链表 | `tasks.c` |

## 6.4 一句线串起整个体系

> "FreeRTOS 用 port 层把 Cortex-M 包成统一接口（L0）；用链表、堆、TCB 搭骨架（L1）；调度核心管状态机、时间、事件唤醒（L2）；任务 API 只是它的薄封装（L3）；队列是一等公民，信号量和互斥量都是它的壳，互斥量额外借用调度器的优先级继承（L4）；任务通知和事件组不经过队列另起炉灶，定时器是队列+任务的拼装品（L5）；最后 FreeRTOSConfig.h 决定整机形态（L6）。"

---

# 附录 A 常见误区速查

| # | 误区 | 真相 |
|---|---|---|
| 1 | "上下文切换是在 PendSV 中断里完成的" | 寄存器保存/恢复在 PendSV 里，但**真正切换发生在 `bx r14` 异常返回边界** |
| 2 | "BASEPRI=5 是关掉优先级低于 5 的中断" | 反了：掩蔽**数值 ≥ 5** 的中断；数值 0-4（逻辑更高）照常响应 |
| 3 | "互斥量比二进制信号量快" | 相反。互斥量有持有者追踪 + 优先级继承开销，更慢但防饿死 |
| 4 | "任务通知是功能受限的慢信号量" | 相反：通知**最快**（无对象无拷贝），代价是只能单播 |
| 5 | "vTaskDelay(1) 保证睡 1 个 tick" | 实际睡 **1~2 个 tick**（醒来点永远是 tick 边界） |
| 6 | "ISR 里能直接调 xQueueReceive 带超时" | 不行。ISR 必须用 FromISR 版本，只能"尝试"，失败立即返回 |
| 7 | "事件组有 32 个事件位可用" | 只有**低 24 位**，高 8 位被控制位占用 |
| 8 | "软件定时器回调在中断里执行" | 在**定时器守护任务**上下文执行，回调不能长时间阻塞 |
| 9 | "uxTopReadyPriority 是个数值" | 在 CLZ 优化模式下是**位图** |
| 10 | "信号量也有优先级继承" | 没有。只有互斥量有（它才需要 `xMutexHolder`） |

# 附录 B 调试器实验建议

| 断点位置 | 观察什么 |
|---|---|
| `vTaskSwitchContext`（tasks.c） | 每次进入 = 一次切换；watch `pxCurrentTCB->pcTaskName` |
| `xTaskIncrementTick` 的 `xConstTickCount == 0` 分支 | tick 回绕 + 双列表交换（可把 `TickType_t` 改 16 位加速触发） |
| `xTaskPriorityInherit`（tasks.c） | watch `pxMutexHolderTCB->uxPriority` 被抬高 |
| `prvCopyDataToQueue`（queue.c） | watch `pxQueue->pcWriteTo` 每次 + uxItemSize，越界回绕 |
| `listGET_OWNER_OF_NEXT_ENTRY` 展开处 | 看 `pxIndex` 游标在同优先级任务间轮转 |

**printf 实验清单**：
1. 两个同优先级任务打印名字 → 观察轮转顺序（pxIndex 游标在转）；
2. 3 任务互斥量实验：L 拿锁后打印 `uxTaskPriorityGet(NULL)`，H 请求锁后应看到 L 被抬到 H 的优先级，释放后恢复；
3. 周期任务统计 100 个周期的抖动：`vTaskDelay` 累积漂移 vs `xTaskDelayUntil` 恒定 ≤1 tick；
4. tick 回绕：调大 `configTICK_RATE_HZ` 或缩小 `TickType_t`，观察 `xNumOfOverflows` 递增。

**现场题**：PendSV 里打断点为什么 `SP` 不是任务栈顶？——PendSV 运行在**主栈 MSP**，任务现场在**进程栈 PSP**；要看任务栈就看 `pxCurrentTCB->pxTopOfStack`。

# 附录 C 文档地图

本仓库 `app/docs/` 下的 FreeRTOS 资料共 4 份，关系如下：

| 文档 | 视角 | 定位 |
|---|---|---|
| **《FreeRTOS体系详解_综合版》（本文）** | 体系 + 机制 | **主文档**：七层主线 + 每层函数逐个拆解 |
| 《FreeRTOS源码级讲解_2-11.md》 | 机制（代码为主） | 细节参考：更完整的源码摘录与行号 |
| 《FreeRTOS直观化补充_配套2-11.md》 | 直观化（内存/类比） | 图解参考：栈帧布局、闹钟模型、插队大戏 |
| 《FreeRTOS直观化演示.html》 | 交互演示 | 动手参考：七层体系图 + 6 个机制模拟器 |

**建议的完整学习路径**：先打开 HTML 演示页的"⓪ 体系总览"逐层搭建一遍建立宏观 → 读本文从 L0 到 L6 逐层深入 → 细节存疑时回查《源码级讲解》和《直观化补充》→ 最后用附录 B 的实验亲手验证。

---

*本文由四份 FreeRTOS 学习资料综合整理，源码版本 V11.1.0（Cortex-M4F）。*

