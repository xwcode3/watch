/**
 * @file    mid_ringbuf.h
 * @brief   通用字节环形缓冲中间件（参考 app/BSP/USART/mid_circular_buffer.c 思路重写）
 *
 * 定位：一份可独立编译、零堆依赖、可用于 MPU6050 等传感器"接收数据"的 FIFO
 *       中间件。比原 mid_circular_buffer 的改进：
 *         - 不依赖 elog / malloc：由调用方提供存储区，移植到无堆/无日志的工程也 OK；
 *         - 容量取 2 的幂，用 (head+1) & mask 代替 % 取模（更快，成熟做法）；
 *         - 保留原实现"由 head/tail 推导长度、牺牲一个槽位判满"的思路；
 *         - 增加整块 write/read（一次塞进/取出一整帧 MPU6050 数据）；
 *         - 增加可选临界区钩子 pf_lock/pf_unlock；
 *         - 满时策略可选：丢弃最新(reject) 或 覆盖最旧(overwrite)；
 *         - 区分 CPU 拷贝 与 DMA 零拷贝 两条写入路径（见下）。
 *
 * ==================================================================
 * 一、索引约定（与原 mid_circular_buffer 同源）
 * ==================================================================
 *   - head 指向下一个可写位置，tail 指向下一个可读位置；
 *   - 牺牲一个槽位判满：(head + 1) & mask == tail  即满；head == tail 即空；
 *   - 长度由索引推导：used = (head - tail) & mask，可用容量 = cap - 1；
 *   - cap 为 2 的幂，mask = cap - 1，回绕用 & mask 代替取模。
 *
 *   选择该方案的收益（也是相对 count 判满版的关键优势）：
 *     【生产者只写 head，消费者只写 tail】，二者不争抢同一个变量。
 *     因此在 reject 模式 + 单生产者/单消费者(SPSC) 下，可以【完全不加锁】
 *     （pf_lock = NULL），做到真正的无锁 FIFO。详见第三节。
 *
 * ==================================================================
 * 二、两种写入路径（必须二选一，不可混用）
 * ==================================================================
 *   A. CPU 拷贝路径 —— ringbuf_write()
 *      数据在调用方缓冲区里，由 CPU 拷进环形区，整个拷贝过程持锁。
 *      适合：任务上下文、数据量小到中等、没有 DMA 的场景（普通用法）。
 *
 *   B. DMA 零拷贝路径 —— ringbuf_dma_begin() + ringbuf_dma_commit()
 *      由 DMA 把外设数据直接写进环形区，CPU 只在"提交"时更新 head。
 *      DMA 传输期间【不】持锁（锁管不住 DMA 这个总线主设备，持锁毫无意义），
 *      只在 commit 的极短窗口内加锁。
 *      适合：I2C/SPI/UART 配 DMA 收数、或中断上下文收数。
 *
 *   混用保护：DMA 在途期间调用 ringbuf_write()/push() 会被 dma_active 拦截并
 *      返回 0 / RINGBUF_BUSY，不会破坏数据。
 *
 *   @note Cortex-M4(F411) 无 D-cache，DMA 写入一致性无虞；若移植到带 D-cache
 *         的 M7 等平台，commit 前需对接收区做 cache invalidate。
 *
 * ==================================================================
 * 三、可重入性与线程安全（重要）
 * ==================================================================
 *   1) 不同实例之间：完全独立、可重入。
 *      本中间件没有任何 static / 全局状态，所有状态都在调用方传入的 ringbuf_t
 *      实例内；内部辅助函数（如 is_power_of_two）只使用参数。
 *      => 多个线程各自操作自己的 ringbuf，互不影响，也不需要任何锁。
 *      => 唯一例外是日志宏 RINGBUF_LOG_E：若把它重定向到 printf / elog 等
 *         非可重入实现，日志本身会成为共享状态。默认实现为 ((void)0)，
 *         不引入任何状态，因此默认是可重入的。
 *
 *   2) 同一个实例：按【模式】区分是否需要锁（本版相对 count 版的重要改进）：
 *
 *      ┌────────────────────┬──────────────┬───────────────────────┐
 *      │ 场景               │ 是否需要锁    │ 原因                  │
 *      ├────────────────────┼──────────────┼───────────────────────┤
 *      │ reject + SPSC      │ 不需要       │ 生产者只写 head，      │
 *      │ (单生产者单消费者)  │ (pf_lock=NULL)│ 消费者只写 tail       │
 *      │ reject + MPSC/MPMC │ 需要         │ 多个生产者争抢 head    │
 *      │ overwrite 模式     │ 需要         │ 生产者要推进 tail      │
 *      │                    │              │ 丢弃旧数据，与消费者   │
 *      │                    │              │ 写 tail 冲突           │
 *      └────────────────────┴──────────────┴───────────────────────┘
 *
 *      多任务/多生产者/overwrite 场景，请用 ringbuf_set_lock 注入锁。
 *
 *   3) 无锁 SPSC 的前提：需要编译器屏障防止重排（见 RINGBUF_BARRIER）。
 *      本头文件已按编译器提供默认实现，通常无需改动。
 *      注意：无锁仅适用于【单核】且无其它总线主设备的场景；
 *      若 DMA 也在写同一缓冲，请走 DMA 路径并保留锁。
 *
 *   4) ISR 上下文：
 *      pf_lock 若挂的是 taskENTER_CRITICAL 这类任务级临界区，不能在 ISR 中调用；
 *      ISR 里应使用 FromISR 变体，或改用上面的 DMA 提交路径（提交窗口极短）。
 *      【切勿】在 ISR 中使用互斥锁。
 *
 * @note    适用平台：任意 Cortex-M + 编译器（无特权指令、无堆依赖）。
 */
