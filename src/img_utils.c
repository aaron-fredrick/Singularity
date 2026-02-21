#include "img_utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Buffer creation ─────────────────────────────────────────────────── */

img_t *create_img(size_t width, size_t height, img_t *img) {
    if (img == NULL) {
        img = malloc(sizeof(img_t));
        if (!img) return NULL;
    }
    img->width  = width;
    img->height = height;
    img->data   = malloc(width * height * sizeof(uint32_t));
    if (!img->data) return NULL;
    return img;
}

/* ── Pixel blending helpers ───────────────────────────────────────────── */

static inline void blend_pixel_fast(img_t *img, int32_t x, int32_t y,
                                     uint32_t color, uint32_t alpha) {
    if ((uint32_t)x >= img->width || (uint32_t)y >= img->height || alpha == 0) return;
    uint32_t idx = (uint32_t)y * img->width + (uint32_t)x;
    
    if (alpha >= 255) {
        img->data[idx] = (0xFF << 24) | (color & 0xFFFFFF);
        return;
    }

    uint32_t bg = img->data[idx];
    uint32_t ba = (bg >> 24) & 0xFF;

    if (ba == 0) {
        /* Blending on empty pixel: just set color and alpha as-is */
        img->data[idx] = (alpha << 24) | (color & 0xFFFFFF);
        return;
    }

    /* Blending on existing UI element: simple linear blend */
    uint32_t br  = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    uint32_t fr  = (color >> 16) & 0xFF, fg = (color >> 8) & 0xFF, fb = color & 0xFF;
    uint32_t ia  = 256 - alpha;
    
    uint32_t out_r = (fr * alpha + br * ia) >> 8;
    uint32_t out_g = (fg * alpha + bg_g * ia) >> 8;
    uint32_t out_b = (fb * alpha + bb * ia) >> 8;
    uint32_t out_a = alpha + (ba * ia >> 8);
    if (out_a > 255) out_a = 255;

    img->data[idx] = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

static inline void blend_pixel(img_t *img, int32_t x, int32_t y,
                                uint32_t color, float alpha) {
    blend_pixel_fast(img, x, y, color, (uint32_t)(alpha * 255.0f));
}

/* Derive a channel colour from the primary_channel index used by dynamic functions */
/* (Implementation moved to img_utils.h for inlining) */

/* ── Solid AA circle ─────────────────────────────────────────────────── */

void draw_circle_aa(img_t *img, int32_t cx, int32_t cy, int32_t radius, uint32_t color) {
    int32_t x0 = cx-radius-2, y0 = cy-radius-2;
    int32_t x1 = cx+radius+2, y1 = cy+radius+2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)img->width)  x1 = img->width  - 1;
    if (y1 >= (int32_t)img->height) y1 = img->height - 1;
    for (int32_t y = y0; y <= y1; y++) {
        for (int32_t x = x0; x <= x1; x++) {
            float dist = sqrtf((float)((x-cx)*(x-cx)+(y-cy)*(y-cy)));
            float diff = fabsf(dist - radius);
            if (diff < 1.0f) blend_pixel(img, x, y, color, 1.0f - diff);
        }
    }
}

void draw_circle_aa_dynamic(img_t *img, int32_t cx, int32_t cy,
                             int32_t radius, int primary_channel) {
    int32_t x0 = cx-radius-2, y0 = cy-radius-2;
    int32_t x1 = cx+radius+2, y1 = cy+radius+2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)img->width)  x1 = img->width  - 1;
    if (y1 >= (int32_t)img->height) y1 = img->height - 1;
    for (int32_t y = y0; y <= y1; y++) {
        for (int32_t x = x0; x <= x1; x++) {
            float dist = sqrtf((float)((x-cx)*(x-cx)+(y-cy)*(y-cy)));
            float diff = fabsf(dist - radius);
            if (diff >= 1.0f) continue;
            float alpha = 1.0f - diff;
            uint8_t tr, tg, tb;
            channel_color(img, x, y, primary_channel, &tr, &tg, &tb);
            blend_pixel(img, x, y, (0xFF<<24)|(tr<<16)|(tg<<8)|tb, alpha);
        }
    }
}

