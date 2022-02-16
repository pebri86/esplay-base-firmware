set(include_dirs display input storage)

set(srcs 
    "display/display.c"
    "input/gamepad.c"
    "storage/sdcard.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    REQUIRES fatfs esp_adc_cal spiffs esp_lcd)