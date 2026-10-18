/**
 * @file    mid_ringbuf.c
 * @brief   通用字节环形缓冲中间件实现（head/tail 推导长度版）
 *
 * 索引约定（与原 app/BSP/USART/mid_circular_buffer 同源）：
 *   - head 指向下一个可写位置，tail 指向下一个可读位置；
 *   - 牺牲一个槽位判满：((head + 1) & mask) == tail 即满，head == tail 即空；
 *     因此可用容量 = cap - 1；
 *   - 长度由索引推导：used = (head - tail) & mask，不需要额外的 count 字段；
 *   - cap 为 2 的幂，回绕用 & mask 代替取模。
 *
 * 这样设计的关键收益：
 *   【生产者只写 head，消费者只写 tail】——两者不争抢同一个变量。
 *   故在 reject 模式 + 单生产者/单消费者(SPSC) 下可以完全不加锁。
 *   为保证无锁正确，写入时"先数据、后 head"、读取时"先数据、后 tail"之间
 *   插入编译器屏障 RINGBUF_BARRIER()，防止编译器重排导致对方看到半截状态。
 *
 * @warning overwrite 模式下生产者会推进 tail 丢弃旧数据，与消费者写 tail
 *          冲突，此时【必须】挂锁，不能依赖无锁 SPSC。
 *
 * 可重入性：无任何 static/全局状态，不同实例之间完全独立、可重入。
 *
 * 临界区纪律：
 *   所有会修改 head/tail 的代码段都必须在锁内【重新】判断满/空，
 *   不得依赖锁外的判断结果（否则并发下会读到/写入越界）。
 */
#include "mid_ringbuf.h"

/* 判断 n 是否为 2 的幂（n>0 且 (n & (n-1)) == 0） */
static bool is_power_of_two(uint32_t n)
{
    return (n != 0u) && ((n & (n - 1u)) == 0u);
}

/* 实例有效性：已 init 过（buf 非空且 cap 非 0）才可用。
   未初始化的实例一律按"容量为 0 的空缓冲"处理，并在写操作处返回错误暴露问题。*/
static bool ringbuf_valid(const ringbuf_t *rb)
{
    return (rb != NULL) && (rb->buf != NULL) && (rb->cap != 0u);
}

/* --- 原始状态判定（内部用，均带屏障以确保无锁 SPSC 下读到最新索引） --- */

static bool ringbuf_empty_raw(const ringbuf_t *rb)
{
    RINGBUF_BARRIER();
    return (rb->head == rb->tail);
}

static bool ringbuf_full_raw(const ringbuf_t *rb)
{
    RINGBUF_BARRIER();
    return (((rb->head + 1u) & rb->mask) == rb->tail);
}

static uint32_t ringbuf_used_raw(const ringbuf_t *rb)
{
    RINGBUF_BARRIER();
    return ((rb->head - rb->tail) & rb->mask);
}

ringbuf_status_t ringbuf_init(ringbuf_t *rb, uint8_t *storage, uint32_t cap_bytes)
{
    if (rb == NULL || storage == NULL) {
        RINGBUF_LOG_E("ringbuf_init: NULL param");
        return RINGBUF_NULL;
    }
    if (!is_power_of_two(cap_bytes)) {
        RINGBUF_LOG_E("ringbuf_init: cap must be power of two");
        return RINGBUF_ERR;
    }

    rb->buf          = storage;
    rb->cap          = cap_bytes;
    rb->mask         = cap_bytes - 1u;
    rb->head         = 0u;
    rb->tail         = 0u;
    rb->dma_active   = false;
    rb->dma_reserved = 0u;
    /* 锁钩子由调用方后续通过 ringbuf_set_lock 设置，这里保持 NULL */
    rb->pf_lock      = NULL;
    rb->pf_unlock    = NULL;
    RINGBUF_BARRIER();
    return RINGBUF_OK;
}

ringbuf_status_t ringbuf_set_lock(ringbuf_t *rb,
                                  void (*lock)(void), void (*unlock)(void))
{
    if (rb == NULL) {
        return RINGBUF_NULL;
    }
    /* 必须成对：只设其一会导致加锁后永不解锁（死锁），直接拒绝这种配置 */
    if (((lock == NULL) && (unlock != NULL)) ||
        ((lock != NULL) && (unlock == NULL))) {
        RINGBUF_LOG_E("ringbuf_set_lock: lock/unlock must be paired");
        return RINGBUF_ERR;
    }

    rb->pf_lock   = lock;
    rb->pf_unlock = unlock;
    return RINGBUF_OK;
}

bool ringbuf_is_empty(const ringbuf_t *rb)
{
    /* 无效实例视为空：读不到东西是安全的 */
    return ringbuf_valid(rb) ? ringbuf_empty_raw(rb) : true;
}

bool ringbuf_is_full(const ringbuf_t *rb)
{
    /* 无效实例返回 false：让"忘记 init"在随后的写操作（返回 RINGBUF_NULL）处暴露，
       而不是被 is_full==true 静默吞掉 */
    return ringbuf_valid(rb) ? ringbuf_full_raw(rb) : false;
}

