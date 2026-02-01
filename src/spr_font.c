#include "spr_font.h"
#include "font.h"
#include <stddef.h>

void spr_draw_char_overlay(uint32_t* buffer, int width, int height, int x, int y, char c, uint32_t color) {
    int idx = (unsigned char)c;
    if (idx < 32 || idx > 127) return;
    idx -= 32;
    for (int r = 0; r < 8; ++r) {
        int py = y + r;
        if (py < 0 || py >= height) continue;
        uint8_t row = font8x8_basic[idx][r];
        for (int b = 0; b < 8; ++b) {
            int px = x + b;
            if (px < 0 || px >= width) continue;
            if ((row >> (7 - b)) & 1) {
                buffer[py * width + px] = color;
            }
        }
    }
}

void spr_draw_string_overlay(uint32_t* buffer, int width, int height, int x, int y, const char* str, uint32_t color) {
    while (*str) {
        spr_draw_char_overlay(buffer, width, height, x + 1, y + 1, *str, 0xFF000000); /* Drop shadow */
        spr_draw_char_overlay(buffer, width, height, x, y, *str, color);
        x += 8;
        str++;
    }
}
