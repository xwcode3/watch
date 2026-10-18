#ifndef OSAL_H
#define OSAL_H

#include <stdint.h>

#define OSAL_NO_WAIT        (0UL)
#define OSAL_WAIT_FOREVER   (0xFFFFFFFFUL)

typedef enum
{
    OSAL_OK = 0,
    OSAL_ERROR = 1,
    OSAL_ERRORTIMEOUT = 2,
    OSAL_ERRORRESOURCE = 3,
    OSAL_ERRORPARAMETER = 4,
    OSAL_ERRORNOMEMORY = 5,
    OSAL_ERRORISR = 6,
    OSAL_RESERVED = 0xFF
} osal_status_t;

typedef struct
{
    osal_status_t (*delay_ms)(uint32_t ms);
} osal_delay_t;

typedef struct
{
    osal_status_t (*get_tick_ms)(uint32_t *ms);
} osal_time_base_t;

/**
 * @brief  消息队列接口, 多生产者/多消费者的按值拷贝数据通道
 * @note   队列按值拷贝传递数据, item_size 在创建时确定, 收发双方必须一致
 * @note   带 *_from_isr 的接口只可在中断服务函数中调用, 且不阻塞
 * @note   各接口的 timeout_ms 单位均为 ms: OSAL_NO_WAIT 不等待, OSAL_WAIT_FOREVER 永久等待
 */
typedef struct
{
    /* Queue Functions */
    osal_status_t (*queue_create)(uint32_t item_num,
                                  uint32_t item_size,
                                  void **handle);
    osal_status_t (*queue_delete)(void *handle);
    osal_status_t (*queue_put)(void *handle,
                               const void *item,
                               uint32_t timeout_ms);
    osal_status_t (*queue_get)(void *handle,
                               void *item,
                               uint32_t timeout_ms);
    osal_status_t (*queue_put_from_isr)(void *handle,
                                        const void *item,
                                        long * const pxHigherPriorityTaskWoken);
    osal_status_t (*queue_get_from_isr)(void *handle,
                                        void *item,
                                        long * const pxHigherPriorityTaskWoken);
} osal_queue_t;

/**
 * @brief  互斥量接口, 用于保护共享资源
 * @note   互斥量有"持有者"并支持优先级继承, 可避免优先级翻转, 保护共享资源优先用它
 * @note   lock/unlock 必须由同一任务成对调用, 不可在中断中使用
 * @note   timeout_ms 单位 ms, 语义同队列
 */
typedef struct
{
    /* Mutex Functions */
    osal_status_t (*mutex_create)(void **handle);
    osal_status_t (*mutex_delete)(void *handle);
    osal_status_t (*mutex_lock)(void *handle, uint32_t timeout_ms);
    osal_status_t (*mutex_unlock)(void *handle);
} osal_mutex_t;

/**
 * @brief  二值信号量接口, 用于任务与中断之间的事件同步
 * @note   计数值只有 0/1, 不累计事件次数: 连续多次 give 只等效于一次
 * @note   无持有者概念, 可在中断中 give; 不适合保护共享资源(无法避免优先级翻转)
 * @note   带 *_from_isr 的接口只可在中断服务函数中调用
 */
typedef struct
{
    /* Binary Semaphore Functions */
    osal_status_t (*semaphore_create)(void **handle);
    osal_status_t (*semaphore_delete)(void *handle);
    osal_status_t (*semaphore_take)(void *handle, uint32_t timeout_ms);
    osal_status_t (*semaphore_give)(void *handle);
    osal_status_t (*semaphore_take_from_isr)(void *handle, long * const pxHigherPriorityTaskWoken);
    osal_status_t (*semaphore_give_from_isr)(void *handle, long * const pxHigherPriorityTaskWoken);
} osal_sem_t;

/**
 * @brief  计数信号量接口, 用于管理多个同类资源或累计同质事件
 * @note   take 成功一次计数减 1, 计数为 0 时阻塞; 适合资源池计数与同类事件累计
 * @note   它只数数、不区分事件来源: 需要区分"是哪类事件"请改用队列、任务通知或事件组
 * @note   它也只解决"还剩几个", 不解决"用哪一个": 要拿到确定的资源句柄请用队列
 */
typedef struct
{
    /* Counting Semaphore Functions */
    osal_status_t (*semaphore_counting_create)(void **handle,
                                               uint32_t max_count,
                                               uint32_t initial_count);
    osal_status_t (*semaphore_counting_delete)(void *handle);
    osal_status_t (*semaphore_counting_take)(void *handle, uint32_t timeout_ms);
    osal_status_t (*semaphore_counting_give)(void *handle);
    osal_status_t (*semaphore_counting_take_from_isr)(void *handle, long * const pxHigherPriorityTaskWoken);
    osal_status_t (*semaphore_counting_give_from_isr)(void *handle, long * const pxHigherPriorityTaskWoken);
} osal_sem_counting_t;