/* ── Transparent (low-alpha) AA circle ───────────────────────────────── */

void draw_circle_aa_alpha(img_t *img, int32_t cx, int32_t cy,
                           int32_t radius, int primary_channel, uint8_t alpha_scale) {
    int32_t x0 = cx-radius-2, y0 = cy-radius-2;
    int32_t x1 = cx+radius+2, y1 = cy+radius+2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)img->width)  x1 = img->width  - 1;
    if (y1 >= (int32_t)img->height) y1 = img->height - 1;
    float ascale = alpha_scale / 255.0f;
    for (int32_t y = y0; y <= y1; y++) {
        for (int32_t x = x0; x <= x1; x++) {
            float dist = sqrtf((float)((x-cx)*(x-cx)+(y-cy)*(y-cy)));
            float diff = fabsf(dist - radius);
            if (diff >= 1.0f) continue;
            float alpha = (1.0f - diff) * ascale;
            uint8_t tr, tg, tb;
            channel_color(img, x, y, primary_channel, &tr, &tg, &tb);
            blend_pixel(img, x, y, (0xFF<<24)|(tr<<16)|(tg<<8)|tb, alpha);
        }
    }
}

/* ── Dashed AA circle ────────────────────────────────────────────────── */

void draw_circle_dashed_aa_dynamic(img_t *img, int32_t cx, int32_t cy,
                                    int32_t radius, int primary_channel) {
    int32_t x0 = cx-radius-2, y0 = cy-radius-2;
    int32_t x1 = cx+radius+2, y1 = cy+radius+2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)img->width)  x1 = img->width  - 1;
    if (y1 >= (int32_t)img->height) y1 = img->height - 1;
    /* 8 dashes: every two octants alternates on/off */
    for (int32_t y = y0; y <= y1; y++) {
        for (int32_t x = x0; x <= x1; x++) {
            float dx = (float)(x - cx), dy = (float)(y - cy);
            float dist = sqrtf(dx*dx + dy*dy);
            float diff = fabsf(dist - radius);
            if (diff >= 1.0f) continue;
            float angle = atan2f(dy, dx); /* -PI..PI */
            if (angle < 0) angle += 6.28318530f;
            /* 48 segments total = 24 dashes */
            int seg = (int)(angle / (6.28318530f / 48.0f));
            if (seg % 2 == 0) continue;
            float alpha = 1.0f - diff;
            uint8_t tr, tg, tb;
            channel_color(img, x, y, primary_channel, &tr, &tg, &tb);
            blend_pixel(img, x, y, (0xFF<<24)|(tr<<16)|(tg<<8)|tb, alpha);
        }
    }
}

/* ── Glow ring (highlight for selected/moving entity) ────────────────── */

void draw_circle_glow(img_t *img, int32_t cx, int32_t cy,
                       int32_t radius, int primary_channel) {
    int32_t x0 = cx-radius-8, y0 = cy-radius-8;
    int32_t x1 = cx+radius+8, y1 = cy+radius+8;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)img->width)  x1 = img->width  - 1;
    if (y1 >= (int32_t)img->height) y1 = img->height - 1;
    for (int32_t y = y0; y <= y1; y++) {
        for (int32_t x = x0; x <= x1; x++) {
            float dist = sqrtf((float)((x-cx)*(x-cx)+(y-cy)*(y-cy)));
            float diff = dist - (float)radius;
            if (diff < 0 || diff > 8.0f) continue;
            float alpha = (1.0f - diff / 8.0f) * 0.5f;
            uint8_t tr, tg, tb;
            channel_color(img, x, y, primary_channel, &tr, &tg, &tb);
            blend_pixel(img, x, y, (0xFF<<24)|(tr<<16)|(tg<<8)|tb, alpha);
        }
    }
}