uint32_t ringbuf_available(const ringbuf_t *rb)
{
    return ringbuf_valid(rb) ? ringbuf_used_raw(rb) : 0u;
}

uint32_t ringbuf_free_space(const ringbuf_t *rb)
{
    /* 牺牲一个槽位，故可用空间 = cap - 1 - used */
    return ringbuf_valid(rb) ? (rb->cap - 1u - ringbuf_used_raw(rb)) : 0u;
}

uint32_t ringbuf_capacity(const ringbuf_t *rb)
{
    /* 注意：返回总容量 cap，实际可存 cap - 1 字节 */
    return ringbuf_valid(rb) ? rb->cap : 0u;
}

ringbuf_status_t ringbuf_push(ringbuf_t *rb, uint8_t b, bool overwrite)
{
    if (!ringbuf_valid(rb)) {
        RINGBUF_LOG_E("ringbuf_push: invalid instance (not initialized?)");
        return RINGBUF_NULL;
    }

    if (rb->pf_lock != NULL) {
        rb->pf_lock();
    }

    /* DMA 在途：拒绝 CPU 写入，避免与 DMA 抢同一片内存 */
    if (rb->dma_active) {
        if (rb->pf_unlock != NULL) { rb->pf_unlock(); }
        RINGBUF_LOG_E("ringbuf_push: DMA in progress");
        return RINGBUF_BUSY;
    }

    /* 判满必须在锁内 */
    if (ringbuf_full_raw(rb)) {
        if (!overwrite) {
            if (rb->pf_unlock != NULL) { rb->pf_unlock(); }
            return RINGBUF_FULL;
        }
        /* 覆盖最旧：丢弃一个 tail 位置，腾出空间
           （注意：这会修改 tail，故 overwrite 模式必须挂锁） */
        rb->tail = (rb->tail + 1u) & rb->mask;
    }

    rb->buf[rb->head] = b;
    /* 先让数据落盘，再公开 head——无锁 SPSC 的关键顺序 */
    RINGBUF_BARRIER();
    rb->head = (rb->head + 1u) & rb->mask;

    if (rb->pf_unlock != NULL) {
        rb->pf_unlock();
    }
    return RINGBUF_OK;
}

uint32_t ringbuf_write(ringbuf_t *rb, const uint8_t *src, uint32_t len, bool overwrite)
{
    uint32_t written = 0u;

    if (!ringbuf_valid(rb) || src == NULL) {
        RINGBUF_LOG_E("ringbuf_write: invalid param");
        return 0u;
    }

    /* 整段用一把锁包住，避免中间态被并发读/写破坏 */
    if (rb->pf_lock != NULL) {
        rb->pf_lock();
    }

    if (rb->dma_active) {
        if (rb->pf_unlock != NULL) { rb->pf_unlock(); }
        RINGBUF_LOG_E("ringbuf_write: DMA in progress");
        return 0u;
    }

    for (written = 0u; written < len; ++written) {
        if (ringbuf_full_raw(rb)) {
            if (!overwrite) {
                break;   /* 空间不足且 reject：停止写入 */
            }
            rb->tail = (rb->tail + 1u) & rb->mask;
        }
        rb->buf[rb->head] = src[written];
        RINGBUF_BARRIER();
        rb->head = (rb->head + 1u) & rb->mask;
    }

    if (rb->pf_unlock != NULL) {
        rb->pf_unlock();
    }
    return written;
}

ringbuf_status_t ringbuf_pop(ringbuf_t *rb, uint8_t *b)
{
    ringbuf_status_t st = RINGBUF_OK;

    if (!ringbuf_valid(rb) || b == NULL) {
        RINGBUF_LOG_E("ringbuf_pop: invalid param");
        return RINGBUF_NULL;
    }

    if (rb->pf_lock != NULL) {
        rb->pf_lock();
    }

    /* 判空必须在【锁内】进行：若放在锁外，并发下另一线程可能已把数据读走，
       本线程随后会读到越界/陈旧数据 */
    if (ringbuf_empty_raw(rb)) {
        st = RINGBUF_EMPTY;
    } else {
        *b = rb->buf[rb->tail];
        /* 先读完数据，再公开 tail——无锁 SPSC 的关键顺序 */
        RINGBUF_BARRIER();
        rb->tail = (rb->tail + 1u) & rb->mask;
    }

    if (rb->pf_unlock != NULL) {
        rb->pf_unlock();
    }
    return st;
}

uint32_t ringbuf_read(ringbuf_t *rb, uint8_t *dst, uint32_t len)
{
    uint32_t got = 0u;

    if (!ringbuf_valid(rb) || dst == NULL) {
        RINGBUF_LOG_E("ringbuf_read: invalid param");
        return 0u;
    }

    if (rb->pf_lock != NULL) {
        rb->pf_lock();
    }

    for (got = 0u; got < len; ++got) {
        if (ringbuf_empty_raw(rb)) {
            break;
        }
        dst[got] = rb->buf[rb->tail];
        RINGBUF_BARRIER();
        rb->tail = (rb->tail + 1u) & rb->mask;
    }

    if (rb->pf_unlock != NULL) {
        rb->pf_unlock();
    }
    return got;
}

