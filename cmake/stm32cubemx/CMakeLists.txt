cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# STM32CubeMX generated symbols (macros) - modified for StdPeriph
set(MX_Defines_Syms 
	# Removed USE_HAL_DRIVER, added USE_STDPERIPH_DRIVER
	USE_STDPERIPH_DRIVER
	STM32F411xE
    $<$<CONFIG:Debug>:DEBUG>
)

# STM32CubeMX generated include paths - modified for StdPeriph
set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc
    # Added Standard Peripheral Library include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_StdPeriph_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS

	# FreeRTOS include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/../../OS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../OS/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../OS/port

	# Middlewares
	${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/RTT

	${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/easylogger/inc
	${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/easylogger/plugins/file
	${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/easylogger/plugins/flash

	# BSP
	${CMAKE_CURRENT_SOURCE_DIR}/../../BSP/SPEAKER
	${CMAKE_CURRENT_SOURCE_DIR}/../../BSP/LCD
)

# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/RTT/SEGGER_RTT.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/RTT/SEGGER_RTT_printf.c

    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/easylogger/src/elog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/easylogger/src/elog_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/easylogger/src/elog_buf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/easylogger/src/elog_async.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/easylogger/port/elog_port.c

	# Core
	${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/usart.c

	# BSP
	${CMAKE_CURRENT_SOURCE_DIR}/../../BSP/SPEAKER/bsp_speaker_driver.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../BSP/LCD/bsp_lcd_driver.c
)

# STM32 Standard Peripheral Drivers (replacing HAL drivers)
set(STD_PERIPH_Src
	${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32f411xe.s
	${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/stm32f4xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/system_stm32f4xx.c
    # Standard Peripheral Library source files - include commonly used drivers
    # Add other required peripheral driver source files as needed
	${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_StdPeriph_Driver/src/misc.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_rcc.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_gpio.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_syscfg.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_exti.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_usart.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_flash.c
)

# FreeRTOS source files
set(FreeRTOS_Src
	${CMAKE_CURRENT_SOURCE_DIR}/../../OS/port/heap_4.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../OS/port/port.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../OS/src/croutine.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../OS/src/event_groups.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../OS/src/list.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../OS/src/queue.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../OS/src/stream_buffer.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../OS/src/tasks.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../OS/src/timers.c
)

# Drivers Midllewares

# Link directories setup
set(MX_LINK_DIRS

)
# Project static libraries
set(MX_LINK_LIBS 
    STM32_StdPeriph
	FreeRTOS_lib
    ${TOOLCHAIN_LINK_LIBRARIES}
    
)
# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create STM32_StdPeriph static library (replacing STM32_Drivers)
add_library(STM32_StdPeriph OBJECT)
target_sources(STM32_StdPeriph PRIVATE ${STD_PERIPH_Src})
target_link_libraries(STM32_StdPeriph PUBLIC stm32cubemx)

# Create FreeRTOS_lib static library (replacing STM32_Drivers)
add_library(FreeRTOS_lib OBJECT)
target_sources(FreeRTOS_lib PRIVATE ${FreeRTOS_Src})
target_link_libraries(FreeRTOS_lib PUBLIC stm32cubemx)


# Add STM32CubeMX generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()

# Note: Commenting out old HAL library configuration
# add_library(STM32_Drivers OBJECT)
# target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
# target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)