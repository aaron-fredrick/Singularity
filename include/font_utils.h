#ifndef _FONT_UTILS_HH
#define _FONT_UTILS_HH

#include <stdint.h>
#include "img_utils.h"

void draw_char(img_t *img, char c, uint32_t x, uint32_t y);
void draw_char_colored(img_t *img, char c, uint32_t x, uint32_t y, uint32_t color);
void draw_string_colored(img_t *img, const char *str, uint32_t x, uint32_t y, uint32_t color);
void draw_string(img_t *img, const char *str, uint32_t x, uint32_t y);

void draw_char_dynamic(img_t *img, char c, uint32_t x, uint32_t y, int primary_channel);
void draw_string_dynamic(img_t *img, const char *str, uint32_t x, uint32_t y, int primary_channel);

#endif /* _FONT_UTILS_HH */