uint32_t ringbuf_peek(const ringbuf_t *rb, uint8_t *dst, uint32_t len)
{
    uint32_t got = 0u;
    uint32_t idx;

    if (!ringbuf_valid(rb) || dst == NULL) {
        RINGBUF_LOG_E("ringbuf_peek: invalid param");
        return 0u;
    }

    /* peek 不修改 tail/head，但【需要】加锁：写入方可能并发推进 head，
       甚至在 overwrite 模式下覆盖我们正在预览的区域，导致读到撕裂数据 */
    if (rb->pf_lock != NULL) {
        rb->pf_lock();
    }

    idx = rb->tail;
    RINGBUF_BARRIER();
    for (got = 0u; (got < len) && (idx != rb->head); ++got) {
        dst[got] = rb->buf[idx];
        idx = (idx + 1u) & rb->mask;
    }

    if (rb->pf_unlock != NULL) {
        rb->pf_unlock();
    }
    return got;
}

void ringbuf_reset(ringbuf_t *rb)
{
    if (!ringbuf_valid(rb)) {
        return;
    }
    if (rb->pf_lock != NULL) {
        rb->pf_lock();
    }
    rb->head         = 0u;
    rb->tail         = 0u;
    rb->dma_active   = false;
    rb->dma_reserved = 0u;
    RINGBUF_BARRIER();
    if (rb->pf_unlock != NULL) {
        rb->pf_unlock();
    }
}

/* ================================================================== */
/* DMA 零拷贝路径                                                      */
/* ================================================================== */

uint8_t *ringbuf_dma_begin(ringbuf_t *rb, uint32_t want_len, uint32_t *contiguous)
{
    uint32_t free_sp;
    uint32_t run;

    if (contiguous != NULL) {
        *contiguous = 0u;
    }
    if (!ringbuf_valid(rb)) {
        RINGBUF_LOG_E("ringbuf_dma_begin: invalid instance");
        return NULL;
    }
    if (want_len == 0u) {
        return NULL;
    }

    if (rb->pf_lock != NULL) {
        rb->pf_lock();
    }

    /* 不允许并发的两次 DMA 预留 */
    if (rb->dma_active) {
        if (rb->pf_unlock != NULL) { rb->pf_unlock(); }
        RINGBUF_LOG_E("ringbuf_dma_begin: previous DMA not committed");
        return NULL;
    }

    /* 只预留空闲空间：DMA 无法在传输中途丢弃旧数据，故不支持 overwrite */
    free_sp = rb->cap - 1u - ringbuf_used_raw(rb);
    if (free_sp == 0u) {
        if (rb->pf_unlock != NULL) { rb->pf_unlock(); }
        return NULL;
    }

    /* 连续段长度 = head 到缓冲区末尾；再受空闲空间与期望长度约束 */
    run = rb->cap - rb->head;
    if (run > free_sp)   { run = free_sp; }
    if (run > want_len)  { run = want_len; }

    rb->dma_active   = true;
    rb->dma_reserved = run;
    if (contiguous != NULL) {
        *contiguous = run;
    }

    if (rb->pf_unlock != NULL) {
        rb->pf_unlock();
    }
    return &rb->buf[rb->head];
}

ringbuf_status_t ringbuf_dma_commit(ringbuf_t *rb, uint32_t n)
{
    if (!ringbuf_valid(rb)) {
        return RINGBUF_NULL;
    }

    /* 只有提交这一小段加锁：DMA 传输期间不持锁（锁管不住 DMA） */
    if (rb->pf_lock != NULL) {
        rb->pf_lock();
    }

    if (!rb->dma_active) {
        if (rb->pf_unlock != NULL) { rb->pf_unlock(); }
        RINGBUF_LOG_E("ringbuf_dma_commit: no DMA in progress");
        return RINGBUF_ERR;
    }
    if (n > rb->dma_reserved) {
        if (rb->pf_unlock != NULL) { rb->pf_unlock(); }
        RINGBUF_LOG_E("ringbuf_dma_commit: n exceeds reserved");
        return RINGBUF_ERR;
    }

    /* DMA 数据已在内存中，公开新的 head */
    RINGBUF_BARRIER();
    rb->head = (rb->head + n) & rb->mask;

    rb->dma_active   = false;
    rb->dma_reserved = 0u;

    if (rb->pf_unlock != NULL) {
        rb->pf_unlock();
    }
    return RINGBUF_OK;
}

void ringbuf_dma_abort(ringbuf_t *rb)
{
    if (!ringbuf_valid(rb)) {
        return;
    }
    if (rb->pf_lock != NULL) {
        rb->pf_lock();
    }
    rb->dma_active   = false;
    rb->dma_reserved = 0u;
    RINGBUF_BARRIER();
    if (rb->pf_unlock != NULL) {
        rb->pf_unlock();
    }
}
