#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/rtc_io.h"
#include "display.h"
#include "splash.h"

#define LINE_BUFFERS (2)
#define LINE_COUNT (24)

// Pin Cofiguration
#define DISP_SPI_MOSI 23
#define DISP_SPI_CLK 18
#define DISP_SPI_CS 5
#define DISP_SPI_DC 12
#define LCD_BCKL 27

// SPI Parameter
#define SPI_CLOCK_SPEED (60 * 1000 * 1000)
#define LCD_HOST       SPI2_HOST

// The pixel number in horizontal and vertical
#define LCD_H_RES              320
#define LCD_V_RES              240
// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

uint16_t *line[LINE_BUFFERS];
extern uint16_t myPalette[];

esp_lcd_panel_handle_t panel_handle = NULL;

static const int DUTY_MAX = 0x1fff;
static const int LCD_BACKLIGHT_ON_VALUE = 1;
static bool isBackLightIntialized = false;

static void backlight_init()
{
    //configure timer0
    ledc_timer_config_t ledc_timer;
    memset(&ledc_timer, 0, sizeof(ledc_timer));

    ledc_timer.duty_resolution = LEDC_TIMER_13_BIT; //set timer counter bit number
    ledc_timer.freq_hz = 5000;                      //set frequency of pwm
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;    //timer mode,
    ledc_timer.timer_num = LEDC_TIMER_0;            //timer index

    ledc_timer_config(&ledc_timer);

    //set the configuration
    ledc_channel_config_t ledc_channel;
    memset(&ledc_channel, 0, sizeof(ledc_channel));

    //set LEDC channel 0
    ledc_channel.channel = LEDC_CHANNEL_0;
    //set the duty for initialization.(duty range is 0 ~ ((2**duty_resolution)-1)
    ledc_channel.duty = (LCD_BACKLIGHT_ON_VALUE) ? 0 : DUTY_MAX;
    //GPIO number
    ledc_channel.gpio_num = LCD_BCKL;
    //GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
    ledc_channel.intr_type = LEDC_INTR_FADE_END;
    //set LEDC mode, from ledc_mode_t
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    //set LEDC timer source, if different channel use one timer,
    //the frequency and duty_resolution of these channels should be the same
    ledc_channel.timer_sel = LEDC_TIMER_0;

    ledc_channel_config(&ledc_channel);

    //initialize fade service.
    ledc_fade_func_install(0);

    // duty range is 0 ~ ((2**duty_resolution)-1)
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (LCD_BACKLIGHT_ON_VALUE) ? DUTY_MAX : 0, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);

    isBackLightIntialized = true;

    printf("Backlight initialization done.\n");
}

void set_display_brightness(int percent)
{
    int duty = DUTY_MAX * (percent * 0.01f);

    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
}


void backlight_deinit()
{
    ledc_fade_func_uninstall();
    esp_err_t err = ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    if (err != ESP_OK)
    {
        printf("%s: ledc_stop failed.\n", __func__);
    }
}

void display_prepare(int percent)
{
    // Return use of backlight pin
    esp_err_t err = rtc_gpio_deinit(LCD_BCKL);
    if (err != ESP_OK)
    {
        abort();
    }
}

void display_poweroff(int percent)
{
    esp_err_t err = ESP_OK;

    // fade off backlight
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (LCD_BACKLIGHT_ON_VALUE) ? 0 : DUTY_MAX, 100);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_WAIT_DONE);

    err = rtc_gpio_init(LCD_BCKL);
    if (err != ESP_OK)
    {
        abort();
    }

    err = rtc_gpio_set_direction(LCD_BCKL, RTC_GPIO_MODE_OUTPUT_ONLY);
    if (err != ESP_OK)
    {
        abort();
    }

    err = rtc_gpio_set_level(LCD_BCKL, LCD_BACKLIGHT_ON_VALUE ? 0 : 1);
    if (err != ESP_OK)
    {
        abort();
    }
}