void draw_circle_glow_color(img_t *img, int32_t cx, int32_t cy,
                             int32_t radius, uint32_t color) {
    int32_t x0 = cx-radius-8, y0 = cy-radius-8;
    int32_t x1 = cx+radius+8, y1 = cy+radius+8;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)img->width)  x1 = img->width  - 1;
    if (y1 >= (int32_t)img->height) y1 = img->height - 1;
    
    float base_alpha = ((color >> 24) & 0xFF) / 255.0f;
    uint32_t rgb = color & 0xFFFFFF; // Strip alpha from color payload since blend_pixel takes 0xFF alpha in color arg
    
    for (int32_t y = y0; y <= y1; y++) {
        for (int32_t x = x0; x <= x1; x++) {
            float dist = sqrtf((float)((x-cx)*(x-cx)+(y-cy)*(y-cy)));
            float diff = fabsf(dist - (float)radius);
            if (diff > 8.0f) continue;
            float alpha = (1.0f - diff / 8.0f) * base_alpha;
            blend_pixel(img, x, y, (0xFF<<24)|rgb, alpha);
        }
    }
}

/* ── Filled circle ───────────────────────────────────────────────────── */

void fill_circle(img_t *img, int32_t cx, int32_t cy,
                 int32_t radius, uint32_t color) {
    int32_t r2 = radius * radius;
    for (int32_t y = cy-radius; y <= cy+radius; y++) {
        for (int32_t x = cx-radius; x <= cx+radius; x++) {
            int32_t dx = x-cx, dy = y-cy;
            if (dx*dx + dy*dy <= r2)
                if (x >= 0 && x < (int32_t)img->width &&
                    y >= 0 && y < (int32_t)img->height)
                    img->data[y*img->width+x] = color;
        }
    }
}

void fill_circle_dynamic(img_t *img, int32_t cx, int32_t cy,
                          int32_t radius, int primary_channel) {
    int32_t r2 = radius * radius;
    for (int32_t y = cy-radius; y <= cy+radius; y++) {
        for (int32_t x = cx-radius; x <= cx+radius; x++) {
            int32_t dx = x-cx, dy = y-cy;
            if (dx*dx + dy*dy > r2) continue;
            if (x < 0 || x >= (int32_t)img->width ||
                y < 0 || y >= (int32_t)img->height) continue;
            uint32_t idx = y*img->width + x;
            uint8_t tr, tg, tb;
            channel_color(img, x, y, primary_channel, &tr, &tg, &tb);
            img->data[idx] = (0xFF<<24)|(tr<<16)|(tg<<8)|tb;
        }
    }
}

void fill_circle_glow(img_t *img, int32_t cx, int32_t cy,
                       int32_t radius, int primary_channel) {
    /* Soft filled dot with radial alpha falloff for highlight */
    int32_t ext = radius + 4;
    for (int32_t y = cy-ext; y <= cy+ext; y++) {
        for (int32_t x = cx-ext; x <= cx+ext; x++) {
            float dist = sqrtf((float)((x-cx)*(x-cx)+(y-cy)*(y-cy)));
            if (dist > (float)ext) continue;
            if (x < 0 || x >= (int32_t)img->width ||
                y < 0 || y >= (int32_t)img->height) continue;
            float alpha = (1.0f - dist / (float)ext) * 0.6f;
            uint8_t tr, tg, tb;
            channel_color(img, x, y, primary_channel, &tr, &tg, &tb);
            blend_pixel(img, x, y, (0xFF<<24)|(tr<<16)|(tg<<8)|tb, alpha);
        }
    }
}

/* ── Rectangle helpers ───────────────────────────────────────────────── */

void draw_rect_fill(img_t *img, int32_t x, int32_t y,
                    int32_t w, int32_t h, uint32_t color) {
    int32_t x0 = (x < 0) ? 0 : x;
    int32_t y0 = (y < 0) ? 0 : y;
    int32_t x1 = (x+w > (int32_t)img->width)  ? (int32_t)img->width  : x+w;
    int32_t y1 = (y+h > (int32_t)img->height) ? (int32_t)img->height : y+h;
    for (int32_t cy = y0; cy < y1; cy++) {
        uint32_t *p = &img->data[cy * img->width + x0];
        for (int32_t cx = x0; cx < x1; cx++) *p++ = color;
    }
}

