cmake_minimum_required(VERSION 3.22)

# STM32 Toolchain configuration
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross compiler
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(water_quality_monitor C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ==============================================================================
# Build configuration
# ==============================================================================
option(BUILD_MODE_ROVER "Enable rover mode with GPS + thrusters" ON)
option(LORA_REGION_EU868 "Use 868 MHz LoRa (EU)" ON)
option(ENABLE_SATELLITE "Enable Iridium satellite backhaul" OFF)
option(BUILD_TESTS "Build unit tests (native, not cross-compiled)" OFF)

# MCU Definitions
set(MCU_FAMILY STM32L4xx)
set(MCU_MODEL STM32L476xx)
set(MCU_LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32L476RGTx_FLASH.ld)

# ==============================================================================
# Compiler flags
# ==============================================================================
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS "${CPU_FLAGS} -Wall -Wextra -Wpedantic -fdata-sections -ffunction-sections")
set(CMAKE_C_FLAGS_DEBUG "-Og -g3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

set(CMAKE_EXE_LINKER_FLAGS "${CPU_FLAGS} -T${MCU_LINKER_SCRIPT} -Wl,--gc-sections -Wl,--print-memory-usage -specs=nano.specs -specs=nosys.specs -lm")

# ==============================================================================
# HAL & CMSIS paths (user must set STM32_HAL_DIR)
# ==============================================================================
if(NOT DEFINED STM32_HAL_DIR)
    set(STM32_HAL_DIR "${CMAKE_SOURCE_DIR}/Drivers" CACHE PATH "Path to STM32 HAL drivers")
endif()

set(CMSIS_DIR "${STM32_HAL_DIR}/CMSIS")
set(HAL_DIR "${STM32_HAL_DIR}/STM32L4xx_HAL_Driver")

# ==============================================================================
# Include directories
# ==============================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/config
    ${CMSIS_DIR}/Include
    ${CMSIS_DIR}/Device/ST/${MCU_FAMILY}/Include
    ${HAL_DIR}/Inc
)

# ==============================================================================
# Source files
# ==============================================================================

# HAL sources (add what you need)
file(GLOB HAL_SOURCES
    "${HAL_DIR}/Src/stm32l4xx_hal.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_cortex.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_rcc.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_rcc_ex.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_pwr.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_pwr_ex.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_gpio.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_adc.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_adc_ex.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_dma.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_spi.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_uart.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_tim.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_tim_ex.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_rtc.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_rtc_ex.c"
    "${HAL_DIR}/Src/stm32l4xx_hal_i2c.c"
)

# Startup file
set(STARTUP_FILE "${CMSIS_DIR}/Device/ST/${MCU_FAMILY}/Source/Templates/gcc/startup_stm32l476xx.s")
set(SYSTEM_FILE "${CMSIS_DIR}/Device/ST/${MCU_FAMILY}/Source/Templates/system_stm32l4xx.c")

# Application sources
set(APP_SOURCES
    src/main.c
    src/stm32l4xx_hal_msp.c
    src/stm32l4xx_it.c
    src/system_clock.c
    src/freertos_hooks.c

    # Sensors
    src/sensors/ph_sensor.c
    src/sensors/turbidity_sensor.c
    src/sensors/do_sensor.c
    src/sensors/temperature_sensor.c
    src/sensors/sensor_manager.c

    # EKF
    src/ekf/ekf.c
    src/ekf/water_quality_ekf.c
    src/ekf/anomaly_detector.c

    # Communications
    src/comms/lora_driver.c
    src/comms/packet.c

    # Power
    src/power/power_manager.c
    src/power/battery_monitor.c
    src/power/solar_monitor.c
)

# Rover-mode specific sources
if(BUILD_MODE_ROVER)
    list(APPEND APP_SOURCES
        src/nav/gps_driver.c
        src/nav/waypoint_nav.c
        src/nav/motor_driver.c
    )
    add_definitions(-DBUILD_MODE_ROVER=1)
endif()

# Satellite backhaul sources
if(ENABLE_SATELLITE)
    list(APPEND APP_SOURCES
        src/comms/satellite.c
    )
    add_definitions(-DENABLE_SATELLITE=1)
endif()

# LoRa region
if(LORA_REGION_EU868)
    add_definitions(-DLORA_REGION_EU868=1)
else()
    add_definitions(-DLORA_REGION_US915=1)
endif()

# ==============================================================================
# FreeRTOS
# ==============================================================================
if(NOT DEFINED FREERTOS_DIR)
    set(FREERTOS_DIR "${CMAKE_SOURCE_DIR}/Middlewares/FreeRTOS" CACHE PATH "Path to FreeRTOS source")
endif()

set(FREERTOS_SOURCES
    ${FREERTOS_DIR}/Source/croutine.c
    ${FREERTOS_DIR}/Source/event_groups.c
    ${FREERTOS_DIR}/Source/list.c
    ${FREERTOS_DIR}/Source/queue.c
    ${FREERTOS_DIR}/Source/stream_buffer.c
    ${FREERTOS_DIR}/Source/tasks.c
    ${FREERTOS_DIR}/Source/timers.c
    ${FREERTOS_DIR}/Source/portable/GCC/ARM_CM4F/port.c
    ${FREERTOS_DIR}/Source/portable/MemMang/heap_4.c
)

include_directories(
    ${FREERTOS_DIR}/Source/include
    ${FREERTOS_DIR}/Source/portable/GCC/ARM_CM4F
    ${CMAKE_SOURCE_DIR}/config  # for FreeRTOSConfig.h
)

# ==============================================================================
# Firmware target
# ==============================================================================
set(TARGET_ELF ${PROJECT_NAME}.elf)
set(TARGET_BIN ${PROJECT_NAME}.bin)
set(TARGET_HEX ${PROJECT_NAME}.hex)

add_executable(${TARGET_ELF}
    ${STARTUP_FILE}
    ${SYSTEM_FILE}
    ${HAL_SOURCES}
    ${FREERTOS_SOURCES}
    ${APP_SOURCES}
)

target_link_libraries(${TARGET_ELF} m)

# Generate .bin and .hex from .elf
add_custom_command(TARGET ${TARGET_ELF} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET_ELF} ${TARGET_BIN}
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${TARGET_ELF} ${TARGET_HEX}
    COMMAND ${CMAKE_SIZE} ${TARGET_ELF}
    COMMENT "Generating ${TARGET_BIN} and ${TARGET_HEX}"
)

# ==============================================================================
# Unit tests (native build, not cross-compiled)
# ==============================================================================
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
