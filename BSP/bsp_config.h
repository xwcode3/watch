#pragma once
#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

#include <stdio.h>

#define OS_SUPPORTING                   /* Enable if OS support is available */
// #define DEBUG                           /* Enable debugging */
// #define DEBUG_OUT(x) printf(x)          /* Outputs debug information */
#define DEBUG_OUT(...) printf(__VA_ARGS__)          /* Outputs debug information */

#endif // BSP_CONFIG_H