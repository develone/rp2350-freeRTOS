if ("${PICO_PLATFORM}" STREQUAL "rp2040")
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
elseif("${PICO_PLATFORM}" STREQUAL "rp2350-arm-s")
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)
elseif("${PICO_PLATFORM}" STREQUAL "rp2350-riscv")
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_RISC-V/FreeRTOS_Kernel_import.cmake)
endif()
#include_directories(/home/devel/rp2350-freeRTOS/test-read-crc16 )
include_directories( /home/devel/pico2/freertos-lib/FreeRTOS-Kernel/include )
#include_directories(/home/devel/pico2/freertos/FreeRTOS-Kernel/include )
include_directories(/home/devel/pico2/freertos)
include_directories("${CMAKE_CURRENT_LIST_DIR} ")
include_directories( FreeRTOS-Kernel/include)
include_directories( /home/devel/pico2/freertos-lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/include)
include_directories( /home/devel/pico2/freertos-lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350/non_secure )
add_executable(test-read-crc16
        main.c crc16.h xreceive.h xtransmit.h  vi.c vi.h comprogs.h   pnmio.h 
)
SET(FREERTOS_KERNEL_PATH "$ENV{FREERTOS_KERNEL_PATH}" CACHE STRING "Course Common Lib")
message("Using FREERTOS_KERNEL_PATH from environment ('${FREERTOS_KERNEL_PATH}')")
SET(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR} " CACHE STRING "Local Config")
message("Using  FREERTOS_CONFIG_FILE_DIRECTORY " ${CMAKE_CURRENT_LIST_DIR} " ")
#SET(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" )
#include("${FREERTOS_CONFIG_FILE_DIRECTORY} freertos_config.cmake")
#include("${FREERTOS_CONFIG_FILE_DIRECTORY}" )
#include(FreeRTOS_Kernel_import.cmake)
target_link_libraries(test-read-crc16 pico_stdlib xmodem FreeRTOS-Kernel lifting crc_crc16 cir-buf klt littlefs openjpeg)
pico_enable_stdio_usb(test-read-crc16 1)
pico_enable_stdio_uart(test-read-crc16 1)

pico_add_extra_outputs(test-read-crc16)
