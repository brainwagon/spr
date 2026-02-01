#ifndef SPR_FONT_H
#define SPR_FONT_H

#include <stdint.h>

/* Draws a single character (8x8) to the buffer */
void spr_draw_char_overlay(uint32_t* buffer, int width, int height, int x, int y, char c, uint32_t color);

/* Draws a string to the buffer */
void spr_draw_string_overlay(uint32_t* buffer, int width, int height, int x, int y, const char* str, uint32_t color);

#endif /* SPR_FONT_H */