void display_send_fb(uint16_t *buffer)
{
    short i, x, y;
    int index, bufferIndex, sending_line = -1, calc_line = 0;

    if (buffer == NULL)
    {
        for (y = 0; y < LCD_V_RES; y += LINE_COUNT) 
        {
            for (i = 0; i < LINE_COUNT; ++i)
            {
                for (x = 0; x < LCD_H_RES; ++x)
                {
                    line[calc_line][x] = 0;
                }
            }
            sending_line = calc_line;
            calc_line = !calc_line;
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + LINE_COUNT, line[sending_line]);
        }
    }
    else
    {
        for (y = 0; y < LCD_V_RES; y += LINE_COUNT)
        {
            for (i = 0; i < LINE_COUNT; ++i)
            {
                if ((y + i) >= LCD_V_RES)
                    break;

                index = i * LCD_H_RES;
                bufferIndex = (y + i) * LCD_H_RES;
                for (x = 0; x < LCD_H_RES; ++x)
                {
                    uint16_t pixel = buffer[bufferIndex++];
                    line[calc_line][index++] = ((pixel >> 8) | ((pixel & 0xff) << 8));
                }
            }
            sending_line = calc_line;
            calc_line = !calc_line;
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + LINE_COUNT, line[sending_line]);
        }
    }
}

void renderGfx(short left, short top, short width, short height, uint16_t *buffer, short sx, short sy, short tileSetWidth)
{
    short x, y, xv, yv;
    int sending_line = -1;
    int calc_line = 0;

    if (left < 0 || top < 0)
        abort();
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;
    if (buffer == NULL)
    {
        for (y = top; y < height + top; y++)
        {
            xv = 0;
            for (x = left; x < width + left; x++)
            {
                line[calc_line][xv] = 0;
                xv++;
            }

            sending_line = calc_line;
            calc_line = !calc_line;
            esp_lcd_panel_draw_bitmap(panel_handle, left, y, left + width, y + 1, line[sending_line]);
        }
    }
    else
    {
        yv = 0;
        for (y = top; y < top + height; y++)
        {
            xv = 0;
            for (int i = left; i < left + width; ++i)
            {
                uint16_t pixel = buffer[(yv + sy) * tileSetWidth + (xv + sx)];
                line[calc_line][xv] = ((pixel << 8) | (pixel >> 8));
                xv++;
            }

            sending_line = calc_line;
            calc_line = !calc_line;
            esp_lcd_panel_draw_bitmap(panel_handle, left, y, left + width, y + 1, line[sending_line]);
            yv++;
        }
    }
}

void display_show_splash()
{
    display_clear(0xffff);
    for (short i = 1; i < 151; ++i)
    {
        renderGfx((LCD_H_RES - splash_screen.width) / 2,
                  (LCD_V_RES - splash_screen.height) / 2,
                  i,
                  splash_screen.height,
                  splash_screen.pixel_data,
                  0,
                  0,
                  150);
        vTaskDelay(2);
    }
    vTaskDelay(100);
}

void display_clear(uint16_t color)
{
    int sending_line = -1;
    int calc_line = 0;

    // clear the buffer
    for (int i = 0; i < LINE_BUFFERS; ++i)
    {
        for (int j = 0; j < LCD_H_RES * LINE_COUNT; ++j)
        {
            line[i][j] = color;
        }
    }

    for (int y = 0; y < LCD_V_RES; y += LINE_COUNT)
    {
        sending_line = calc_line;
        calc_line = !calc_line;
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 0 + LCD_H_RES, y + LINE_COUNT, line[sending_line]);
    }
}

void display_init()
{
    // Line buffers
    const size_t lineSize = LCD_H_RES * LINE_COUNT * sizeof(uint16_t);
    for (int x = 0; x < LINE_BUFFERS; x++)
    {
        line[x] = heap_caps_malloc(lineSize, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        if (!line[x])
            abort();
    }

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[DISP_SPI_DC], PIN_FUNC_GPIO);
    gpio_set_direction(DISP_SPI_DC, GPIO_MODE_OUTPUT);

    backlight_init();

    spi_bus_config_t buscfg = {
        .sclk_io_num = DISP_SPI_CLK,
        .mosi_io_num = DISP_SPI_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LINE_COUNT * LCD_H_RES * 2 + 8
    };

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = DISP_SPI_DC,
        .cs_gpio_num = DISP_SPI_CS,
        .pclk_hz = SPI_CLOCK_SPEED,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    // Initialize the LCD configuration
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    // Turn off backlight to avoid unpredictable display on the LCD screen while initializing
    // the LCD panel driver. (Different LCD screens may need different levels)
    set_display_brightness(0);

    // Reset the display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    // Initialize LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Swap x and y axis (Different LCD screens may need different options)
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));

    // Turn ON Display
    set_display_brightness(100);
}
