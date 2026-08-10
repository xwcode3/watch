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

typedef struct
{
    osal_status_t (*queue_create)(uint32_t item_num,
                            uint32_t item_size,
                            void **handle);
    osal_status_t (*queue_put)(void *handle,
                         const void *item,
                         uint32_t timeout_ms);
    osal_status_t (*queue_get)(void *handle,
                         void *item,
                         uint32_t timeout_ms);
    osal_status_t (*queue_delete)(void *handle);
} osal_queue_t;

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

#endif /* OSAL_H */
