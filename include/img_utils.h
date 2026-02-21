#ifndef _IMG_UTILS_HH
#define _IMG_UTILS_HH

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

struct img_t;

typedef struct img_t {
    uint32_t *data;
    size_t    height;
    size_t    width;
    struct img_t *bg_ref;
} img_t;

img_t *create_img(size_t width, size_t height, img_t *img);

static inline void channel_color(img_t *img, int32_t x, int32_t y, int ch, uint8_t *tr, uint8_t *tg, uint8_t *tb) {
    uint32_t bg_pixel = 0;
    if (img->bg_ref) {
        /* Sample from background reference (scaled) */
        int32_t bx = x * (int32_t)img->bg_ref->width / (int32_t)img->width;
        int32_t by = y * (int32_t)img->bg_ref->height / (int32_t)img->height;
        if ((uint32_t)bx < img->bg_ref->width && (uint32_t)by < img->bg_ref->height) {
            bg_pixel = img->bg_ref->data[by * img->bg_ref->width + bx];
        }
    } else {
        /* Sample from current image */
        uint32_t idx = (uint32_t)y * img->width + (uint32_t)x;
        bg_pixel = img->data[idx];
    }

    uint8_t r = (bg_pixel >> 16) & 0xFF, g = (bg_pixel >> 8) & 0xFF, b = bg_pixel & 0xFF;
    if      (ch == 1) { *tr = 255;             *tg = (255-g)/2; *tb = (255-b)/2; }
    else if (ch == 3) { *tr = (255-r)/2;       *tg = (255-g)/2; *tb = 255;       }
    else               { *tr = (255-r);         *tg = 255;       *tb = (255-b);   }
}

/* Basic shapes */
void draw_circle_aa(img_t *img, int32_t cx, int32_t cy, int32_t radius, uint32_t color);
void draw_circle_aa_dynamic(img_t *img, int32_t cx, int32_t cy, int32_t radius, int primary_channel);
void draw_circle_aa_alpha(img_t *img, int32_t cx, int32_t cy, int32_t radius, int primary_channel, uint8_t alpha_scale);
void draw_circle_dashed_aa_dynamic(img_t *img, int32_t cx, int32_t cy, int32_t radius, int primary_channel);
void draw_circle_glow(img_t *img, int32_t cx, int32_t cy, int32_t radius, int primary_channel);
void draw_circle_glow_color(img_t *img, int32_t cx, int32_t cy, int32_t radius, uint32_t color);

void fill_circle(img_t *img, int32_t cx, int32_t cy, int32_t radius, uint32_t color);
void fill_circle_dynamic(img_t *img, int32_t cx, int32_t cy, int32_t radius, int primary_channel);
void fill_circle_glow(img_t *img, int32_t cx, int32_t cy, int32_t radius, int primary_channel);

void draw_rect_fill(img_t *img, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void draw_rect_blend(img_t *img, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void draw_rect_stroke(img_t *img, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

void draw_rounded_rect_blend(img_t *img, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);
void draw_rounded_rect_fill(img_t *img, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);

#endif /* _IMG_UTILS_HH */