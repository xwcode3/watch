# FreeRTOS 从零到会用 —— 零基础循序渐进指南

> **这是一份整合文档**。内容筛选自本仓库 `docs/` 下已有的 5 份 FreeRTOS 资料，重新按
> "基础概念 → 基础组件 → 进阶组件 → 实战内功"的顺序编排，语言尽量通俗，面向**零基础初学者**。
>
> 源码环境：本工程 `OS/` 目录 = **FreeRTOS Kernel V11.1.0**，Cortex-M4F 单核移植（芯片 STM32F411CEU6）。
> 文中所有"本工程"指的就是这个手表 App 工程。
>
> 想深入源码细节的读者，读完后按 **附录 C 的资料地图**回查原文档即可。

---

## 目录

**写在前面**

- [0.1 这份文档怎么读](#01-这份文档怎么读)
- [0.2 先搞清一件事：RTOS 到底解决什么问题](#02-先搞清一件事rtos-到底解决什么问题)
- [0.3 本工程的运行环境](#03-本工程的运行环境)

**地基篇（正式开读前先看这一章）**

- [第 0 章 双向循环链表 —— 贯穿全内核的地基](#第-0-章-双向循环链表--贯穿全内核的地基)

**第一部分 基础篇：四项核心设施（必学）**

- [第 1 章 任务 Task —— RTOS 的第一块积木](#第-1-章-任务-task--rtos-的第一块积木)
- [第 2 章 队列 Queue —— 任务之间传数据的水管](#第-2-章-队列-queue--任务之间传数据的水管)
- [第 3 章 信号量 Semaphore —— 只发信号，不传数据](#第-3-章-信号量-semaphore--只发信号不传数据)
- [第 4 章 互斥量与临界区 —— 保护共享资源](#第-4-章-互斥量与临界区--保护共享资源)

**第二部分 进阶篇：三个"外接组件"（基础扎实后再看）**

- [第 5 章 任务通知 Task Notification —— 最快的轻量信号](#第-5-章-任务通知-task-notification--最快的轻量信号)
- [第 6 章 事件组 Event Group —— 多个条件"任意/全部"组合](#第-6-章-事件组-event-group--多个条件任意全部组合)
- [第 7 章 软件定时器 Software Timer —— 会跑回调的"闹钟任务"](#第-7-章-软件定时器-software-timer--会跑回调的闹钟任务)

**第三部分 实战与内功**

- [第 8 章 一张表搞定选型：我该用哪个](#第-8-章-一张表搞定选型我该用哪个)
- [第 9 章 中断与 RTOS 的相处规则（最容易踩坑）](#第-9-章-中断与-rtos-的相处规则最容易踩坑)
- [第 10 章 内存、栈与"我的系统还剩多少水"](#第-10-章-内存栈与我的系统还剩多少水)
- [第 11 章 FreeRTOSConfig.h 配置速览](#第-11-章-freertosconfigh-配置速览)
- [第 12 章 新手十大典型错误与排查清单](#第-12-章-新手十大典型错误与排查清单)
- [第 13 章 动手实验清单（照做就能"看见"机制）](#第-13-章-动手实验清单照做就能看见机制)

**附录**

- [附录 A 概念与 API 速查表](#附录-a-概念与-api-速查表)
- [附录 B 常见误区速查（面试也爱考）](#附录-b-常见误区速查面试也爱考)
- [附录 C 本仓库 FreeRTOS 资料地图](#附录-c-本仓库-freertos-资料地图)

---

# 写在前面

## 0.1 这份文档怎么读

1. **第一次读**：第 0 章（链表地基）→ 第 1 章到第 4 章。这四章是 FreeRTOS 的"四项核心设施"，
   学会它们你已经能写出 90% 的常规嵌入式应用。
   第 0 章务必先看——后面每一章的底层都是链表操作，跳过它会有很多"为什么"想不通。
2. **第二次读**：第 5-7 章的三个进阶组件，第 8 章的选型表帮你决定什么时候该用它们。
3. **随时查**：第 9-12 章是"内功与排错"，写代码卡住时回来看。
4. **动手**：第 13 章有 6 个可以直接照抄的小实验，用 printf 就能验证。

> 每章开头都有一句"**一句话认知**"，先把这句话记住，再往下看细节，不容易迷路。

## 0.2 先搞清一件事：RTOS 到底解决什么问题

### 裸机写法（你大概率已经会）

```c
int main(void)
{
    硬件初始化();
    while (1)
    {
        按键扫描();     // 1 ms 扫一次
        屏幕刷新();     // 可能要 20 ms
        串口处理();     // 数据来了才处理
        传感器读取();   // 要等 80 ms 才出结果
    }
}
```

问题在哪？

- **一个慢动作会拖死所有人**：传感器要等 80 ms，这期间按键没人扫，按键就"丢了"。
- **响应无法保证**：如果屏幕刷新突然变慢，所有东西都跟着变慢。
- **代码越写越乱**：所有功能挤在一个 while 里，加一个新功能要重新算一遍时序。

### RTOS 写法

把每个功能拆成一个独立的**任务**，每个任务**自己管自己的节奏**。任务"等不及"的时候就
主动说"我先睡一会儿，让别人跑"，等条件满足了 RTOS 再把它叫醒。

```c
void 按键任务(void *p)   { for(;;) { 扫描按键(); vTaskDelay(1);   } }
void 屏幕任务(void *p)   { for(;;) { 刷新屏幕(); vTaskDelay(20);  } }
void 传感器任务(void *p) { for(;;) { 发命令(); vTaskDelay(80); 读结果(); } }
```

**一句话认知**：裸机是"一个人按清单轮流干活"，RTOS 是"多个人各干各的，有人专门负责
（调度器）决定此刻该谁上场"。代价是多占一点 RAM（每个任务一份栈）和一点切换开销，
换来的是**结构清晰 + 响应可预期**。

### 三个必须先记住的名词

| 名词 | 通俗解释 |
|---|---|
| **任务（Task）** | 一个独立的 `for(;;)` 函数 + 它自己的一块栈。可以理解为一个"线程"。 |
| **调度器（Scheduler）** | 内核里的裁判。永远让"优先级最高的就绪任务"跑；同优先级则轮流跑。 |
| **tick（节拍）** | 硬件定时器的心跳。本工程 1 ms 一次（`configTICK_RATE_HZ = 1000`），所有延时都以它为单位。 |

## 0.3 本工程的运行环境

| 项目 | 值 | 说明 |
|---|---|---|
| 内核版本 | FreeRTOS Kernel V11.1.0 | `OS/` 目录 |
| 芯片 | STM32F411CEU6（Cortex-M4F） | 100 MHz，128 KB RAM |
| tick 频率 | **1000 Hz（1 ms）** | `FreeRTOSConfig.h:60` |
| 可用优先级 | **0 ~ 4**（共 5 级） | `configMAX_PRIORITIES = 5`，**数字越大优先级越高** |
| 堆大小 | 75 KB（`heap_4`） | `configTOTAL_HEAP_SIZE` |
| 中断优先级位数 | 4 位（数值 0~15） | `__NVIC_PRIO_BITS = 4` |
| 可调 RTOS API 的中断 | **NVIC 数值 ≥ 5** | `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5` |
| 已开启组件 | 互斥量、递归互斥量、计数信号量、软件定时器 | 均为 1 |

> ⚠️ **最容易踩的第一坑**：本工程最多只有 5 个优先级（0-4），`xTaskCreate` 的优先级参数
> 传 5 就会断言失败。写新任务前先规划好优先级表。

---

# 第 0 章 双向循环链表 —— 贯穿全内核的地基

> **为什么要先看这一章？**
> 因为 FreeRTOS 里**几乎没有"算法"，只有"链表搬家"**。
> 任务就绪 = 把它的链表项挂到就绪链表；任务延时 = 挂到延时链表；等队列 = 挂到队列的等待链表；
> 唤醒 = 从原链表摘下、挂回就绪链表。**你后面学的每一个组件，底层都是同一套链表操作。**
> 先花 20 分钟搞懂这 20 行数据结构，后面 4 章会顺很多。

**一句话认知**：内核自己实现了一个**带哨兵、带游标的双向循环链表**，
节点里不存数据，只存一个指向"主人"（通常是 TCB）的指针。

### 0.1 为什么 FreeRTOS 要自己写链表？

嵌入式环境用不了标准库（没有 STL），而且通用链表不满足 RTOS 的三个诉求：

| 诉求 | 说明 |
|---|---|
| **不依赖 libc** | 内核不能指望 `malloc` / 任何库函数 |
| **插入删除 O(1)** | 调度是最高频操作，不能容忍遍历 |
| **支持"轮转"语义** | 需要一个游标来实现同优先级任务轮流获得 CPU（标准链表没有） |

于是就有了 `OS/include/list.h` + `OS/src/list.c` —— 整个内核最底层、最简单的两个文件，
却支撑了上面的一切。

### 0.2 节点：`ListItem_t`（链表的"一节车厢"）

```c
/* OS/include/list.h:144 —— 本工程实际定义（V11.1.0） */
struct xLIST_ITEM
{
    TickType_t         xItemValue;   /* ① 排序键值（升序） */
    struct xLIST_ITEM *pxNext;       /* ② 后指针 */
    struct xLIST_ITEM *pxPrevious;   /* ③ 前指针（注意：是 pxPrevious，不是 pxPrev） */
    void              *pvOwner;      /* ④ 指向"主人"（绝大多数情况 = TCB） */
    struct xLIST      *pxContainer;  /* ⑤ 指向"我当前挂在哪条链表上" */
};
typedef struct xLIST_ITEM ListItem_t;
```

**五个字段逐个讲**：

| 字段 | 作用 | 为什么需要它 |
|---|---|---|
| `xItemValue` | 排序用的键 | 延时列表里存"唤醒时刻"、事件列表里存"优先级倒数值" |
| `pxNext` / `pxPrevious` | 双向链接 | 摘除自己时不需要知道前驱是谁 → **O(1) 删除** |
| `pvOwner` | 指回主人 | 链表里串的是**节点**不是任务，靠它反查 TCB |
| `pxContainer` | 记住所属链表 | 删除时不用遍历找"我在哪条链表" → **O(1) 删除** |

> 💡 **`pxContainer` 的妙用**：它让"摘除一个节点"变成纯 O(1) 操作——
> 节点自己就知道自己属于哪条链表。同时内核用 `listLIST_ITEM_CONTAINER(item) == NULL`
> 来判断"这个节点当前有没有挂在链表上"。

### 0.3 链表头：`List_t`（链表的"车头 + 铁轨端点"）

```c
/* OS/include/list.h:172 */
typedef struct xLIST
{
    UBaseType_t  uxNumberOfItems;   /* ① 链表里有多少个节点 */
    ListItem_t  *pxIndex;           /* ② 轮转游标：停在"上次选中的节点"上 */
    MiniListItem_t xListEnd;        /* ③ 哨兵节点（链表的地板/终点标记） */
} List_t;
```

**哨兵 `xListEnd` 是什么？** 一个**永远存在、不存业务数据**的节点，是循环链表的"接缝"：

```c
/* OS/src/list.c:49  vListInitialise() 的核心 */
pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd );  /* 游标初始指向哨兵 */
pxList->xListEnd.xItemValue = portMAX_DELAY;               /* 值最大 = 永远排最后 */
pxList->xListEnd.pxNext     = ( ListItem_t * ) &( pxList->xListEnd );  /* 空链表时 */
pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd );  /* 自己指向自己 */
pxList->uxNumberOfItems = 0;
```

**哨兵带来的三大好处**（这是双向循环链表的经典技巧）：

1. **空链表也有确定的边界**——不需要 `if (head == NULL)` 特判，遍历时遇到 `xListEnd` 就知道"到底了"；
2. **插入删除无需区分"头/尾/中间"**——所有位置都是"在两个节点中间插"，代码统一；
3. **它的 `xItemValue` 是最大值**（`portMAX_DELAY` = 0xFFFFFFFF），按值升序插入时永远落在队尾，不会被误插到它后面。

```
空链表（初始化后）：              有 2 个节点后：

   ┌──────────────┐                 ┌──────────────┐
   │  xListEnd    │                 │  xListEnd    │
   │ value = MAX  │                 │ value = MAX  │
   └──────┬───────┘                 └───┬──────┬───┘
      ↑   │   ↓                    ┌────┘      └────┐
      └───┘   └──→ 回到自己          ↓               ↑
    (pxNext/pxPrevious 都指向自己)  节点A ⇄ 节点B ────┘
                                     ↑
                              pxIndex 可能停在这里
```

### 0.4 内存视角：一个节点长什么样

```
ListItem_t（节点，20 字节 @32位）
┌────────────────────────────────┐
│ xItemValue    排序键值          │   ← 决定我排在第几位
│ pxNext    ────────────────→ 下一个节点
│ pxPrevious ←────────────── 上一个节点
│ pvOwner   ────────────────→ TCB（真正的主人）
│ pxContainer ──────────────→ 我挂在的 List_t
└────────────────────────────────┘

List_t（链表头）
┌────────────────────────────────┐
│ uxNumberOfItems = 2            │
│ pxIndex ──→ 游标（上次选中的）   │
│ xListEnd  哨兵（值 = MAX）      │
└────────────────────────────────┘
```

**关键认知**：链表里**串的是"钩子"（ListItem_t），不是任务本身**。
任务（TCB）安静地躺在内存里，只是把身上挂着的钩子挂到不同的链表上。
一个 TCB 身上有**两个钩子**（`xStateListItem` 和 `xEventListItem`），所以可以同时挂在两条链表上——
这是理解"阻塞"的关键，见 0.6。

### 0.5 五个操作（这就是链表层的全部 API）

| 操作 | 函数 / 宏 | 复杂度 | 用在哪 |
|---|---|---|---|
| 初始化 | `vListInitialise(pxList)` | O(1) | 内核启动时初始化所有链表 |
| **按值升序插入** | `vListInsert(pxList, pxItem)` | O(n) | 延时列表（按唤醒时刻）、事件列表（按优先级） |
| **尾部插入** | `vListInsertEnd(pxList, pxItem)` | **O(1)** | 就绪链表（同优先级 FIFO） |
| **摘除** | `uxListRemove(pxItem)` | **O(1)** | 一切状态迁移 |
| **取下一个主人** | `listGET_OWNER_OF_NEXT_ENTRY(pxList, pxTCB)` | **O(1)** | 时间片轮转 |

#### `vListInsert`：按值升序，相同值插到最后

```c
/* OS/src/list.c:139（精简） */
for( pxIterator = &pxList->xListEnd;
     pxIterator->pxNext->xItemValue <= xValueOfInsertion;   /* ← 注意是 <= */
     pxIterator = pxIterator->pxNext )
{
    /* 一路找到"第一个比我大的节点"前面停下 */
}
pxNewListItem->pxNext = pxIterator->pxNext;
pxNewListItem->pxNext->pxPrevious = pxNewListItem;
pxNewListItem->pxPrevious = pxIterator;
pxIterator->pxNext = pxNewListItem;
pxNewListItem->pxContainer = pxList;      /* 记住我属于哪条链表 */
pxList->uxNumberOfItems++;
```

> 源码注释里特别解释了为什么用 `<=` 而不是 `<`：
> **相同 `xItemValue` 的节点，新来的排到后面**。就绪链表里同优先级任务的值完全相同，
> 正是靠这个"后来者排后面"，保证了它们能公平地分到 CPU。

#### `vListInsertEnd`：O(1) 插到游标前面（即链表尾部）

就绪链表专用。因为同优先级任务的值都一样，没必要排序，直接挂到尾部即可——**从 O(n) 变成 O(1)**。

#### `uxListRemove`：O(1) 摘除

```c
/* OS/src/list.c:215（精简） */
List_t * const pxList = pxItemToRemove->pxContainer;   /* ← 不用找，节点自己知道 */
pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;
/* 如果摘的正好是游标指向的节点，把游标退到前驱，防止悬空 */
if( pxList->pxIndex == pxItemToRemove ) { pxList->pxIndex = pxItemToRemove->pxPrevious; }
pxItemToRemove->pxContainer = NULL;      /* 标记为"已不在任何链表上" */
pxList->uxNumberOfItems--;
return pxList->uxNumberOfItems;
```

#### `listGET_OWNER_OF_NEXT_ENTRY`：轮转游标（时间片轮转的全部秘密）

```c
/* OS/include/list.h:291 */
( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;      /* 游标前进一格 */
if( ( void * ) ( pxConstList )->pxIndex == ( void * ) &( ( pxConstList )->xListEnd ) )
{
    ( pxConstList )->pxIndex = ( pxConstList )->xListEnd.pxNext;  /* 到哨兵了 → 绕回队头 */
}
( pxTCB ) = ( pxConstList )->pxIndex->pvOwner;                    /* 返回主人的 TCB */
```

**它不是"取队头"，而是"取游标的下一个"**——游标每次都停在"上次选中的任务"上，
所以同优先级的任务会一个个轮流。这就是第 1 章要讲的**时间片轮转**的实现基础。

```
三个同优先级任务 A B C（pxIndex 当前停在 A）：

第1次调用 → pxIndex: A → B，返回 B
第2次调用 → pxIndex: B → C，返回 C
第3次调用 → pxIndex: C → 哨兵 → 绕回 A，返回 A
                                    ↑ 一轮结束，回到起点
```

### 0.6 一个 TCB 怎么"一脚踩两条船"

这是理解"阻塞"最关键的一张图。场景：TaskB（优先级 2）阻塞在队列 Q 上等数据。

```
   ┌────────── 就绪链表 pxReadyTasksLists[3] ─────────┐
   │  [TaskA·P3]                                      │
   └──────────────────────────────────────────────────┘

   ┌────────── 延时列表（Blocked 状态） ───────────────┐
   │  [TaskB] ◄── xStateListItem 挂在这里              │  ← 决定"我什么状态"
   └──────────────────────────────────────────────────┘

   ┌────────── 队列 Q 的 xTasksWaitingToReceive ───────┐
   │  [TaskB] ◄── xEventListItem 挂在这里              │  ← 决定"我在等什么"
   └──────────────────────────────────────────────────┘

内存中的 TaskB TCB：
┌───────────────────────────────────┐
│ pxTopOfStack                      │
│ xStateListItem  ←→ 延时列表        │  ← 状态：Blocked
│ xEventListItem  ←→ 队列 Q 等待列表 │  ← 等谁：队列 Q
│ uxPriority = 2                    │
│ ...                               │
└───────────────────────────────────┘
```

**唤醒 TaskB = 把它的两个钩子都摘下来，再把 `xStateListItem` 挂回就绪链表。**

> 面试一句话：`xStateListItem` 决定**我在哪条队里**（状态），
> `xEventListItem` 决定**我在等什么**（事件源）。

### 0.7 内核里到底有几条链表（先混个脸熟）

| 链表 | 类型 | 排序依据 | 谁住在里面 |
|---|---|---|---|
| `pxReadyTasksLists[prio]` | 数组，每个优先级一条 | 不排序（`vListInsertEnd`） | 就绪任务 |
| `pxDelayedTaskList` / `pxOverflowDelayedTaskList` | 两条（应对 tick 回绕） | 按**唤醒时刻**升序 | 延时/超时等待的任务 |
| `xPendingReadyList` | 单条 | 不排序 | 调度器挂起期间被唤醒的任务 |
| `xSuspendedTaskList` | 单条 | 不排序 | 被 `vTaskSuspend()` 的任务 |
| `xTasksWaitingTermination` | 单条 | 不排序 | 自删任务，等空闲任务回收 |
| 队列的 `xTasksWaitingToSend/Receive` | 每个队列两条 | 按**优先级倒数值**升序 | 因队列空/满而阻塞的任务 |
| 事件组的 `xTasksWaitingForBits` | 每个事件组一条 | **不排序**（无序列表） | 等事件位的任务 |
| 定时器的 `xActiveTimerList1/2` | 两条 | 按**到期时刻**升序 | 活跃的软件定时器 |

看完这张表再回头看第 1~4 章，你会发现：**它们讲的都是"什么时候把钩子从这条链表换到那条链表"**。

### 0.8 使用链表的三条纪律（写驱动/中间件时会用到）

1. **操作前必须保护**：链表不是线程安全的。任务里改链表要进临界区或挂起调度器；
   中断里**绝不直接改**内核链表（只置标志，等 `prvUnlockQueue` / `xPendingReadyList` 补处理）。
2. **遍历中删除要用"下一个"的临时变量**：
   ```c
   pxNext = listGET_NEXT( pxListItem );   /* 先存下一个！ */
   ... 可能把 pxListItem 摘出链表 ...
   pxListItem = pxNext;                   /* 再跳过去 */
   ```
   直接写 `pxListItem = pxListItem->pxNext` 的话，节点一旦被摘除，`pxNext` 就失效了。
   内核的 `xEventGroupSetBits()` 里就有这个经典写法（第 6 章会再见到）。
3. **判断"是否还在链表上"用 `pxContainer`**：`listLIST_ITEM_CONTAINER(item) == NULL` 表示没挂在任何链表上。

### 0.9 顺带纠正原资料里的两处笔误

本仓库早前的几份 FreeRTOS 资料里，链表字段沿用了旧版本的写法，与本工程 V11.1.0 源码有出入：

| 资料里写的 | 本工程源码实际 | 位置 |
|---|---|---|
| `pxPrev` | **`pxPrevious`** | `OS/include/list.h:149` |
| 哨兵值 `0xFFFF` | **`portMAX_DELAY`（0xFFFFFFFF）** | `OS/src/list.c:62` |

另外 `ListItem_t` 现在有 **5 个**字段（多了 `pxContainer`），老资料只列了 4 个。

---

# 第一部分 基础篇：四项核心设施

> 这四章对应 FreeRTOS 官方文档里最常用的部分，也是"会不会用 RTOS"的分水岭。
> **任务**是演员，**队列**是传话筒，**信号量**是信号灯，**互斥量**是钥匙。
> 顺序不能颠倒——后面的都建立在前面的基础上（而它们共同的底座，就是第 0 章的链表）。

---

## 第 1 章 任务 Task —— RTOS 的第一块积木

**一句话认知**：任务 = 一个永不返回的函数 + 一块专属的栈 + 一张身份证（TCB）。
调度器做的事就是"把 CPU 从一个任务的栈切换到另一个任务的栈"。

### 1.1 任务的"身份证"：TCB

每个任务在内核里都有一个结构体，叫 **TCB（Task Control Block，任务控制块）**。你不必记住
它的全部字段，但要知道它装了什么：

```c
typedef struct tskTaskControlBlock
{
    StackType_t * pxTopOfStack;   /* 栈顶指针 —— 必须是第一个成员（内核汇编要用） */
    ListItem_t   xStateListItem;  /* 状态链表项：决定"我在哪条队里" */
    ListItem_t   xEventListItem;  /* 事件链表项：决定"我在等什么" */
    UBaseType_t  uxPriority;      /* 当前优先级 */
    UBaseType_t  uxBasePriority;  /* 基础优先级（优先级继承时用来恢复） */
    char         pcTaskName[10];  /* 任务名（调试用） */
    ...
} TCB_t;
```

**一个关键认知**：任务的所有状态，本质上都记录为"这个任务的链表项挂在哪条链表上"。
（链表本身的构造见**第 0 章**，这里只讲它装了什么。）

> ⚠️ 注意：下面代码里的 `ListItem_t xStateListItem` / `xEventListItem` 是**内嵌在 TCB 里的节点**，
> 不是指针。一个 TCB 自带两个"钩子"，可以同时挂在两条链表上——这是理解阻塞的关键。

```
xStateListItem 挂在就绪链表   → 任务处于"就绪"（Ready）
xStateListItem 挂在延时链表   → 任务处于"阻塞"（Blocked）
xStateListItem 挂在挂起链表   → 任务处于"挂起"（Suspended）
xEventListItem 挂在队列等待链表 → 任务正在"等这个队列"
```

**记忆口诀**：`xStateListItem` 决定**我在哪条队里**，`xEventListItem` 决定**我在等什么**。

### 1.2 任务的五种状态

```
                    ┌──────────────────────────────────────┐
                    │                                      │
   xTaskCreate      ▼                                      │
  ┌─────────┐   创建成功   ┌─────────┐   被调度器选中   ┌───────────┐
  │  (新)   │ ──────────→ │  Ready  │ ──────────────→ │  Running  │
  └─────────┘             │  就绪   │ ←────────────── └───────────┘
                          └─────────┘  被更高优先级抢占       │
                               ▲        或时间片到期          │
                               │                             │ vTaskDelay
                               │ 等待的条件满足 / 超时         │ 等队列/信号量/事件
                               │                             ▼
                          ┌─────────┐                  ┌───────────┐
                          │ Blocked │                  │ Suspended │
                          │  阻塞   │                  │   挂起    │
                          └─────────┘                  └───────────┘
                               ▲                             │
                               │                             │ vTaskResume
                               └─────────────────────────────┘
```

| 状态 | 含义 | 进入方式 | 退出方式 |
|---|---|---|---|
| **Running 运行** | 正占着 CPU | 被调度器选中 | 被抢占 / 主动阻塞 / 挂起 / 删除 |
| **Ready 就绪** | 一切就绪，只差 CPU | 创建、被唤醒、被恢复 | 被调度器选中 |
| **Blocked 阻塞** | 在等某件事（延时、队列、信号量） | `vTaskDelay`、等待 API | 条件满足或超时 |
| **Suspended 挂起** | 被别人"冻住"，不参与调度 | `vTaskSuspend` | `vTaskResume` |
| **Deleted 删除** | 等待内存回收 | `vTaskDelete` | 空闲任务回收 TCB 和栈 |

> ⚠️ **Blocked 和 Suspended 的区别**：阻塞是"我在等一个**具体的事**"（到点会自己醒），
> 挂起是"我被别人**按了暂停**"（只能靠别人 `vTaskResume` 恢复，自己醒不过来）。

### 1.3 创建任务：`xTaskCreate`

```c
BaseType_t xTaskCreate(
    TaskFunction_t  pvTaskCode,      /* ① 任务函数指针 */
    const char *    pcName,          /* ② 任务名字（调试用，本工程最长 10 字符） */
    uint16_t        usStackDepth,    /* ③ 栈深度，单位是"字"不是字节！ */
    void *          pvParameters,    /* ④ 传给任务函数的参数 */
    UBaseType_t     uxPriority,      /* ⑤ 优先级，本工程 0~4 */
    TaskHandle_t *  pxCreatedTask    /* ⑥ 回传的任务句柄（可填 NULL） */
);
```

**逐个参数讲清楚**：

- **① 任务函数**：必须是 `void func(void *param)` 形式，**永远不返回**（见 1.4）。
- **③ 栈深度**：单位是 `StackType_t`（32 位机上是 4 字节）。写 `256` = **1 KB 栈**。
  栈里放：局部变量、函数调用嵌套、寄存器现场（约 60~100 字节）。**不够用会踩内存**，
  现象是莫名其妙死机。保守起步给 256，后面用第 10 章的方法量。
- **⑤ 优先级**：数字越大越优先。本工程只有 0-4 五级。
  优先级 0 被**空闲任务**占用，你的业务任务一般用 1~4。
- **⑥ 句柄**：后面要 `vTaskSuspend`、发通知、删任务时用它。**不需要就填 NULL。**

返回值：`pdPASS` 成功；`errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY` 失败（堆不够）。
**必须判返回值**——本工程 `configUSE_MALLOC_FAILED_HOOK = 0`，分配失败时静默返回，不判就会埋雷。

### 1.4 任务函数长什么样（三条铁律）

```c
void my_task(void *pvParameters)
{
    /* ① 初始化区：只跑一次 */
    硬件初始化();
    int cnt = 0;

    /* ② 主体：必须是死循环 */
    for (;;)
    {
        干活();

        /* ③ 循环体里必须有"让出 CPU"的动作 */
        vTaskDelay(pdMS_TO_TICKS(10));   // 或者等队列、等信号量
    }

    /* ④ 绝对不能 return！真的要退出就调 vTaskDelete(NULL) */
}
```

| 铁律 | 违反后果 |
|---|---|
| 必须 `for(;;)` 死循环 | 函数返回后会跳到内核的 `prvTaskExitError`，触发断言死循环 |
| 循环里必须有阻塞/延时 | 低优先级任务永远饿死；同优先级任务永远轮不到 |
| 不能 `return` | 同上。要结束就 `vTaskDelete(NULL)` |

**为什么任务函数"不能返回"？** 因为调度器把任务函数当成"刚刚被中断打断的普通函数"来启动的
（内核伪造了一个假的栈帧，让 `PC` 指向你的任务函数）。如果它返回了，就相当于"中断返回到一个
不存在的地方"，内核只能死给你看。

### 1.5 延时：`vTaskDelay` 与 `xTaskDelayUntil`

这是新手最容易用错的一对 API。

```c
/* A. 相对延时：从"现在"开始睡 N 个 tick */
void vTaskDelay(TickType_t xTicksToDelay);

/* B. 绝对延时：保证"两次唤醒之间"恰好 N 个 tick */
BaseType_t xTaskDelayUntil(TickType_t *pxPreviousWakeTime, TickType_t xTimeIncrement);
```

**一句话区别**：

- `vTaskDelay(10)` —— "我**从现在起**睡 10 毫秒。"
- `xTaskDelayUntil(&last, 10)` —— "我要保证**每隔** 10 毫秒醒一次。"

**为什么会有差别？** 画个时间轴（每格 1 ms，周期 10 ms）：

```
vTaskDelay(10)：
   干活(2ms) ──── 睡10ms(中途被高优先级抢走3ms) ──── 干活(2ms) ...
   |<── 实际周期 15ms ──>|                            周期越来越长，漂移累积

xTaskDelayUntil(&last, 10)：
   干活 ── 睡(只睡到 last+10) ── 干活 ── 睡(只睡到 last+20) ...
   |<── 10ms ──>|<── 10ms ──>|                       永远不漂移
```

**结论**：

- 只是"歇一会儿"→ `vTaskDelay` 够了；
- **周期性任务（采样、闪烁、上报）必须用 `xTaskDelayUntil`**。

> ⚠️ `vTaskDelay(1)` 实际会睡 **1~2 个 tick**，不是精确 1 ms —— 因为唤醒点只能落在 tick 边界上。
> 用它做精确延时是不靠谱的，精确延时用硬件定时器。

### 1.6 挂起、恢复、删除

```c
void vTaskSuspend(TaskHandle_t xTaskToSuspend);  // 传 NULL = 挂起自己
void vTaskResume (TaskHandle_t xTaskToResume);   // 只能在任务里调
void vTaskDelete (TaskHandle_t xTaskToDelete);   // 传 NULL = 删除自己
```

**`vTaskDelete` 的一个隐藏约束**（面试常考）：

> 自删的任务**不能立刻释放自己的 TCB 和栈**——因为 CPU 此刻正踩在这块内存上。
> 内核只能把它挂到"等待终止"链表，等**空闲任务**跑起来时再回收。
>
> 所以：**只要你的任务会自删，就必须保证空闲任务有机会运行**（别让所有任务都永远阻塞在
> 高优先级上，也别让空闲任务优先级被占）。

### 1.7 调度器是怎么"换人"的（通俗版）

你只要记住这条流水线：

```
① SysTick 每 1ms 中断一次
      │
      ▼
② xTaskIncrementTick()：tick 计数 +1，检查"谁的延时到点了"
      │
      ├── 到点的任务 → 从延时链表摘下 → 挂回就绪链表
      │
      ▼
③ 如果有"比当前任务优先级更高的任务"变成就绪 → 置位 PendSV
      │
      ▼
④ PendSV 异常（优先级最低，所有中断都处理完才执行）：
      ├── 把当前任务的寄存器压进它自己的栈
      ├── 调用 vTaskSwitchContext()：选出该跑的任务
      └── 从新任务的栈里恢复寄存器 → 异常返回 → 新任务继续跑
```

**三个必须知道的点**：

1. **抢占不是"中断打断任务"那么简单**：真正的切换统一发生在 `PendSV` 里，而 PendSV 被
   **最低优先级**（这条怎么实现的、SysTick 为何也最低、代价是什么，见 §1.9）。
   这样多个中断可以"攒一起"，等所有中断处理完再切一次，避免切换抖动。
2. **同优先级怎么分？时间片轮转**。每个优先级一条链表，链表里有个游标 `pxIndex`，每次选
   "游标的下一个"，所以同优先级的任务会**轮流**得到 CPU（游标机制见 §0.5）。
3. **选谁是 O(1) 的**：内核用一个 32 位位图 `uxTopReadyPriority` 记录"哪些优先级有就绪任务"，
   再用一条 ARM `CLZ` 指令直接算出最高位，不用遍历。

```
位图示例（本工程 5 个优先级）：
  只有 IDLE(P0) 就绪        → 位图 0b00001 → 最高 = 0
  创建了传感器任务(P1)      → 位图 0b00011 → 最高 = 1
  创建了温湿度任务(P3)      → 位图 0b01101 → 最高 = 3
  温湿度任务阻塞了          → 位图 0b00101 → 最高 = 2
```

### 1.8 空闲任务（IDLE）

调度器启动时会自动创建一个 **优先级 0 的空闲任务**，它干两件事：

1. 回收被删除任务的 TCB 和栈；
2. （可选）进低功耗、跑 hook 函数。

**新手要记住的两条**：

- **不要**创建优先级 0 的忙循环任务去"占坑"，会把空闲任务饿死；
- 如果你的任务真的"什么也不用等"（比如纯计算），至少放一个 `vTaskDelay(1)` 或
  `taskYIELD()`，给别人留机会。

### 1.9 深入：PendSV / SysTick 的最低优先级是怎么设出来的

这一节回答两个常问的问题：**"最低优先级"是硬件规定的还是软件设的？** 以及
**SysTick 也设成最低，tick 会不会不准？**

#### 1.9.1 是软件写寄存器设的，不是硬件固定

> **关键事实：Cortex-M 复位后，PendSV 和 SysTick 的优先级是 0（最高优先级）！**
> 是 FreeRTOS 的移植层主动把它们改成最低的。

优先级写在 **SCB 的 SHPR（System Handler Priority Register）** 里 —— 注意它和
配置外设中断优先级的 NVIC_IPR 寄存器**不是同一组**：

| 寄存器 | 地址 | 管理的异常 |
|---|---|---|
| `SHPR1` | `0xE000ED18` | MemoryManage / BusFault / UsageFault |
| `SHPR2` | `0xE000ED1C` | **SVC**（bit 31~24） |
| `SHPR3` | `0xE000ED20` | **PendSV**（bit 23~16）、**SysTick**（bit 31~24） |

```
SHPR3 (0xE000ED20)，每个字段 8 位：
  bit 31 ──────── 24   bit 23 ──────── 16   bit 15 ──── 8   bit 7 ──── 0
  ┌─────────────────┬─────────────────┬───────────────┬───────────────┐
  │  SysTick 优先级 │  PendSV 优先级  │ DebugMonitor  │    保留        │
  └─────────────────┴─────────────────┴───────────────┴───────────────┘

  复位值：              0x00            0x00          0x00          —
                      ↑ 0 = 最高优先级（不是最低！）

  FreeRTOS 写入：      0xFF            0xFF
                      ↑ STM32F4 只实现每个字段的【高 4 位】（__NVIC_PRIO_BITS = 4），
                        低 4 位写进去也会被丢弃，读回恒为 0。
                        所以写 0xFF 实际生效的是 0xF0 → 优先级值 = 15 = 最低。
```

> 💡 **写 `0xFF` 而不是 `0xF` 是个小技巧**：不管芯片实现了几位优先级
> （4 位 / 3 位 / 8 位），全写 1 一定得到"该芯片的最低优先级"，移植层不用做条件判断。

本工程源码（`OS/port/port.c:434`）：

```c
/* Make PendSV and SysTick the lowest priority interrupts, and make SVCall
 * the highest priority. */
portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;      /* 0xFF << 16 */
portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;     /* 0xFF << 24 */
portNVIC_SHPR2_REG = 0;                         /* SVC = 0（最高） */
```

配套的宏定义（`OS/port/port.c:67`）：

```c
#define portMIN_INTERRUPT_PRIORITY   ( 255UL )                    /* = 0xFF */
#define portNVIC_PENDSV_PRI          ( portMIN_INTERRUPT_PRIORITY << 16UL )
#define portNVIC_SYSTICK_PRI         ( portMIN_INTERRUPT_PRIORITY << 24UL )
```

⚠️ **一个容易踩的点**：这里**写死了 255**，并没有使用 `configKERNEL_INTERRUPT_PRIORITY`。
也就是说在本版本的移植里，**改 `FreeRTOSConfig.h` 的配置项不会改变 PendSV / SysTick 的
实际优先级**，它们恒为最低。

#### 1.9.2 硬件负责"提供能力"，软件负责"做选择"

| 层面 | 谁做的 | 内容 |
|---|---|---|
| **硬件设计** | ARM Cortex-M 内核 | 提供 PendSV 这个"**软件可挂起**"的系统异常（写 `ICSR.PENDSVSET` 就能挂起，不需要外部信号），ARM 手册明确说它就是为 OS 上下文切换准备的；提供 SHPR 让优先级**可软件配置**；提供**尾链（tail-chaining）**机制 |
| **软件实现** | FreeRTOS 移植层 | 在 `xPortStartScheduler()` 里写 `SHPR3` 把它设成最低；在 ISR 里只用"挂起 PendSV"代替"直接切换" |

**结论**：硬件**没有**强制 PendSV 必须最低（复位值反而是最高）。这是 RTOS 作者基于三条理由
做出的主动选择：① 把多次切换请求攒成一次 → 减少抖动；② 保证它是中断序列的最后一棒 →
避免尾链期间改 PSP 导致栈帧错乱、也省 RAM；③ 永不抢占任何硬件中断 → 不增加中断延迟。

#### 1.9.3 SysTick 也设成最低，tick 会不会不准？—— 会，但有前提

这是很多人读到源码都会产生的疑问，答案是：**源码确实是这么写的，而且这个设计确实有代价**。

要把两件事分开看：

| | 是否受影响 | 说明 |
|---|---|---|
| **SysTick 硬件计数器的周期** | ✅ 完全不受影响 | 24 位向下计数器挂在 CPU 时钟上，独立于 NVIC 仲裁。它每 N 个周期重装一次，节奏永远准 |
| **SysTick ISR 的执行时刻** | ❌ 会被推迟 | 优先级最低 → 必须等所有更高优先级的 ISR 跑完才轮到它 |
| **`xTickCount` 软件计数** | ❌ 可能被**丢 tick** | `xTickCount` 靠 ISR 里加 1。pending 位是**单个 bit，不累加**：若一个 tick 还没被响应就到了下一次溢出，这次会被**直接丢弃** |

**丢 tick 的判定条件**（本工程 tick = 1 ms）：

```
任何一个 ISR（或关中断的临界区）连续占用 CPU 超过 1 个 tick 周期 → 丢 1 个 tick
```

后果分两级：

| 延迟程度 | 后果 |
|---|---|
| **< 1 个 tick**（常见） | tick 不丢，只是唤醒时刻抖 ±1 tick。这本来就是 FreeRTOS 的既有抖动（`vTaskDelay(1)` 实际睡 1~2 tick） |
| **> 1 个 tick**（ISR 太长 / 临界区太久） | **永久丢 tick**，`xTickCount` 变慢，所有延时和超时同步被拉长 |

**那为什么还要这么设计？** —— 这是 RTOS 的**实时性取舍**：

> 串口接收寄存器满了没读 → **丢字节**（硬错误，不可恢复）
> tick 晚 1 ms 处理 → **定时精度略降**（软降级，可接受）
>
> 所以：让路给外设中断，牺牲 tick 的绝对精度。RTOS 的隐含前提是**你的 ISR 很短**。

**工程对策**（要精确计时时必看）：

1. **ISR 只做三件事**：清标志 → 取数据 → 发通知（`...FromISR`）。绝不在 ISR 里 printf、循环、算浮点。
2. **临界区尽量短**：`taskENTER_CRITICAL()` 期间 tick 是不进的。
3. **不要把 `xTaskGetTickCount()` 当高精度时基**。需要 μs 级时间戳，用**硬件定时器**（输入捕获 / 另一个通用 TIM 的 CNT 寄存器）。
4. **若 tick 精度优先级高于外设实时性**：可以换一个定时器做 tick 源 —— 覆写
   `vPortSetupTimerInterrupt()`（FreeRTOS 里它是弱函数），用 TIM2/TIM5 生成 tick，
   并把该 TIM 的 NVIC 优先级配成你想要的级别。PendSV 仍保持最低，两者不再是同一个轮子上的部件。

> **一句话总结**：`SysTick 硬件周期永远准，但软件 tick 会因为优先级最低而延迟甚至丢失。
> 这是 FreeRTOS 用"定时精度"换"中断实时性"的有意取舍，代价由"ISR 必须短"这条编码纪律来兜。

### 1.10 本工程实战：`app_task_create()`

文件：`Core/Src/FreeRTOS_tasks.c:373`

```c
void app_task_create(void)
{
    BaseType_t ret = pdFAIL;

    /* OTA 任务：优先级 2，栈 256 字(1KB) */
    ret = xTaskCreate(ota_task, "ota_task", 256, NULL, 2, &ota_task_handle);
    if (ret != pdPASS) { printf("ota_task create failed\r\n"); }

    /* 温湿度处理任务：优先级 3，通过参数把"依赖注入"的接口表传进去 */
    ret = xTaskCreate(temp_humi_handler_thread, "temp_humi_task", 256,
                      &temp_humi_input_arg, 3, NULL);

    /* 温湿度采集任务：优先级 1，每 1 秒发一次事件 */
    ret = xTaskCreate(send_temp_humi_event, "send_task", 256, NULL, 1, NULL);
}
```

**这段代码值得学的三点**：

1. **每次创建都判返回值**——好习惯，堆不够时能第一时间发现。
2. **用 `pvParameters` 传依赖**：`&temp_humi_input_arg` 把"IIC 驱动接口、时基接口、
   队列接口、延时接口"整包塞进任务，任务本身不需要 `#include` 具体硬件——这就是**依赖注入**，
   换芯片只改传进来的表。
3. **优先级分层清晰**：采集 1（慢、可被抢占）< OTA 2 < 温湿度处理 3。

再看一个"任务里等队列 + 定时"的完整例子（同文件 `key_led_handle_task`）：

```c
void key_led_handle_task(void *pv)
{
    while (led_queue == NULL) {
        vTaskDelay(100);        /* ← 别人还没建好队列，先让一让（工程上的权宜之计） */
    }

    for (;;)
    {
        key_press_status_t *p_key_value = NULL;
        /* 等按键事件，不阻塞（超时 0），等不到就去干别的 */
        if (xQueueReceive(key_queue, &p_key_value, 0) == pdTRUE) {
            ... 处理按键 ...
        }
        vTaskDelay(10);         /* ← 让出 CPU */
    }
}
```

> 💡 **改进建议**：`while (队列 == NULL) vTaskDelay(100)` 这种"等别人初始化"的写法比较脆弱，
> 更好的做法是在创建任务**之前**把队列建好，用返回值确认后再创建任务。

---

## 第 2 章 队列 Queue —— 任务之间传数据的水管

**一句话认知**：队列 = 一段环形缓冲 + 两条"等待者链表"。
它是 FreeRTOS 的**一等公民**——后面学的信号量、互斥量，其实都是队列的"特化马甲"。

### 2.1 为什么需要队列

两个任务要传数据，最直觉的想法是"用全局变量"。但它有致命问题：

```c
/* 反面教材 */
int g_sensor_value;             // 全局变量

void 生产者(void) { g_sensor_value = 读传感器(); }        // 写一半被打断？
void 消费者(void) { printf("%d", g_sensor_value); }       // 读到的是新值还是旧值？
```

问题：**什么时候数据有效？** 消费者不知道。它只能不停轮询，浪费 CPU；或者读到"写了一半"的值。

队列同时解决了三件事：

1. **传数据**（把数据从 A 拷到 B，安全）；
2. **通知**（"有货了，来取"）；
3. **阻塞等待**（没货就让任务睡，不浪费 CPU）。

### 2.2 三个 API 打天下

```c
/* ① 创建：长度 N，每项 size 字节 */
QueueHandle_t q = xQueueCreate(10, sizeof(uint32_t));

/* ② 发送：从队尾入队。超时 xTicksToWait（0 = 不等待，portMAX_DELAY = 死等） */
xQueueSend(q, &value, pdMS_TO_TICKS(100));         // 等价于 xQueueSendToBack
xQueueSendToFront(q, &value, 0);                   // 插队（紧急消息）

/* ③ 接收 */
if (xQueueReceive(q, &buf, portMAX_DELAY) == pdTRUE) { /* 拿到数据 */ }
```

**命名对照**（读源码时会遇到，别被绕晕）：

| 你写的 | 内核实现 | 说明 |
|---|---|---|
| `xQueueSend()` | `xQueueGenericSend(..., queueSEND_TO_BACK)` | 排到队尾 |
| `xQueueSendToFront()` | `xQueueGenericSend(..., queueSEND_TO_FRONT)` | 插到队头 |
| `xQueueOverwrite()` | `xQueueGenericSend(..., queueOVERWRITE)` | 长度必须 1，满了就覆盖 |

> 本工程的代码（含 OSAL 层 `System/OSAL/osal_freertos.c`）统一用 `xQueueSend` / `xQueueReceive` 这一族。

### 2.3 队列里面长什么样（环形缓冲 + 按值拷贝）

```
存储区 = uxLength 个槽 + 1 字节尾标记

pcHead ──→ ┌────┬────┬────┬────┬────┬────┬────┬────┬────┐
           │ D1 │ D2 │ D3 │    │    │    │    │    │ ★  │ ← ★ 只是边界标记，不存数据
           └────┴────┴────┴────┴────┴────┴────┴────┴────┘
             ↑              ↑                        ↑
        pcReadFrom      pcWriteTo                  pcTail
      (上次读的位置)   (下次写的位置)
```

- **写**：`memcpy` 到 `pcWriteTo`，指针前进一格，越界回绕到 `pcHead`。
- **读**：指针**先前进一格再 memcpy**（因为 `pcReadFrom` 指的是"上次读的位置"）。
- **满/空**：`uxMessagesWaiting == uxLength` 就是满，`== 0` 就是空。

#### ⚠️ 最重要的概念：按值拷贝

```c
void 生产者(void)
{
    uint32_t data = 42;
    xQueueSend(q, &data, 0);     /* ← 把 42 拷进队列存储区 */
}   /* data 是局部变量，函数返回就没了 —— 但 42 已经安全躺在队列里了 */
```

**为什么必须拷贝？** 如果队列只存指针，那么发送方的栈变量在函数返回后就失效了，
接收方拿到的是**悬垂指针**——这是 C 语言最经典的坑。FreeRTOS 用拷贝从根上杜绝了它。

**代价与正确姿势**：

- 拷贝 4 字节 ≈ 几条指令，可忽略；
- **但不要往队列里塞大结构体**（比如 200 字节的传感器帧），既慢又占内存。
  正确做法是：**队列里传指针**（指向静态缓冲 / 内存池里的块），自己管理生命周期。

### 2.4 队列的两个"等待链表"——阻塞的本质

```
Queue_t
├── xTasksWaitingToSend      ← 因为"队列满了"而卡住的任务
└── xTasksWaitingToReceive   ← 因为"队列空了"而卡住的任务
```

这两条就是第 0 章说的 `List_t`：任务把自己 TCB 里的 `xEventListItem`
挂上去（按优先级排序），`xStateListItem` 同时挂在延时列表里——标准的"踩两条船"。

**一次 `xQueueSend` 内部发生了什么**（理解这个，后面所有组件都通了）：

```
1. 关中断，先试一次：
   └── 有空位？→ 拷数据 → 检查"等待接收"链表里有没有人在等
                          └── 有 → 唤醒其中优先级最高的那个
                                └── 被唤醒的比当前任务优先级高？→ 标记"该切换了"
2. 没空位 且 超时=0 → 直接返回 errQUEUE_FULL
3. 没空位 且 愿意等 → 挂起调度器 → 把自己挂到"等待发送"链表 → 进延时列表（睡）
4. 被唤醒（有人取走了数据 / 超时）→ 回到第 1 步重试
```

**这就是 FreeRTOS 里"等 X"的通用套路**，后面所有组件（信号量、事件组、定时器）都是它的变体：

> **把自己挂到某个"事件源"的等待链表上 → 进延时列表睡觉 → 条件满足时被唤醒 → 重试。**

**唤醒顺序**：等待链表按优先级排序，所以**永远是优先级最高的等待者先被唤醒**。

### 2.5 中断里用队列（黄金法则）

中断里**绝对不能阻塞**，所以中断里只能用专门的 `FromISR` 版本：

```c
void USART1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;   /* ① 必须是局部变量，初始化 pdFALSE */

    清中断标志();
    uint8_t ch = USART_ReceiveData(USART1);

    /* ② 用 FromISR 版本，第三个参数是指针 */
    xQueueSendFromISR(g_uart_queue, &ch, &xHigherPriorityTaskWoken);

    /* ③ 中断出口处，交给它决定是否切换 */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

**三步不能少**：

1. `xHigherPriorityTaskWoken` 初始化为 `pdFALSE`；
2. 调用 `...FromISR` API，把它的地址传进去；
3. **中断最后调 `portYIELD_FROM_ISR(xHigherPriorityTaskWoken)`**。

> **为什么必须第 3 步？** ISR 版本的 API 不会自己切换任务，它只通过
> `xHigherPriorityTaskWoken` 告诉你"有个更高优先级的任务被我唤醒了"。
> 你在中断出口置位 PendSV，真正的切换会等所有中断嵌套退出后才发生——
> 这样才不会在中断进行中破坏栈帧。
>
> **省掉第 3 步的后果**：任务不会被立刻唤醒，要等到下一个 tick（1 ms 后）才切换。
> 看似没事，实际上把实时性从"微秒级"降到了"毫秒级"。

### 2.6 本工程实战：按键 → LED 的两级队列

```
   [按键任务]                [按键处理任务]              [LED 任务]
        │                          │                        │
        │ xQueueSend(key_queue)    │                        │
        │ ───────────────────────→ │ xQueueReceive          │
        │                          │ 判断长按/短按           │
        │                          │ xQueueSend(led_queue)  │
        │                          │ ─────────────────────→ │ xQueueReceive
        │                          │                        │ 执行闪烁
```

对应代码（`Core/Src/FreeRTOS_tasks.c:18`）：

```c
void key_led_handle_task(void *pv)
{
    static led_operation_t led_operation = LED_INITED_VALUE;
    key_press_status_t *p_key_value = NULL;

    for (;;)
    {
        if (xQueueReceive(key_queue, &p_key_value, 0) == pdTRUE) {
            led_operation_t *p_led_operation = &led_operation;
            if (*p_key_value == KEY_SHORT_PRESSED) {
                led_operation = LED_BLINK_1_TIMES;
                if (xQueueSendToBack(led_queue, &p_led_operation, 0) != pdTRUE) {
                    printf("send to led_queue failed\r\n");
                }
            }
            ...
        }
        vTaskDelay(10);
    }
}
```

**这个例子值得注意的细节**：

- 队列里传的是**指针**（`led_operation_t *`），不是大结构体——符合 2.3 的建议；
- 但要注意：它指向的是 `static` 变量，生命周期够长，**如果指向栈上的局部变量就会出事**；
- 发送超时填 0（不等待），并**判了返回值**，队列满时打印而不是死等——健壮。

---

## 第 3 章 信号量 Semaphore —— 只发信号，不传数据

**一句话认知**：信号量 = **不搬数据的队列**。它只有一个"计数"，用来表示"发生了几次"或
"还剩几个资源"。

### 3.1 先看穿它的底裤：信号量就是队列的马甲

```c
typedef QueueHandle_t SemaphoreHandle_t;   /* 句柄类型完全一样！ */

#define xSemaphoreGive(sem)   xQueueGenericSend(sem, NULL, 0, queueSEND_TO_BACK)
#define xSemaphoreTake(sem, t) xQueueSemaphoreTake(sem, t)
```

| | 普通队列 | 二值信号量 | 计数信号量 | 互斥量 |
|---|---|---|---|---|
| `uxItemSize` | > 0 | **0** | **0** | **0** |
| `uxLength` | N | **1** | **max** | **1** |
| 计数值含义 | 消息条数 | 0 或 1 | 0 ~ max | 0 或 1 |

**记忆口诀**：

> `xSemaphoreGive()` ≡ 往一个"长度为 N、每项 0 字节"的队列里写一条空消息；
> `xSemaphoreTake()` ≡ 从里面取一条空消息（只把计数减 1，不拷数据）。

因为不拷数据，信号量**只占一个队列结构体（约 80 字节）**，非常省内存。

### 3.2 二值信号量（Binary Semaphore）

**用途：同步（signaling）**——"我干完了，通知你"。计数只能是 0 或 1。

```c
SemaphoreHandle_t g_dma_done_sem;

void 初始化(void)
{
    g_dma_done_sem = xSemaphoreCreateBinary();   /* ⚠️ 初始计数 = 0！必须先 give 才能 take */
}

/* DMA 传输完成中断（NVIC 优先级数值必须 ≥ 5） */
void DMA1_Stream5_IRQHandler(void)
{
    BaseType_t hpw = pdFALSE;
    清中断标志();
    xSemaphoreGiveFromISR(g_dma_done_sem, &hpw);   /* 计数 0 → 1，同时唤醒等待的任务 */
    portYIELD_FROM_ISR(hpw);
}

/* 等待事件的任务 */
void 处理任务(void *p)
{
    for (;;)
    {
        if (xSemaphoreTake(g_dma_done_sem, portMAX_DELAY) == pdTRUE)
        {
            处理数据();      /* 注意：不用把信号量 give 回去！ */
        }
    }
}
```

**二值信号量的三个坑（必看）**：

| 坑 | 说明 |
|---|---|
| **① 事件会丢** | 只有 0/1。中断连发 3 次、任务还没来得及取，后两次直接返回 `errQUEUE_FULL`。要计数请用**计数信号量**。 |
| **② 不能当锁用** | 它**没有优先级继承**（见第 4 章），拿它保护资源会出优先级反转。 |
| **③ 初值陷阱** | `xSemaphoreCreateBinary()` 初值 **0**（必须先 give）；老的 `vSemaphoreCreateBinary()` 初值 **1**（已废弃，别用）。 |

### 3.3 计数信号量（Counting Semaphore）

`uxLength = max`。两种经典用法，区别在于**初值**：

| 用法 | 初值 | give 的人 | take 的人 | 计数含义 |
|---|---|---|---|---|
| **事件计数** | **0** | 事件产生方（通常是中断） | 事件处理任务 | 已发生但没处理的事件数 |
| **资源管理** | **max** | 资源释放方 | 资源申请方 | 还剩几个可用资源 |

```c
/* 用法 1：事件计数 —— 想知道"漏了多少次按键" */
SemaphoreHandle_t xEventCount = xSemaphoreCreateCounting(10, 0);

void EXTI_IRQHandler(void)
{
    BaseType_t hpw = pdFALSE;
    xSemaphoreGiveFromISR(xEventCount, &hpw);   /* 计数 +1，到 10 封顶 */
    portYIELD_FROM_ISR(hpw);
}

/* 用法 2：资源池 —— 比如 3 个 DMA 缓冲区 */
SemaphoreHandle_t xBufPool = xSemaphoreCreateCounting(3, 3);

void 用一个缓冲区(void)
{
    if (xSemaphoreTake(xBufPool, pdMS_TO_TICKS(100)) == pdTRUE) {  /* 计数 3 → 2 */
        使用缓冲区();
        xSemaphoreGive(xBufPool);                                  /* 用完归还 2 → 3 */
    }
}
```

> 本工程 `configUSE_COUNTING_SEMAPHORES = 1`，已开启。

### 3.4 计数信号量的语义边界：它只数数，不区分事件 ★

**这是初学者最容易想错的一点**：计数信号量内部**只有一个数字**（`uxMessagesWaiting`），
它**不携带任何"这是什么事件"的信息**。所以：

> **前提**：give 到同一个计数信号量的所有事件，必须是**同质的**——
> 也就是"消费者拿到一个计数后要做的事，与是哪个 give 无关"。
> 如果不同，这个计数就没有意义，因为取走一个计数没人知道该处理谁。

#### 反例：两类事件共用一个计数信号量

```c
/* ❌ 错误：按键和串口挤进同一个信号量 */
SemaphoreHandle_t bad = xSemaphoreCreateCounting(10, 0);

void EXTI_IRQHandler(void)      { ...; xSemaphoreGiveFromISR(bad, &hpw); }  /* 按键事件 */
void USART1_IRQHandler(void)    { ...; xSemaphoreGiveFromISR(bad, &hpw); }  /* 串口事件 */

void 处理任务(void *p)
{
    for (;;)
    {
        xSemaphoreTake(bad, portMAX_DELAY);
        /* 现在 count 减了 1，但请问：这次该处理按键还是串口？
           答案是：不知道。信息在 give 的那一刻就丢了。 */
    }
}
```

#### 正例：要么同质，要么分开

```c
/* ✅ 方案一：一类事件一个计数信号量（最直观） */
SemaphoreHandle_t key_cnt = xSemaphoreCreateCounting(10, 0);   /* 按键事件计数 */
SemaphoreHandle_t uart_cnt = xSemaphoreCreateCounting(64, 0);  /* 串口事件计数 */

/* ✅ 方案二：本来就同质——"有多少个字节到了"，处理动作完全一样 */
SemaphoreHandle_t rx_bytes = xSemaphoreCreateCounting(256, 0);
/* 中断每次收一个字节就 +1；任务 take 到 N 就去取 N 个字节。谁给的？无所谓。 */
```

#### 判据（一句话自检）

> **"我拿到一个计数时，需要知道它是谁给的吗？"**
> - 不需要 → 计数信号量可用
> - 需要 → 换机制

#### 想区分事件时，四个替代方案

| 方案 | 怎么区分事件 | 代价 |
|---|---|---|
| **各配一个计数信号量** | 不同对象天然隔离 | 每个多占 ~80 字节 |
| **队列**（`xQueueSend`） | **数据本身携带类型/内容** | 有 memcpy 开销 |
| **任务通知位**（`eSetBits`） | 最多 32 个位标志，**单播** | 只能一个接收方 |
| **事件组位** | 24 个位，**可广播**（多任务同时等） | ISR 里置位要走守护任务 |

对照本指南 §8.1 的选型表：**"能传数据"是队列与信号量的分水岭**，而"能区分事件"
其实也是同一条分水岭——因为**区分事件的信息必须由数据携带**，而计数信号量恰恰不搬数据。

#### 进阶坑：资源池用法也**不完整**

计数信号量做"资源池"（初值 = N）时，它只告诉你**"还剩几个可用"**，
**不告诉你"该用哪一个"**：

```c
/* 3 个同构缓冲区的池子 */
SemaphoreHandle_t pool = xSemaphoreCreateCounting(3, 3);
buffer_t *bufs[3] = { &b0, &b1, &b2 };

xSemaphoreTake(pool, portMAX_DELAY);   /* 计数 3 → 2 */
/* 现在要用哪个 buffer？信号量没说，得自己再维护一个"空闲索引"——
   而那个索引本身又需要同步，于是问题原封不动地回来了 */
```

**更完整的做法**：把"资源句柄"本身装进队列，用一个"N 项队列"当资源池：

```c
QueueHandle_t pool = xQueueCreate(3, sizeof(buffer_t *));
xQueueSend(pool, &pb0, 0);   /* 初始化时把 3 个句柄入队 */
xQueueSend(pool, &pb1, 0);
xQueueSend(pool, &pb2, 0);

/* 申请资源 = 出队；归还 = 入队 */
buffer_t *pb;
xQueueReceive(pool, &pb, portMAX_DELAY);   /* 拿到一个确定的缓冲区 ★ */
... 使用 pb ...
xQueueSend(pool, &pb, 0);                  /* 用完还回去 */
```

**结论**：计数信号量适合"只关心数量"的场合。一旦你需要**知道是哪一个**，
就该换成队列——**队列既能传计数，又能传身份**。

### 3.5 信号量 API 一览

| 场景 | 任务中 | 中断中 |
|---|---|---|
| 获取（计数 -1） | `xSemaphoreTake(s, ticks)` | `xSemaphoreTakeFromISR(s, &hpw)` |
| 释放（计数 +1） | `xSemaphoreGive(s)` | `xSemaphoreGiveFromISR(s, &hpw)` |
| 创建二值 | `xSemaphoreCreateBinary()` | — |
| 创建计数 | `xSemaphoreCreateCounting(max, init)` | — |
| 查询计数 | `uxSemaphoreGetCount(s)` | — |
| 删除 | `vSemaphoreDelete(s)` | — |

> ⚠️ 创建失败会返回 `NULL`，**必须判空**。本工程 `configSUPPORT_STATIC_ALLOCATION = 0`，
> 所以 `xSemaphoreCreateBinaryStatic()` 等静态创建 API **全部没编译**，不能用。

---

## 第 4 章 互斥量与临界区 —— 保护共享资源

**一句话认知**：**互斥量是带"所有权"和"优先级继承"的锁**；临界区是更粗暴但更快的一招——
**直接关中断**。

### 4.1 什么是"竞争"，为什么需要保护

```c
/* 两个任务都调用这个函数，共享一条 I2C 总线 */
uint8_t iic_read(uint8_t addr)
{
    IIC_Start();            /* A 任务执行到这里 */
    IIC_SendByte(addr);     /* ← B 任务插进来也发了一个 Start！总线乱套 */
    IIC_Stop();
}
```

问题根源：**一段"必须连续做完、中途不能被打断"的代码**（临界区）。只要涉及共享的
外设、全局变量、缓冲区，都要保护。

FreeRTOS 给你三种强度的武器：

| 武器 | 做法 | 速度 | 适用 |
|---|---|---|---|
| **关中断 / 关调度** | `taskENTER_CRITICAL()` / `vTaskSuspendAll()` | 最快 | 极短的几行赋值 |
| **互斥量** | `xSemaphoreTake/Give` | 中等 | 保护外设、较长临界区（推荐） |
| **递归互斥量** | `...Recursive` | 中等 | 同一任务可能嵌套获取 |

### 4.2 关中断与关调度（最快，但别滥用）

```c
/* A. 临界区：关中断（BASEPRI 掩蔽），可嵌套 */
taskENTER_CRITICAL();
{
    g_counter++;                    /* 只做几件事，做完马上出去 */
}
taskEXIT_CRITICAL();

/* 中断里要用这一对（返回旧状态，成对恢复） */
UBaseType_t uxSaved = taskENTER_CRITICAL_FROM_ISR();
g_flag = 1;
taskEXIT_CRITICAL_FROM_ISR(uxSaved);

/* B. 挂起调度器：只禁止任务切换，中断照常响应 */
vTaskSuspendAll();
{
    ... 操作一组合链表 ...
}
xTaskResumeAll();
```

**铁律**：

- 临界区里**绝对不能调用任何可能阻塞的 API**（`vTaskDelay`、`xQueueReceive` 带超时、
  `xSemaphoreTake` …）。因为中断被掩蔽了、任务也不能切换，等的人永远不可能释放 → **死锁**。
- 临界区要**尽可能短**。长时间关中断会丢中断、毁掉实时性。

> 💡 **BASEPRI 小知识**：Cortex-M 移植不用 `CPSID I` 关总中断，而是把 `BASEPRI` 寄存器
> 设成 `configMAX_SYSCALL_INTERRUPT_PRIORITY`（本工程 = 5）。
> 效果是**掩蔽"优先级数值 ≥ 5"的中断**，而数值 0~4 的高优先级中断照常响应。
> 这叫"部分关中断"，比全关温柔得多。

### 4.3 互斥量（Mutex）：正确的加锁姿势

```c
SemaphoreHandle_t g_iic_bus_mutex;

void bsp_init(void)
{
    g_iic_bus_mutex = xSemaphoreCreateMutex();   /* 初值 = 1（可直接 take） */
    configASSERT(g_iic_bus_mutex != NULL);
}

uint8_t iic_bus_read(uint8_t addr, uint8_t *buf, uint16_t len)
{
    /* ① 带超时获取，绝不裸写 portMAX_DELAY */
    if (xSemaphoreTake(g_iic_bus_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ERROR_BUSY;
    }

    /* ② 临界操作 */
    uint8_t ret = iic_transfer(addr, buf, len);

    /* ③ 所有出口都必须 give！ */
    xSemaphoreGive(g_iic_bus_mutex);
    return ret;
}
```

**互斥量 vs 二值信号量**（这张表要背下来）：

| | 二值信号量 | 互斥量 |
|---|---|---|
| 设计目的 | **同步**（我做完通知你） | **互斥**（这资源只有我能碰） |
| 所有权 | 无，谁都能 give | **有**，内核记录 `xMutexHolder` |
| 谁 give | 任意任务 / 中断 | **必须持有者自己** |
| 优先级继承 | **无** | **有** |
| 初值 | **0**（必须先 give） | **1**（创建即可 take） |
| ISR 中可用 | 可以（`...FromISR`） | **不行** |
| 递归获取 | 不行（自死锁） | 不行（要用递归互斥量） |

### 4.4 优先级反转与"插队"（互斥量最值钱的部分）

#### 先看问题：什么叫优先级反转

三个任务：

- **H**：优先级 3（最高），要用共享资源
- **M**：优先级 2（中等），纯计算，跟资源无关
- **L**：优先级 1（最低），正拿着锁在用资源

```
时刻 1：L 拿到锁，正在用资源
时刻 2：H 就绪 → 抢占 L → 开始跑
时刻 3：H 也要用资源 → 拿锁失败 → 阻塞（等 L 释放）
时刻 4：L 恢复运行（优先级 1）
时刻 5：M 就绪（优先级 2）→ 抢占 L → 一直跑
        ⚠️ M 跟这把锁毫无关系，却把 L 挤到一边
时刻 6：H（优先级 3）只能眼睁睁等 M 跑完 + L 释放锁
```

**结果**：优先级最高的 H，反而被一个中等优先级的 M 间接拖死。这就是**优先级反转**。

#### FreeRTOS 的解法：优先级继承

当高优先级任务 H 阻塞在一把互斥量上时，内核会**临时把持有者 L 的优先级抬到 H 的高度**：

```
幕一：H 拿锁失败 → 阻塞
幕二：内核执行 xTaskPriorityInherit()
        L 的优先级：1 ──→ 3（临时抬高，uxBasePriority 仍是 1）
        L 从"优先级1就绪链表"摘下 → 插入"优先级3就绪链表"
幕三：M（优先级2）想抢占？抢不过了 —— L 现在"插队"到 M 前面
        L 以高优先级快速跑完临界区
幕四：L 释放锁 → xTaskPriorityDisinherit()
        L 的优先级：3 ──→ 1（恢复基础优先级）
        H 被唤醒，拿到锁，继续跑
```

**效果对比**：

```
有继承：L(3)跑完 → 释放 → H(3)跑 → M(2)最后跑     ✅ 高优先级没被饿死
无继承：M(2)插队跑 → H(3)干等                      ❌ 优先级反转
```

**局限**（要知道）：优先级继承只能**缩短**反转持续时间，不能彻底消除——L 用资源的那段时间
H 还是得等。另外，如果 H **等待超时放弃了**，L 的优先级不能直接降回 1（可能还有别的等待者），
内核会降为 `max(基础优先级, 剩余等待者最高优先级)`。

### 4.5 递归互斥量

**问题**：函数 A 拿了锁，内部又调用了也需要这把锁的函数 B → **自死锁**。

**解法**：用递归互斥量，它记一个"重复获取次数"：

```c
SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();

void 外层函数(void)
{
    xSemaphoreTakeRecursive(m, portMAX_DELAY);   /* 计数 0 → 1 */
    内层函数();
    xSemaphoreGiveRecursive(m);                  /* 计数 1 → 0，真正释放 */
}

void 内层函数(void)
{
    xSemaphoreTakeRecursive(m, portMAX_DELAY);   /* 同一个任务，计数 1 → 2，直接过 */
    ... 干活 ...
    xSemaphoreGiveRecursive(m);                  /* 计数 2 → 1 */
}
```

> 本工程 `configUSE_RECURSIVE_MUTEXES = 1`，可用。
> 典型场景：W25Q64 外部 Flash 的 `write_page()` 被 `write_multi()` 嵌套调用。

### 4.6 互斥量使用清单（照着检查）

- [ ] 用的是 `xSemaphoreCreateMutex()`（不是二值信号量）；
- [ ] 超时用 `pdMS_TO_TICKS(10~100)`，**不要**裸写 `portMAX_DELAY`（持有者出事就永久挂死）；
- [ ] **判了返回值**，拿不到锁就报错返回，不能"假装拿到了"进临界区；
- [ ] **所有 return 路径都会 give**（建议用 `do { } while(0)` 收口）；
- [ ] **不在中断里用**（ISR 没有 TCB，不能当持有者，内核有断言拦着）；
- [ ] 不嵌套获取同一把普通锁（要嵌套就用递归互斥量）；
- [ ] 不在临界区（`taskENTER_CRITICAL`）里 take 锁 → 死锁。

> 🚨 **本工程已发现的问题**（`APP/OTA/ota.c:62`，代码目前被注释）：
> `xSemaphoreTake(Semaphore_ExtFlashState, 0)` 超时写 0，获取失败也不判断，
> 后面的 `W25Q64_WriteData()` **照样执行，锁等于没加**。以后启用请改成带超时 + 判返回值。

---

# 第二部分 进阶篇：三个"外接组件"

> 这三个组件**都不属于队列体系**，各有各的架构思路。学完第一部分再来，会轻松很多。
> 它们解决的都是"队列/信号量不太好使"的场景。

---

## 第 5 章 任务通知 Task Notification —— 最快的轻量信号

**一句话认知**：不创建任何对象，直接往目标任务的 TCB 里写一个 32 位值。
**它是 FreeRTOS 里最快的任务间通信方式。**

### 5.1 它为什么快

| | 二值信号量 | 任务通知 |
|---|---|---|
| 需要创建对象 | 是（一个 `Queue_t`，约 80 字节） | **否**（值就存在 TCB 里） |
| 需要拷贝数据 | 不需要（但走队列流程） | **不需要** |
| 额外 RAM | ~80 字节 | **0 字节** |
| 速度 | 中 | **最快**（官方数据：比信号量快约 45%） |

**代价**：

- 只能**单播**（一个发送方 → 一个指定任务），不能广播、不能"多任务等同一个通知"；
- 发送方**不能阻塞**（ISR 版本可唤醒对方，但发送本身不会等）；
- 接收方只能有一个。

### 5.2 API 与用法

```c
TaskHandle_t g_worker_handle;   /* 必须先拿到目标任务句柄 */

/* ===== 发送方 ===== */
xTaskNotify(g_worker_handle,          /* 目标任务句柄 */
            0x01,                     /* 值 */
            eSetBits,                 /* 动作 */
            NULL);

/* 中断里用 FromISR 版本 */
BaseType_t hpw = pdFALSE;
xTaskNotifyFromISR(g_worker_handle, 0x01, eSetBits, &hpw);
portYIELD_FROM_ISR(hpw);

/* ===== 接收方（等通知） ===== */
uint32_t notified_value;
xTaskNotifyWait(0x00,        /* 进入时不清除任何位 */
                0xFFFFFFFF,  /* 退出时清除所有位 */
                &notified_value,
                portMAX_DELAY);
```

**五种动作（`eAction`）**：

| 动作 | 效果 | 类比 |
|---|---|---|
| `eSetBits` | `value |= 参数` | 事件标志（推荐，可累积） |
| `eIncrement` | `value++` | 相当于"计数信号量" |
| `eSetValueWithOverwrite` | `value = 参数`（直接覆盖） | 传一个值 |
| `eSetValueWithoutOverwrite` | 上次没被取走就失败 | 传值且不丢 |
| `eNoAction` | 值不变，只发个信号 | 最轻的"戳一下" |

**两个"简写版"API**（把通知当信号量用时最方便）：

```c
xTaskNotifyGive(handle);            /* ≡ eIncrement，通知值 +1 */
ulTaskNotifyTake(pdTRUE, ticks);    /* 取出通知值；pdTRUE = 取出后清零（当二值信号量） */
                                    /*           pdFALSE = 取出后减一（当计数信号量） */
```

### 5.3 典型场景：ISR 通知任务

```c
/* 中断：只负责"戳一下"任务 */
void DMA2_Stream0_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0)) {
        DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);

        BaseType_t hpw = pdFALSE;
        xTaskNotifyFromISR(g_dma_handle, BIT_DMA, eSetBits, &hpw);
        portYIELD_FROM_ISR(hpw);       /* ← 别忘了这一步 */
    }
}

/* 任务：等通知 */
void dma_handler(void *p)
{
    uint32_t evt;
    for (;;) {
        do {
            xTaskNotifyWait(0x0, BIT_DMA, &evt, portMAX_DELAY);
        } while (0 == (evt & BIT_DMA));    /* 过滤掉其他位 */
        处理数据();
    }
}
```

> **本工程实例**：`Core/Src/FreeRTOS_tasks.c` 的 `dma_handler` / `adc_app` 两个任务，
> 用 `xTaskNotify` + `eSetBits` 实现了一个**双缓冲 + 位标志**的生产者-消费者模型，
> 配合互斥量保证实时性。学完本章可以回去读一遍，是非常好的综合范例。

### 5.4 什么时候该用它

✅ **该用**：

- 中断 → 任务发信号（DMA 完成、数据到达、按键触发）
- 一对一同步，不需要传大数据

❌ **不该用**：

- 一个事件要唤醒**多个**任务（用事件组）
- 要传数据块（用队列）
- 发送方需要阻塞等待（用队列/信号量）

---

## 第 6 章 事件组 Event Group —— 多个条件"任意/全部"组合

**一句话认知**：一个 32 位的"事件位图"。任务可以等待"**任意一个位**置起"或
"**所有位都置起**"，满足条件时**所有等待者一起被唤醒**（广播）。

### 6.1 什么时候需要它

假设一个任务要等三件事都齐了才能干活：

- 用信号量：得创建 3 个信号量，一个个 take，代码又臭又长；
- 用任务通知：只能单播，且位操作要自己管；
- **用事件组**：一次搞定。

```c
#define WIFI_OK    (1 << 0)    /* bit0：Wi-Fi 连上了 */
#define TIME_OK    (1 << 1)    /* bit1：时间同步了 */
#define SENSOR_OK  (1 << 2)    /* bit2：传感器就绪了 */

EventGroupHandle_t g_evt;

void 主任务(void *p)
{
    /* 等三件事全部就绪（pdTRUE = 全部位都要满足） */
    EventBits_t bits = xEventGroupWaitBits(
                           g_evt,
                           WIFI_OK | TIME_OK | SENSOR_OK,  /* 我关心哪些位 */
                           pdTRUE,    /* 满足后是否清除这些位 */
                           pdTRUE,    /* pdTRUE = "全部"；pdFALSE = "任意一个" */
                           portMAX_DELAY);

    if ((bits & (WIFI_OK | TIME_OK | SENSOR_OK)) == (WIFI_OK | TIME_OK | SENSOR_OK)) {
        开始干活();
    }
}

/* 各个模块就绪时置位 */
void wifi_task(void *p)  { ...连上...; xEventGroupSetBits(g_evt, WIFI_OK); }
void time_task(void *p)  { ...同步...; xEventGroupSetBits(g_evt, TIME_OK); }
```

### 6.2 API 一览

```c
EventGroupHandle_t g_evt = xEventGroupCreate();

/* 等待位：超时填 0 就是"只查一下" */
EventBits_t xEventGroupWaitBits(g_evt,
                                uxBitsToWaitFor,   /* 关心哪些位 */
                                xClearOnExit,      /* 满足后是否清位 */
                                xWaitForAllBits,   /* pdTRUE=全部，pdFALSE=任意 */
                                xTicksToWait);

xEventGroupSetBits(g_evt, BIT_0);                  /* 任务里置位 */
xEventGroupSetBitsFromISR(g_evt, BIT_0, &hpw);     /* 中断里置位（受限，见下） */
xEventGroupClearBits(g_evt, BIT_0);                /* 手动清位 */
xEventGroupSync(g_evt, 我置的位, 我等的位, 超时);   /* 同步屏障（rendezvous） */
```

### 6.3 三个必须知道的限制

| 限制 | 说明 |
|---|---|
| **① 只有 24 位可用** | 32 位里**高 8 位被内核占用**做控制位（判断是否"全部满足"、是否"因位置起被唤醒"、是否"退出时清位"）。你只能用 **bit0 ~ bit23**。 |
| **② ISR 里置位是"延迟"的** | 中断不能直接遍历等待链表，`xEventGroupSetBitsFromISR` 实际是把操作**挂到软件定时器守护任务**去执行。所以它**依赖 `configUSE_TIMERS=1`**，且延迟取决于定时器任务的优先级。实时性要求高时别用。 |
| **③ 唤醒是广播** | 多个等待者条件同时满足时，会**一次性全部唤醒** —— 这点和任务通知的"只唤醒一个"截然不同。 |

### 6.4 同步屏障：`xEventGroupSync`

"多个人到齐了才一起出发"：

```c
/* 任务 A */
xEventGroupSync(g_evt, TASK_A_BIT, TASK_A_BIT | TASK_B_BIT | TASK_C_BIT, portMAX_DELAY);
/* 三个任务的位都置起后，三个任务同时被唤醒，且这些位会被自动清除 */
```

---

## 第 7 章 软件定时器 Software Timer —— 会跑回调的"闹钟任务"

**一句话认知**：软件定时器是**用一个队列 + 一个守护任务拼出来的**，
它的**回调函数运行在任务上下文，不是中断里**。

### 7.1 它到底是个什么结构

```
   你的任务 / ISR
        │ xTimerStart / Stop / Reset / ChangePeriod
        ▼
   ┌──────────────┐
   │ xTimerQueue  │  ← 命令队列（"请启动定时器"、"请改周期"）
   └──────┬───────┘
          ▼
   ┌──────────────────────────────────────┐
   │  prvTimerTask（定时器守护任务）        │
   │  for(;;) {                           │
   │      取最近到期时间；                 │
   │      阻塞等待"到期"或"新命令";        │
   │      到期 → 执行回调函数;             │
   │      处理命令队列;                    │
   │  }                                   │
   └──────────────────────────────────────┘
```

**关键点**：所有 `xTimer*` API 都只是**往队列里发一条命令**，真正的增删改由守护任务执行。
所以这些 API **不会阻塞很久**，也不会在调用者的上下文里跑回调。

### 7.2 用法

```c
TimerHandle_t g_led_timer;

void 定时器回调(TimerHandle_t xTimer)     /* 原型固定，无返回值 */
{
    GPIO_ToggleBits(GPIOC, GPIO_Pin_13);
}

void 初始化(void)
{
    g_led_timer = xTimerCreate(
                    "led_t",                  /* 名字（调试用） */
                    pdMS_TO_TICKS(500),       /* 周期：500 ms */
                    pdTRUE,                   /* pdTRUE = 自动重载（周期）；pdFALSE = 单次 */
                    (void *)0,                /* 定时器 ID（可用来区分多个定时器） */
                    定时器回调);

    xTimerStart(g_led_timer, 0);              /* 启动（第二个参数是"命令发送"的超时） */
}

/* 在别处可以随停随开 */
xTimerStop(g_led_timer, 0);
xTimerReset(g_led_timer, 0);                  /* 重新计时（常用于"喂看门狗"式超时检测） */
xTimerChangePeriod(g_led_timer, pdMS_TO_TICKS(1000), 0);
```

中断里用 `xTimerStartFromISR()` / `xTimerResetFromISR()`（同样要配 `portYIELD_FROM_ISR`）。

### 7.3 三条铁律

1. **回调里绝对不能阻塞**。不能 `vTaskDelay`、不能带超时地等队列/信号量。
   因为所有定时器共用一个守护任务，你卡住它，**所有定时器一起停摆**。
2. **回调要短小**。典型做法：回调里只发个信号（任务通知 / 队列），真正的活交给任务。
3. **优先级由 `configTIMER_TASK_PRIORITY` 决定**（本工程 = 2）。它必须足够高才能准时，
   但也不能高到压过关键业务。

> **本工程已启用**：`configUSE_TIMERS = 1`，`configTIMER_TASK_PRIORITY = 2`，
> `configTIMER_QUEUE_LENGTH = 10`（命令队列最多积压 10 条命令，超了会失败）。

---

# 第三部分 实战与内功

---

## 第 8 章 一张表搞定选型：我该用哪个

### 8.1 五大通信/同步机制横向对比

| 维度 | 队列 | 信号量 | 互斥量 | 任务通知 | 事件组 |
|---|---|---|---|---|---|
| 传数据 | ✅ 按值拷贝 | ❌ 只有计数 | ❌ | ✅ 一个 32 位值 | ❌ 只有位 |
| **能否区分"是哪类事件"** | ✅ 靠数据携带 | ❌ **只有个数，无语义** | — | ✅ 最多 32 位 | ✅ 24 位 |
| 创建对象 | 需要 | 需要（~80B） | 需要（~80B） | **不需要（0 B）** | 需要 |
| 广播唤醒 | ❌ 唤醒一个 | ❌ 唤醒一个 | ❌ | ❌ 单播 | ✅ **可广播** |
| 优先级继承 | ❌ | ❌ | ✅ | ❌ | ❌ |
| ISR 可用 | ✅ | ✅ | ❌ | ✅ | ⚠️ 受限（走守护任务） |
| 相对开销 | 高（memcpy） | 中 | 中（+继承开销） | **最低** | 中 |
| 本工程是否已开 | ✅ | ✅ | ✅ | ✅ | ✅ |

### 8.2 选型口诀（背下来就够用）

```
要传数据            → 队列
要保护共享资源      → 互斥量（有优先级继承，防饿死；不是二值信号量！）
要数"发生了几次"    → 计数信号量（前提：同类事件；要区分事件类型请用队列/通知位/事件组）
只是"戳一下"通知    → 任务通知（最快，零额外内存）
多个条件组合        → 事件组（任意 / 全部 / 唤醒后清位）
定时任务 / 周期回调 → 软件定时器（回调里别阻塞）
极短的几行保护      → taskENTER_CRITICAL()（比信号量快得多）
```

### 8.3 本工程场景推荐（落地参考）

| 场景 | 推荐 | 理由 |
|---|---|---|
| MPU6050 DMA 完成 → 处理任务 | **任务通知**最优，次选二值信号量 | 只需"发生了"这个信号。当前代码用队列塞一个常量 `1`，属于"用队列模拟信号量"，可优化 |
| 软件 I2C 总线被多任务访问 | **互斥量** | 需要所有权 + 优先级继承；临界区几百 µs，不适合全程关中断 |
| W25Q64 / 外部 Flash | **互斥量**（必要时递归） | 写 Flash 期间禁止其他任务访问，可能有嵌套调用 |
| 环形缓冲区生产者-消费者 | 单生产单消费可无锁；多生产用互斥量或队列 | 队列自带保护，但要付 memcpy 成本 |
| 按键事件计数（允许知道漏了几次） | **计数信号量** | 二值信号量会丢事件且不计数。⚠️ 仅限**同一类**事件，多类事件要各配一个（见 §3.4） |
| N 个缓冲区 / DMA 通道资源池 | **计数信号量**，初值 = N；要拿到**具体是哪个**则改用装句柄的队列 | 语义贴切，但计数信号量只管"还剩几个"，不管"分配哪一个"（见 §3.4） |
| 极短临界区（几条赋值） | `taskENTER_CRITICAL()` | 比信号量快得多，但内部绝不能阻塞 |

---

## 第 9 章 中断与 RTOS 的相处规则（最容易踩坑）

### 9.1 黄金法则

> **中断里只能调用 `...FromISR` 结尾的 API，且这些 API 绝不阻塞。**

```
❌ 中断里禁止：
   vTaskDelay / vTaskDelayUntil      （中断不能"睡"）
   xQueueSend / xQueueReceive        （要用 ...FromISR 版）
   xSemaphoreTake 带超时
   xSemaphoreGive（互斥量，任何形式）
   xEventGroupSetBits（要用 FromISR 版）

✅ 中断里可以：
   xQueueSendFromISR / xQueueReceiveFromISR
   xSemaphoreGiveFromISR（仅限二值/计数信号量）
   xTaskNotifyFromISR
   xTimerStartFromISR / ResetFromISR / ChangePeriodFromISR
   xEventGroupSetBitsFromISR（延迟执行）
   taskENTER_CRITICAL_FROM_ISR / EXIT_CRITICAL_FROM_ISR
```

### 9.2 中断优先级的硬约束（本工程）

Cortex-M 的中断优先级**数值越小，逻辑优先级越高**。FreeRTOS 约定：

```
configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5   ← 本工程

NVIC 数值 0 ───── 4  ：逻辑优先级高于阈值
    ✅ 中断响应最快
    ❌ 但【绝对不能】调用任何 FreeRTOS API（连 FromISR 都不行）
    ✅ 只能做纯硬件操作（清标志、扔进自己的裸缓冲）

NVIC 数值 5 ───── 15 ：逻辑优先级不高于阈值
    ✅ 可以调用 ...FromISR API
    ✅ 会被临界区（BASEPRI=5）掩蔽
```

**配置方法**（用 HAL/SPL 时）：

```c
NVIC_InitTypeDef nvic;
nvic.NVIC_IRQChannel = DMA2_Stream0_IRQn;
nvic.NVIC_IRQChannelPreemptionPriority = 6;   /* ← 必须 ≥ 5！ */
nvic.NVIC_IRQChannelSubPriority = 0;
nvic.NVIC_IRQChannelCmd = ENABLE;
NVIC_Init(&nvic);
```

**还有一条**：优先级分组必须**全部分配给抢占优先级**（`NVIC_PriorityGroup_4`），
不能留子优先级位，否则 BASEPRI 的掩蔽行为会不符合预期。

> 如果违反了，内核的 `vPortValidateInterruptPriority()` 会触发 `configASSERT`——
> 本工程 `configASSERT` 的定义是 **关中断 + `for(;;)` 死循环**，现象就是"系统卡死不跑"。

### 9.3 ISR 模板（背下来）

```c
void XXX_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;    /* ① 必须初始化 */

    if (中断标志置起) {
        清中断标志();                                  /* ② 先清标志 */

        某个FromISR的API(..., &xHigherPriorityTaskWoken);  /* ③ 传地址 */
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);      /* ④ 出口处，不能少 */
}
```

**为什么不能省第 ④ 步**：ISR API 不会自己切换任务，只通过
`xHigherPriorityTaskWoken` 告诉你"有更高优先级任务被唤醒了"。
你在中断出口置位 PendSV，等所有中断嵌套退出后再统一切换——这样不会破坏栈帧。
省掉它，任务响应会从"微秒级"退化到"下一个 tick（最多 1 ms）"。

---

## 第 10 章 内存、栈与"我的系统还剩多少水"

### 10.1 堆：任务、队列、信号量都从这里分配

FreeRTOS 不用标准库的 `malloc`（不可重入、行为不定），自带 5 种堆策略。
本工程用的是 **`heap_4`**（`OS/port/heap_4.c`），总大小 **75 KB**。

| 策略 | 特点 | 适用 |
|---|---|---|
| heap_1 | 只分配，不释放 | 任务/队列从不删除的极简场景 |
| heap_2 | 可释放，不合并碎片 | 少量、固定大小的分配 |
| **heap_4** ✅ | **空闲链表 + 释放时合并相邻块** | **大多数项目默认，本工程用它** |
| heap_5 | heap_4 + 支持多段不连续内存 | 多个 RAM 区 |

**heap_4 的行为**：

- 分配：遍历空闲链表找第一块够大的（first-fit），剩余够大就**分裂**；
- 释放：标记为空闲，**尝试与前后相邻空闲块合并**（这是它抗碎片的关键）；
- 分配期间会**挂起调度器**，所以 `pvPortMalloc` 不能在中断里直接调。

**常用查询**：

```c
size_t 剩余堆 = xPortGetFreeHeapSize();                    /* 当前剩余 */
size_t 历史最低 = xPortGetMinimumEverFreeHeapSize();       /* 跑过一段时间后的最低水位 ★ */
```

> 💡 **排障技巧**：上电跑几分钟后打印 `xPortGetMinimumEverFreeHeapSize()`，
> 如果它接近 0，说明堆快爆了。留 20% 以上余量比较安全。

### 10.2 栈：每个任务一份，溢出就死给你看

**栈里装什么**：局部变量、函数调用嵌套（每层几个到几十字节）、
以及**寄存器现场**（Cortex-M 上约 64~100 字节，用 FPU 时更多）。

**怎么定大小**：

1. 起步给 `256`（= 1 KB），跑起来再量；
2. 用 API 查水位：

```c
UBaseType_t 剩余 = uxTaskGetStackHighWaterMark(NULL);   /* NULL = 查自己 */
printf("栈最低水位剩余 %lu 字（%lu 字节）\n", 剩余, 剩余 * 4);
```

> `uxTaskGetStackHighWaterMark` 返回的是"任务运行以来**最小的剩余栈量**"。
> 剩得越多越安全。**建议留 30% 以上余量**（因为中断嵌套会额外吃栈）。

**栈溢出的典型现象**：

- 系统莫名其妙进 HardFault；
- 某个变量的值"自己变了"；
- 打印输出乱码。

> ⚠️ **本工程 `configCHECK_FOR_STACK_OVERFLOW = 0`**（`FreeRTOSConfig.h:70`），
> **栈溢出检测是关掉的**！强烈建议开发阶段改成 **2**（最严格的检测方式），
> 并配上 `vApplicationStackOverflowHook()` 打印任务名，能省下大量调试时间。
> 发布前再改回 0 以省开销。

### 10.3 静态 vs 动态创建

本工程 `configSUPPORT_DYNAMIC_ALLOCATION = 1`，`configSUPPORT_STATIC_ALLOCATION = 0`。

- **动态创建**（`xTaskCreate`、`xQueueCreate`）：从 FreeRTOS 堆里分配，**可能失败，必须判空**；
- **静态创建**（`xTaskCreateStatic` 等）：**本工程没编译**，用不了。
  若未来要做功能安全（禁止动态内存），需先把宏置 1 并实现
  `vApplicationGetIdleTaskMemory()` / `vApplicationGetTimerTaskMemory()`。

---

## 第 11 章 FreeRTOSConfig.h 配置速览

文件：`OS/FreeRTOSConfig.h`。这是整个 RTOS 的"总开关"，改一个宏可能裁掉一整棵依赖树。

### 11.1 本工程的实际值

| 配置项 | 本工程值 | 含义 / 改了会怎样 |
|---|---|---|
| `configUSE_PREEMPTION` | 1 | 抢占式调度。**设 0 变协作式**，只有任务主动让出才切换 |
| `configTICK_RATE_HZ` | **1000** | 1 ms 一个 tick。调高 = 更精确但开销大；调低 = 省电但精度差 |
| `configMAX_PRIORITIES` | **5** | 优先级 0~4。**你的任务优先级不能 ≥ 5** |
| `configTOTAL_HEAP_SIZE` | 75 KB | 所有任务/队列/信号量的内存来源 |
| `configMINIMAL_STACK_SIZE` | 130 字 | 空闲任务的栈 |
| `configMAX_TASK_NAME_LEN` | **10** | 任务名最长 10 字符（超了会被截断） |
| `configUSE_MUTEXES` | 1 | 互斥量可用 |
| `configUSE_RECURSIVE_MUTEXES` | 1 | 递归互斥量可用 |
| `configUSE_COUNTING_SEMAPHORES` | 1 | 计数信号量可用 |
| `configUSE_TIMERS` | 1 | 软件定时器可用（关掉则守护任务不创建） |
| `configTIMER_TASK_PRIORITY` | 2 | 定时器守护任务优先级 |
| `configTIMER_QUEUE_LENGTH` | 10 | 定时器命令队列长度 |
| `configIDLE_SHOULD_YIELD` | 1 | 空闲任务遇到同优先级就绪任务时立即让出 |
| `configCHECK_FOR_STACK_OVERFLOW` | **0** ⚠️ | **栈溢出检测关闭，建议开发期改 2** |
| `configUSE_MALLOC_FAILED_HOOK` | **0** ⚠️ | 分配失败不告警，靠你自己判空 |
| `configUSE_TRACE_FACILITY` | 1 | 支持 `vTaskList` 等调试 API |
| `configGENERATE_RUN_TIME_STATS` | 0 | CPU 占用率统计（打开可看每个任务占多少 CPU） |
| `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` | **5** | NVIC 数值 ≥5 的中断才能调 FromISR API |
| `configSUPPORT_DYNAMIC_ALLOCATION` | 1 | 动态创建可用；静态创建**不可用** |

### 11.2 裁剪逻辑（理解"条件编译裁掉整棵依赖树"）

| 关掉这个 | 会发生什么 |
|---|---|
| `configUSE_TIMERS = 0` | 定时器守护任务不创建，`xTimer*` API 消失；事件组 FromISR 也跟着失效 |
| `configUSE_MUTEXES = 0` | 互斥量 API 消失，优先级继承代码不编译 |
| `configUSE_TASK_NOTIFICATIONS = 0` | TCB 不再含通知字段，通知 API 消失（**默认就是 1**） |
| `configMAX_PRIORITIES` 改小 | 就绪链表数组变小，省 RAM；但优先级分级变少 |

### 11.3 两个超好用的调试 API

```c
/* ① 打印任务列表（需要 configUSE_TRACE_FACILITY = 1，本工程已开） */
char buf[512];
vTaskList(buf);
printf("任务名      状态  优先级  剩余栈  任务号\n%s\n", buf);
/* 状态字符：R=运行  B=阻塞  S=挂起  D=删除  X=就绪 */

/* ② 运行时统计（需要把 configGENERATE_RUN_TIME_STATS 置 1 + 提供高分辨率时基） */
vTaskGetRunTimeStats(buf);   /* 每个任务占用 CPU 的百分比 ★ 找性能瓶颈神器 */
```

---

## 第 12 章 新手十大典型错误与排查清单

| # | 错误 | 现象 | 正确做法 |
|---|---|---|---|
| 1 | 任务函数 `return` 了 | 进断言死循环 | 永远 `for(;;)`，要退出用 `vTaskDelete(NULL)` |
| 2 | 任务循环里没有阻塞 | 低优先级任务饿死，系统像卡住 | 至少加 `vTaskDelay(1)` 或等队列 |
| 3 | 优先级传了 ≥ 5 | 断言失败 | 本工程只有 0~4 |
| 4 | 栈给太小 | 莫名 HardFault、变量乱变 | 起步 256，用 `uxTaskGetStackHighWaterMark` 量 |
| 5 | 中断里用了非 FromISR API | 断言死循环 | 只用 `...FromISR` |
| 6 | 中断忘了 `portYIELD_FROM_ISR` | 响应慢一个 tick | 中断出口补上 |
| 7 | 中断优先级数值 < 5 | 调用 API 时断言死机 | NVIC 抢占优先级设 ≥ 5 |
| 8 | 拿二值信号量当锁 | 优先级反转，高优先级任务被拖死 | 保护资源一律用互斥量 |
| 9 | `xSemaphoreTake(..., 0)` 不判返回值 | 锁没拿到却进了临界区 | 带超时 + 判返回值 |
| 10 | 临界区里调阻塞 API | 系统死锁 | 临界区内只做不阻塞的事 |

**更多常见坑（进阶）**：

- **中断连发二值信号量** → 后几次返回 `errQUEUE_FULL`，**事件静默丢失**。要计数用计数信号量。
- **递归获取普通互斥量** → 自死锁。改递归互斥量或重构掉嵌套。
- **在 ISR 里用互斥量** → 断言死循环（ISR 没有 TCB，不能当持有者）。
- **调度器启动前调 take/give** → `xTaskGetSchedulerState()` 断言。**创建**可以，take/give 放任务里。
- **`vTaskDelete` 后内存没回收** → 空闲任务被饿死了。别让所有任务都长期阻塞在高优先级。
- **删除仍被持有的锁** → 等待者永久阻塞。删之前确认无人持有、无人等待。

### 排障三步走

```
第一步：看任务状态
    → vTaskList() 打印。所有任务都是 B（阻塞）？是不是没人唤醒它们？
      某个任务是 R 且优先级最高？是不是它死循环不阻塞？

第二步：看内存
    → xPortGetMinimumEverFreeHeapSize()：堆爆了？
    → uxTaskGetStackHighWaterMark()：栈快没了？
    → 临时把 configCHECK_FOR_STACK_OVERFLOW 改成 2 试试

第三步：看中断
    → NVIC 优先级数值是不是 ≥ 5？
    → 是不是忘了清中断标志（中断会一直重入，系统卡死）？
```

---

## 第 13 章 动手实验清单（照做就能"看见"机制）

### 实验 1：时间片轮转（1 小时）

创建两个**同优先级**任务，各自 `printf` 自己的名字 + `vTaskDelay(200)`。

**预期**：打印交替出现。这就是链表游标 `pxIndex` 在同优先级任务间轮转的效果。

### 实验 2：抢占（30 分钟）

创建 A（优先级 1，死循环打印）和 B（优先级 3，每 500 ms 打印一次）。

**预期**：A 会被 B 打断。体会"高优先级任务一就绪就立刻抢 CPU"。

### 实验 3：`vTaskDelay` 漂移 vs `xTaskDelayUntil`（1 小时）

```c
/* 周期任务里统计 100 个周期的实际耗时 */
TickType_t last = xTaskGetTickCount();
for (int i = 0; i < 100; i++) {
    干活();
    vTaskDelay(10);          /* 换成 xTaskDelayUntil(&last, 10) 再测一次 */
}
```

**预期**：`vTaskDelay` 的抖动会**累积**；`xTaskDelayUntil` 抖动恒定 ≤ 1 tick。

### 实验 4：优先级继承（2 小时，最有价值）

```c
/* L（优先级1）拿锁后打印自己的优先级 */
L: xSemaphoreTake(m, portMAX_DELAY);
   printf("L 拿锁，优先级 = %lu\n", uxTaskPriorityGet(NULL));   /* 应显示 1 */
   vTaskDelay(500);   /* 故意占着锁不放 */

/* H（优先级3）延迟 100ms 后尝试拿锁 */
H: vTaskDelay(100);
   printf("H 尝试拿锁...\n");
   xSemaphoreTake(m, portMAX_DELAY);
   printf("H 拿到锁了\n");

/* 在 L 释放锁前再打印一次优先级 */
/* 预期：H 阻塞后，L 的优先级被临时抬到 3 */
```

**观察**：L 拿锁后 `uxTaskPriorityGet(NULL)` 从 1 变成 3，释放后恢复 1。这就是优先级继承。

### 实验 5：中断 → 任务通知（1 小时）

用一个定时器中断，每 1 秒 `xTaskNotifyFromISR` 一次；任务里 `ulTaskNotifyTake` 计数。

**对比实验**：把 `portYIELD_FROM_ISR` 注释掉，观察任务响应是否延迟。

### 实验 6：栈与堆水位（30 分钟）

```c
void 监控任务(void *p)
{
    for (;;) {
        printf("剩余堆=%u, 历史最低=%u\n",
               xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        printf("本任务栈水位=%u 字\n", uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(5000);
    }
}
```

跑 10 分钟后看数值，判断余量是否充足。

---

# 附录

## 附录 A 概念与 API 速查表

### A.1 概念速查

| 概念 | 一句话 |
|---|---|
| **`ListItem_t`** | 链表节点（钩子）。5 个字段：值 / 前后指针 / `pvOwner`(主人) / `pxContainer`(所属链表)。见 §0.2 |
| **`List_t`** | 链表头。3 个字段：节点数 / `pxIndex`(轮转游标) / `xListEnd`(哨兵)。见 §0.3 |
| **哨兵 `xListEnd`** | 永远存在的标记节点，值为最大。让空链表也有边界，插入删除免特判。见 §0.3 |
| **`pxIndex` 游标** | 停在"上次选中的节点"上，每次取它的下一个 → 时间片轮转。见 §0.5 |
| **TCB** | 任务控制块，任务的身份证。含栈顶、两条链表项、优先级、通知值 |
| **就绪链表** | 每个优先级一条链表，存放所有就绪任务 |
| **延时列表** | 按"唤醒时刻"升序排列的阻塞任务链表（闹钟架） |
| **事件列表** | 挂在某个"事件源"（队列/事件组）上的等待者链表，按优先级排序 |
| **tick** | 系统心跳，本工程 1 ms。`pdMS_TO_TICKS(n)` 把毫秒换算成 tick |
| **PendSV** | 真正执行上下文切换的异常，优先级设为最低 |
| **BASEPRI** | 掩蔽"优先级数值 ≥ 阈值"的中断，FreeRTOS 的临界区机制 |
| **优先级继承** | 高优先级任务等锁时，临时抬高锁持有者的优先级，防饿死 |
| **空闲任务** | 优先级 0，负责回收被删任务的内存 |
| **heap_4** | 本工程的堆策略，支持释放并合并相邻空闲块 |

### A.2 任务 API

| 功能 | API |
|---|---|
| 创建 | `xTaskCreate(func, name, stackWords, param, prio, &handle)` |
| 删除 | `vTaskDelete(handle_or_NULL)` |
| 相对延时 | `vTaskDelay(ticks)` |
| 绝对延时 | `xTaskDelayUntil(&last, ticks)` |
| 挂起 / 恢复 | `vTaskSuspend(h)` / `vTaskResume(h)` |
| 查优先级 | `uxTaskPriorityGet(h)` / `vTaskPrioritySet(h, p)` |
| 查栈水位 | `uxTaskGetStackHighWaterMark(h)` |
| 当前句柄 | `xTaskGetCurrentTaskHandle()` |
| 启动调度器 | `vTaskStartScheduler()` |
| 主动让出 | `taskYIELD()` |

### A.3 通信/同步 API

| 组件 | 创建 | 发送 / 释放 | 接收 / 获取 | ISR 版 |
|---|---|---|---|---|
| 队列 | `xQueueCreate(n, size)` | `xQueueSend(q, &v, t)` | `xQueueReceive(q, &v, t)` | `...FromISR` |
| 二值信号量 | `xSemaphoreCreateBinary()` | `xSemaphoreGive(s)` | `xSemaphoreTake(s, t)` | `...FromISR` |
| 计数信号量 | `xSemaphoreCreateCounting(max, init)` | 同上 | 同上 | 同上 |
| 互斥量 | `xSemaphoreCreateMutex()` | `xSemaphoreGive(s)` | `xSemaphoreTake(s, t)` | ❌ 不可用 |
| 递归互斥量 | `xSemaphoreCreateRecursiveMutex()` | `...GiveRecursive(s)` | `...TakeRecursive(s, t)` | ❌ 不可用 |
| 任务通知 | **不需要创建** | `xTaskNotify(h, v, act)` | `xTaskNotifyWait(mask_in, mask_out, &v, t)` | `...FromISR` |
| 事件组 | `xEventGroupCreate()` | `xEventGroupSetBits(g, bits)` | `xEventGroupWaitBits(g, bits, clr, all, t)` | 受限 |
| 软件定时器 | `xTimerCreate(...)` | `xTimerStart / Stop / Reset / ChangePeriod` | 回调形式 | `...FromISR` |

### A.4 常用宏

```c
pdMS_TO_TICKS(ms)      /* 毫秒 → tick（本工程 tick=1ms，数值相等，但建议始终用它） */
pdTRUE / pdFALSE
pdPASS / pdFAIL
portMAX_DELAY          /* 死等（慎用：保护外设时用它 = 永久挂死的风险） */
portTICK_PERIOD_MS     /* 一个 tick 是多少毫秒（本工程 = 1） */
```

---

## 附录 B 常见误区速查（面试也爱考）

| # | 误区 | 真相 |
|---|---|---|
| 1 | "上下文切换是在 PendSV 中断里完成的" | 寄存器保存/恢复在 PendSV 里，但**真正的切换发生在异常返回边界**（`bx r14`） |
| 2 | "BASEPRI=5 是关掉优先级低于 5 的中断" | 反了：掩蔽**数值 ≥ 5** 的中断；数值 0-4（逻辑更高）照常响应 |
| 3 | "互斥量比二值信号量快" | 相反。互斥量多了持有者追踪 + 优先级继承开销，**更慢但防饿死** |
| 4 | "任务通知是功能受限的慢信号量" | 相反：通知**最快**（无对象无拷贝），代价是只能单播 |
| 5 | "`vTaskDelay(1)` 保证睡 1 个 tick" | 实际睡 **1~2 个 tick**（醒来点永远落在 tick 边界） |
| 6 | "ISR 里能调带超时的 `xQueueReceive`" | 不行。ISR 必须用 FromISR 版本，只能"尝试"，失败立即返回 |
| 7 | "事件组有 32 个事件位可用" | 只有**低 24 位**，高 8 位被控制位占用 |
| 8 | "软件定时器回调在中断里执行" | 在**定时器守护任务**上下文执行，回调不能长时间阻塞 |
| 9 | "`uxTopReadyPriority` 是个数值" | 在 CLZ 优化模式下是**位图** |
| 10 | "信号量也有优先级继承" | 没有。**只有互斥量有**（它才需要 `xMutexHolder`） |
| 11 | "任务通知可以广播" | 不能，只能单播（广播请用事件组） |
| 12 | "中断里可以给互斥量" | 不行，ISR 没有 TCB，内核有断言拦着 |
| 13 | "计数信号量能告诉我发生了哪几类事件" | **不能**。它内部只有一个数字，不携带事件类型。只适用于**同质**事件（详见 §3.4） |
| 14 | "计数信号量当资源池就能管理好资源" | 只解决了"还剩几个"，没解决"用哪一个"。要拿确定的资源句柄，用**队列**装句柄（详见 §3.4） |

---

## 附录 C 本仓库 FreeRTOS 资料地图

本文档内容筛选整合自以下 5 份资料。读完后想深入源码，按这个地图回查：

| 原文档 | 视角 | 什么时候回查 |
|---|---|---|
| **《FreeRTOS_零基础循序渐进指南》（本文档）** | 入门 + 应用 | 第一次学、写代码时查用法 |
| 《FreeRTOS体系详解_综合版.md》 | 体系（七层模型 + 每层函数拆解） | 想系统理解"层与层怎么搭起来" |
| 《FreeRTOS源码级讲解_2-11.md》 | 源码（第 2-11 个知识点，含行号） | 想对着源码逐行验证某个机制 |
| 《FreeRTOS直观化补充_配套2-11.md》 | 图解（栈帧布局、闹钟模型、插队大戏） | 某个机制"想不通"时看它的类比和 ASCII 图 |
| 《FreeRTOS信号量与互斥量_源码级讲解.md》 | 专题（信号量/互斥量深挖 + 本工程落地建议） | 要在本工程里加信号量/互斥量时必读 |
| 《FreeRTOS直观化演示.html》 | 交互演示（七层体系图 + 6 个机制模拟器） | 想"动手玩"机制模拟器 |

### 建议的完整学习路径

```
第 1 周：本文档第 0 章（链表地基）+ 第 1-4 章 + 实验 1/2/3
        目标：能独立写出多任务 + 队列 + 互斥量的应用，
              并且能从"链表搬家"的角度解释每个 API 做了什么

第 2 周：本文档第 5-8 章 + 实验 4/5
        目标：理解三个进阶组件，会选型

第 3 周：读《体系详解_综合版》的 L0-L3，配合《直观化演示.html》
        目标：搞懂"任务怎么被创建、怎么被调度、怎么切换"

第 4 周：读《信号量与互斥量源码级讲解》+《源码级讲解_2-11》
        目标：能在源码层面解释机制，具备排障和面试能力

持续：   用第 10 章的水位监控 + vTaskList() 做日常体检
```

---

*本文档整合自本仓库 `docs/` 下的 5 份 FreeRTOS 资料，源码版本 FreeRTOS Kernel V11.1.0（Cortex-M4F），
工程配置以 `OS/FreeRTOSConfig.h` 为准。文中"本工程"均指 STM32F411 手表 App 工程。*

