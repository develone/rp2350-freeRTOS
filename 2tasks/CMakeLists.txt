add_executable(2tasks
        main.c
)
if ("${PICO_PLATFORM}" STREQUAL "rp2040")
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
elseif("${PICO_PLATFORM}" STREQUAL "rp2350-arm-s")
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)
elseif("${PICO_PLATFORM}" STREQUAL "rp2350-riscv")
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_RISC-V/FreeRTOS_Kernel_import.cmake)
endif()

include_directories("${CMAKE_CURRENT_LIST_DIR} ")
include_directories( FreeRTOS-Kernel/include)
include_directories( FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/include)
SET(FREERTOS_KERNEL_PATH "$ENV{FREERTOS_KERNEL_PATH}" CACHE STRING "Course Common Lib")
message("Using FREERTOS_KERNEL_PATH from environment ('${FREERTOS_KERNEL_PATH}')")
SET(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR} " CACHE STRING "Local Config")
message("Using  FREERTOS_CONFIG_FILE_DIRECTORY " ${CMAKE_CURRENT_LIST_DIR} " ")
#SET(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" )
#include("${FREERTOS_CONFIG_FILE_DIRECTORY} freertos_config.cmake")
#include("${FREERTOS_CONFIG_FILE_DIRECTORY}" )
#include(FreeRTOS_Kernel_import.cmake)

target_link_libraries(2tasks pico_stdlib FreeRTOS-Kernel)

pico_enable_stdio_usb(2tasks 1)
pico_enable_stdio_uart(2tasks 0)

pico_add_extra_outputs(2tasks)