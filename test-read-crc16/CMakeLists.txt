add_executable(test-read-crc16
        main.c crc16.h xreceive.h xtransmit.h pico_hal.c pico_hal.h lfs.c lfs.h lfs_util.h vi.c vi.h comprogs.h FreeRTOS.h  pnmio.h 
)

target_link_libraries(test-read-crc16 pico_stdlib xmodem freertos lifting crc_crc16 cir-buf klt littlefs-lib openjpeg)

pico_enable_stdio_usb(test-read-crc16 1)
pico_enable_stdio_uart(test-read-crc16 1)

pico_add_extra_outputs(test-read-crc16)