/**
 * @brief 任务通知值写入策略
 * @note  与 FreeRTOS 的 eNotifyAction 对应; 经 OSAL 调用时上层无需包含 FreeRTOS 头文件
 */
typedef enum
{
    OSAL_NOTIFY_ACTION_NONE = 0,          /**< 只唤醒目标任务, 不改写通知值 */
    OSAL_NOTIFY_ACTION_SET_BITS,          /**< 通知值按位或写入 value, 适合当事件标志用 */
    OSAL_NOTIFY_ACTION_INCREMENT,         /**< 通知值自增 1 并忽略 value, 即计数信号量语义 */
    OSAL_NOTIFY_ACTION_SET_VALUE,         /**< 用 value 覆盖通知值 */
    OSAL_NOTIFY_ACTION_SET_VALUE_IF_CLEAR /**< 仅当无未读通知时才写入, 已有未读则返回错误 */
} osal_notify_action_t;

/**
 * @brief  任务通知接口, 最轻量的任务间同步原语
 * @note   通知值内嵌在目标任务的 TCB 中, 不额外分配内核对象, 因而没有 create/delete;
 *         handle 即 osal_thread_t::create 输出的任务句柄
 * @note   一个任务只有一个通知值, 只能点对点单播: 不能广播, 也不能同时等待两个通知;
 *         多对一/广播场景请改用队列或事件组
 * @note   notify_wait / notify_take 只作用于调用者自身, 故不带 handle 参数
 */
typedef struct
{
    /**
     * @brief  向目标任务发送通知(计数信号量语义)
     * @param  handle 目标任务句柄, 不可为 NULL
     * @retval OSAL_OK             通知值已自增 1
     * @retval OSAL_ERRORPARAMETER handle 为 NULL
     * @note   任务上下文专用; 中断中请使用 notify_send_from_isr
     * @note   与 notify_take 配对: 目标任务当前未阻塞等待时, 计数会累计
     */
    osal_status_t (*notify_send)(void *handle);

    /**
     * @brief  中断上下文中向目标任务发送通知(可携带 32 位通知值)
     * @param  handle               目标任务句柄, 不可为 NULL
     * @param  value                通知值, 含义由 action 决定; action 为 INCREMENT 时被忽略
     * @param  action               通知值写入策略
     * @param  pxHigherPriorityTaskWoken 输出参数, 非 0 表示唤醒了更高优先级任务,
     *                              须在中断退出前执行一次上下文切换, 否则唤醒会推迟到下次调度
     * @retval OSAL_OK             发送成功
     * @retval OSAL_ERRORRESOURCE  action 为 SET_VALUE_IF_CLEAR 且目标任务已有未读通知
     * @retval OSAL_ERRORPARAMETER handle 为 NULL
     * @note   仅可在中断服务函数中调用
     */
    osal_status_t (*notify_send_from_isr)(void *handle,
                                          uint32_t value,
                                          osal_notify_action_t action,
                                          long * const pxHigherPriorityTaskWoken);

    /**
     * @brief  阻塞等待本任务的通知, 并读出通知值(不改写通知值)
     * @param  value      输出参数, 读到的通知值
     * @param  timeout_ms 阻塞超时, 单位 ms; OSAL_NO_WAIT 不阻塞, OSAL_WAIT_FOREVER 永久阻塞
     * @retval OSAL_OK            收到通知
     * @retval OSAL_ERRORTIMEOUT  超时
     * @retval OSAL_ERRORPARAMETER value 为 NULL
     * @note   返回后"未读"状态被清除, 再次等待需重新 send; 通知值本身保留
     */
    osal_status_t (*notify_wait)(uint32_t *value,
                                 uint32_t timeout_ms);

    /**
     * @brief  阻塞等待并取走本任务的通知计数(与 notify_send 配对)
     * @param  count      输出参数, 取走前的计数值, 成功时恒大于 0
     * @param  timeout_ms 阻塞超时, 单位 ms
     * @retval OSAL_OK            计数非 0, 已取走并使计数减 1
     * @retval OSAL_ERRORTIMEOUT  超时, 计数仍为 0
     * @retval OSAL_ERRORPARAMETER count 为 NULL
     */
    osal_status_t (*notify_take)(uint32_t *count,
                                 uint32_t timeout_ms);
} osal_notify_t;

typedef struct
{
    osal_status_t (*enter)(void);
    osal_status_t (*exit)(void);
} osal_critical_t;

typedef void (*osal_thread_entry_t)(void *param);

typedef struct
{
    osal_status_t (*create)(osal_thread_entry_t entry,
                            const char *name,
                            uint32_t stack_depth,
                            void *param,
                            uint32_t priority,
                            void **handle);
    osal_status_t (*delete)(void *handle);
} osal_thread_t;

typedef struct
{
    osal_status_t (*yield)(void);
} osal_yield_t;


typedef struct {
    void (*loop_delay_us) (uint32_t us);
    void (*loop_delay_ms) (uint32_t ms); 
} osal_loop_delay_t;

#endif /* OSAL_H */
