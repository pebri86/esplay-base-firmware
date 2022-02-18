#ifndef _DISPLAY_H_
#define _DISPLAY_H_
#include <stdint.h>

//*****************************************************************************
//
// Make sure all of the definitions in this header have a C binding.
//
//*****************************************************************************

#ifdef __cplusplus
extern "C"
{
#endif

void display_init();
void backlight_deinit();
void display_send_fb(uint16_t* buffer);
void renderGfx(short left, short top, short width, short height, uint16_t* buffer, short sx, short sy, short tileSetWidth);
void display_prepare();
void display_show_hourglass();
void display_show_empty_battery();
void display_show_splash();
void display_clear(uint16_t color);
void set_display_brightness(int percent);
void display_prepare();
void display_poweroff();

#ifdef __cplusplus
}
#endif

#endif /*_DISPLAY_H_*/