#ifndef MID_RINGBUF_H
#define MID_RINGBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 可选日志：默认空实现（可重入）。需要时在包含本头之前 #define RINGBUF_LOG_E(...) 即可。
   注意：重定向到 printf/elog 会引入共享状态，可能破坏"不同实例间可重入"的性质。*/
#ifndef RINGBUF_LOG_E
#define RINGBUF_LOG_E(...)   ((void)0)
#endif

/* 编译器屏障：无锁 SPSC 下防止编译器重排 head/tail 的读写顺序。
   单核 Cortex-M 为顺序执行，编译器屏障即足够（无需 DMB 指令）。
   多核或带乱序的平台上，可将本宏重定义为硬件内存屏障（如 __DMB()）。*/
#ifndef RINGBUF_BARRIER
#  if defined(__GNUC__) || defined(__clang__)
#    define RINGBUF_BARRIER() __asm__ volatile("" ::: "memory")
#  elif defined(__ARMCC_VERSION)
#    define RINGBUF_BARRIER() __schedule_barrier()
#  else
#    define RINGBUF_BARRIER() ((void)0)
#  endif
#endif

/** 返回状态 */
typedef enum {
    RINGBUF_OK      = 0x00,  /*!< 成功                        */
    RINGBUF_ERR     = 0x01,  /*!< 一般错误（含 DMA 在途冲突） */
    RINGBUF_NULL    = 0x02,  /*!< 入参为 NULL / 实例未初始化  */
    RINGBUF_FULL    = 0x03,  /*!< 缓冲已满（reject 策略）     */
    RINGBUF_EMPTY   = 0x04,  /*!< 缓冲为空                    */
    RINGBUF_BUSY    = 0x05,  /*!< DMA 传输在途，拒绝 CPU 写入 */
} ringbuf_status_t;

/** 环形缓冲实例（所有字段均为内部状态，调用方只负责提供 buf 存储） */
typedef struct ringbuf {
    uint8_t  *buf;     /*!< 用户提供的环形存储区               */
    uint32_t  cap;     /*!< 总容量（必须是 2 的幂）            */
    uint32_t  mask;    /*!< cap - 1，用于 & 运算代替取模       */
    uint32_t  head;    /*!< 下一个写入位置（写指针，生产者写）  */
    uint32_t  tail;    /*!< 下一个读取位置（读指针，消费者写）  */
    bool      dma_active;    /*!< DMA 传输在途标志（拦截 CPU 写入）*/
    uint32_t  dma_reserved;  /*!< 本次 DMA 预留的连续字节数        */
    void    (*pf_lock)(void);   /*!< 可选：进入临界区（NULL=不加锁） */
    void    (*pf_unlock)(void); /*!< 可选：退出临界区（NULL=不加锁） */
} ringbuf_t;

/**
 * @brief  用外部存储初始化环形缓冲
 * @param  rb        实例指针
 * @param  storage   用户提供的字节数组（生命周期需 >= 缓冲生命周期）
 * @param  cap_bytes 存储区字节数，必须是 2 的幂（如 32/64/128/256）
 * @retval RINGBUF_OK 成功；RINGBUF_NULL 入参错；RINGBUF_ERR 容量非 2 的幂
 * @note   不调用 malloc，适合无堆/静态分配场景。
 * @note   可用容量为 cap_bytes - 1（牺牲一个槽位用于判满）。选型时记得留这一字节。
 */
ringbuf_status_t ringbuf_init(ringbuf_t *rb, uint8_t *storage, uint32_t cap_bytes);

/**
 * @brief  设置线程安全钩子（可选）
 * @param  rb     实例指针
 * @param  lock   进入临界区回调（如 taskENTER_CRITICAL 封装）
 * @param  unlock 退出临界区回调
 * @retval RINGBUF_OK 成功；RINGBUF_NULL rb 为空；RINGBUF_ERR 两个钩子未成对
 * @note   lock/unlock 必须【同时为 NULL 或同时非 NULL】。只设其中一个会导致
 *         加锁后永不解锁（死锁），本函数会直接拒绝这种配置。
 * @note   reject + 单生产者/单消费者场景可以【不挂锁】，实现真正的无锁 FIFO。
 */
