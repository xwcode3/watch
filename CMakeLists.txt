cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME watch_bootloader)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32 Standard Peripheral Library sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    # Note: Sources are now added via the stm32cubemx subdirectory
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    USE_STDPERIPH_DRIVER
    STM32F411xE
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    # Add user defined libraries
)

# Custom target: flash firmware to MCU via J-Link
# Shows up in CMake Tools sidebar under Targets

# Generate .bin file from .elf after linking
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${CMAKE_PROJECT_NAME}.bin"
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.bin"
)

add_custom_target(flash
    COMMAND ${CMAKE_SOURCE_DIR}/flash.sh
    COMMENT "Flashing watch.elf to STM32F411CEU6 via J-Link"
    USES_TERMINAL
)

# Custom target: full chip erase via J-Link
add_custom_target(erase
    COMMAND ${CMAKE_SOURCE_DIR}/erase.sh
    COMMENT "Full chip erase of STM32F411CEU6 (Bootloader + APP)"
    USES_TERMINAL
)
