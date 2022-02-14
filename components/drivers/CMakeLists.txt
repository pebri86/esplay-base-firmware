set(include_dirs display input storage)

set(srcs 
    "display/disp_spi.c"
    "display/display.c"
    "display/ili9341.c"
    "display/ili9342.c"
    "input/gamepad.c"
    "storage/sdcard.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    REQUIRES fatfs esp_adc_cal spiffs)