ringbuf_status_t ringbuf_set_lock(ringbuf_t *rb,
                                  void (*lock)(void), void (*unlock)(void));

/** @return 缓冲是否为空（head == tail）。无效实例视为"空" */
bool        ringbuf_is_empty(const ringbuf_t *rb);
/** @return 缓冲是否已满（(head+1)&mask == tail）。无效实例返回 false，让"忘记 init"在写操作处暴露 */
bool        ringbuf_is_full(const ringbuf_t *rb);
/** @return 当前已存字节数 = (head - tail) & mask（无效实例返回 0） */
uint32_t    ringbuf_available(const ringbuf_t *rb);
/** @return 当前剩余可写字节数 = cap - 1 - used（无效实例返回 0） */
uint32_t    ringbuf_free_space(const ringbuf_t *rb);
/** @return 缓冲总容量 cap（注意：可用容量为 cap - 1；无效实例返回 0） */
uint32_t    ringbuf_capacity(const ringbuf_t *rb);

/**
 * @brief  写入一个字节
 * @param  overwrite true=满时覆盖最旧数据；false=满时返回 RINGBUF_FULL 拒绝写入
 * @note   DMA 在途时返回 RINGBUF_BUSY。
 * @note   overwrite=true 时本函数会修改 tail，此时【必须】挂锁（见头文件第三节）。
 */
ringbuf_status_t ringbuf_push(ringbuf_t *rb, uint8_t b, bool overwrite);

/**
 * @brief  批量写入（CPU 拷贝路径，适合一次塞进一整帧 MPU6050 数据）
 * @param  src       数据源
 * @param  len       期望写入字节数
 * @param  overwrite 同 ringbuf_push
 * @return 实际写入的字节数（可能 < len：空间不足且 reject）
 * @note   DMA 在途时返回 0。
 * @note   若 pf_lock 挂的是临界区（关中断），len 不宜过大，否则长时间关中断。
 */
uint32_t ringbuf_write(ringbuf_t *rb, const uint8_t *src, uint32_t len, bool overwrite);

/**
 * @brief  读出一个字节（消费，读指针前进）
 */
ringbuf_status_t ringbuf_pop(ringbuf_t *rb, uint8_t *b);

/**
 * @brief  批量读取（消费）
 * @return 实际读出的字节数（可能 < len：可读不足）
 */
uint32_t ringbuf_read(ringbuf_t *rb, uint8_t *dst, uint32_t len);

/**
 * @brief  窥视（不消费，读指针不前进），用于先预览帧头再决定是否取出
 * @return 实际可窥视的字节数
 * @note   本函数【会】加锁：因为 head/tail 可能被并发的写入方修改，
 *         不加锁会读到撕裂数据。
 */
uint32_t ringbuf_peek(const ringbuf_t *rb, uint8_t *dst, uint32_t len);

/** 清空（重置 head/tail，不擦除 buf 内容） */
void ringbuf_reset(ringbuf_t *rb);

/* ================================================================== */
/* DMA 零拷贝路径（路径 B，与 ringbuf_write 二选一，不可混用）          */
/* ================================================================== */

/**
 * @brief  预留一段【连续】可写空间，交给 DMA 直接写入
 * @param  rb          实例指针
 * @param  want_len    期望预留的字节数
 * @param  contiguous  出参：实际可连续写入的字节数（<= want_len）
 * @return 可写起始地址；失败返回 NULL（无空间 / 实例无效 / DMA 已在途）
 * @note   只预留空闲空间，【不】覆盖已有数据（DMA 无法在传输中途丢弃旧数据）。
 * @note   若发生回绕，一次只能拿到尾部这一段；提交后可再次调用本函数取头部段。
 */
uint8_t *ringbuf_dma_begin(ringbuf_t *rb, uint32_t want_len, uint32_t *contiguous);

/**
 * @brief  提交 DMA 已写入的字节数（更新 head，仅此处短暂加锁）
 * @param  rb   实例指针
 * @param  n    DMA 实际写入的字节数（<= 上次 begin 返回的 contiguous）
 * @retval RINGBUF_OK 成功；RINGBUF_ERR 未 begin 或 n 超出预留；RINGBUF_NULL 入参错
 */
ringbuf_status_t ringbuf_dma_commit(ringbuf_t *rb, uint32_t n);

/**
 * @brief  放弃本次 DMA 预留（传输失败时回滚，不做任何提交）
 */
void ringbuf_dma_abort(ringbuf_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* MID_RINGBUF_H */