void draw_rect_blend(img_t *img, int32_t x, int32_t y,
                     int32_t w, int32_t h, uint32_t color) {
    if (x >= (int32_t)img->width  || y >= (int32_t)img->height ||
        x+w <= 0                  || y+h <= 0) return;
    int32_t x0 = (x < 0) ? 0 : x;
    int32_t y0 = (y < 0) ? 0 : y;
    int32_t x1 = (x+w > (int32_t)img->width)  ? (int32_t)img->width  : x+w;
    int32_t y1 = (y+h > (int32_t)img->height) ? (int32_t)img->height : y+h;
    uint32_t sa = (color >> 24) & 0xFF, ia = 256 - sa;
    uint32_t sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    for (int32_t row = y0; row < y1; row++) {
        uint32_t *ptr = &img->data[row * img->width + x0];
        for (int32_t col = x0; col < x1; col++) {
            uint32_t dst = *ptr;
            uint32_t r = (sr*sa + ((dst>>16)&0xFF)*ia) >> 8;
            uint32_t g = (sg*sa + ((dst>> 8)&0xFF)*ia) >> 8;
            uint32_t b = (sb*sa + (dst&0xFF)*ia) >> 8;
            *ptr++ = (0xFF<<24)|(r<<16)|(g<<8)|b;
        }
    }
}

void draw_rect_stroke(img_t *img, int32_t x, int32_t y,
                      int32_t w, int32_t h, uint32_t color) {
    draw_rect_fill(img, x,   y,       w, 1, color);
    draw_rect_fill(img, x,   y+h-1,   w, 1, color);
    draw_rect_fill(img, x,   y,       1, h, color);
    draw_rect_fill(img, x+w-1, y,     1, h, color);
}

/* ── Rounded rectangle ───────────────────────────────────────────────── */

void draw_rounded_rect_blend(img_t *img, int32_t x, int32_t y,
                              int32_t w, int32_t h, int32_t r, uint32_t color) {
    if (r <= 0) { draw_rect_blend(img, x, y, w, h, color); return; }
    int32_t x0 = (x < 0) ? 0 : x;
    int32_t y0 = (y < 0) ? 0 : y;
    int32_t x1 = (x+w > (int32_t)img->width)  ? (int32_t)img->width  : x+w;
    int32_t y1 = (y+h > (int32_t)img->height) ? (int32_t)img->height : y+h;
    uint32_t base_alpha = (color >> 24) & 0xFF;
    for (int32_t row = y0; row < y1; row++) {
        for (int32_t col = x0; col < x1; col++) {
            int in_corner = 0, dx = 0, dy = 0;
            if      (col <  x+r   && row <  y+r)   { dx=x+r-col;       dy=y+r-row;       in_corner=1; }
            else if (col >= x+w-r && row <  y+r)   { dx=col-(x+w-r-1); dy=y+r-row;       in_corner=1; }
            else if (col <  x+r   && row >= y+h-r) { dx=x+r-col;       dy=row-(y+h-r-1); in_corner=1; }
            else if (col >= x+w-r && row >= y+h-r) { dx=col-(x+w-r-1); dy=row-(y+h-r-1); in_corner=1; }
            uint32_t final_alpha = base_alpha;
            if (in_corner) {
                float dist = sqrtf((float)(dx*dx+dy*dy));
                float diff = dist - r;
                if (diff > 1.0f) continue;
                if (diff > -1.0f) {
                    float aa = 0.5f - diff * 0.5f;
                    if (aa < 0) aa = 0;
                    if (aa > 1) aa = 1;
                    final_alpha = (uint32_t)(base_alpha * aa);
                }
            }
            blend_pixel_fast(img, col, row, color, final_alpha);
        }
    }
}

void draw_rounded_rect_fill(img_t *img, int32_t x, int32_t y,
                             int32_t w, int32_t h, int32_t r, uint32_t color) {
    if (r <= 0) { draw_rect_fill(img, x, y, w, h, color); return; }
    draw_rounded_rect_blend(img, x, y, w, h, r, color);
}