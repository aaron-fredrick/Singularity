#include "ui.h"
#include "font_utils.h"
#include "img_utils.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <complex.h>
#include "audio.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Layout constants ────────────────────────────────────────────────── */
#define POPUP_W          340
#define POPUP_H          320
#define SLIDER_W         300
#define SLIDER_X_OFF     20
#define SLIDER_Y_START   50
#define SLIDER_Y_STEP    45
#define RADIUS_POPUP     12
#define RADIUS_BTN       6
#define RADIUS_HANDLE    6
#define SETTINGS_W       340

/* ═══════════════════════════════════════════════════════════════════════
   Internal helpers
   ═══════════════════════════════════════════════════════════════════════ */

static void clamp_rect(int *px, int *py, int w, int h,
                        int img_w, int img_h) {
    if (*px + w > img_w) *px = img_w - w;
    if (*py + h > img_h) *py = img_h - h;
    if (*px < 0) *px = 0;
    if (*py < 0) *py = 0;
}

/* Slider range for a given index and edit_mode */
static void slider_range(int idx, int edit_mode,
                          double *mn, double *mx, int *is_int) {
    *mn = -5; *mx = 5; *is_int = 0;
    if (edit_mode == 0) {
        if (idx == 4) { *mn = 1; *mx = 10; *is_int = 1; }
    } else {
        if (idx == 0) { *mn = 0;    *mx = 5;   }
        if (idx == 1) { *mn = -180; *mx = 180; }
        if (idx == 2) { *mn = 1;    *mx = 10;  *is_int = 1; }
    }
}

/* Apply slider value at screen-x mx to entity (called from both click & drag) */
static void apply_slider(singularity_t *e, int idx, int mx, int popup_x,
                          int edit_mode) {
    double mn, mx_v; int is_int;
    slider_range(idx, edit_mode, &mn, &mx_v, &is_int);

    int rx = popup_x + SLIDER_X_OFF;
    double t = (double)(mx - rx) / (double)SLIDER_W;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    double val = mn + t * (mx_v - mn);
    if (is_int) val = (double)(int)(val + 0.5);

    if (edit_mode == 0) {
        if (idx == 0) e->m = val + I * cimag(e->m);
        if (idx == 1) e->m = creal(e->m) + I * val;
        if (idx == 2) e->c = val + I * cimag(e->c);
        if (idx == 3) e->c = creal(e->c) + I * val;
        if (idx == 4) e->e = (uint8_t)val;
    } else {
        double complex ratio = (cabs(e->m) > 1e-6) ? (-e->c / e->m) : 0;
        double cur_e   = (double)e->e;
        double cur_R   = pow(cabs(ratio), 1.0 / cur_e);
        double cur_Ang = carg(ratio) * 180.0 / M_PI;
        if (idx == 0) cur_R   = val;
        if (idx == 1) cur_Ang = val;
        if (idx == 2) cur_e   = val;
        e->m = 1.0 + 0.0 * I;
        e->e = (uint8_t)cur_e;
        e->c = -pow(cur_R, cur_e) * cexp(I * cur_Ang * M_PI / 180.0);
    }
}

/* ── Single slider widget ─────────────────────────────────────────────── */
static void draw_slider_widget(img_t *img, int rx, int ry,
                                const char *label, double val,
                                double mn, double mx, int is_int) {
    char buf[64];
    if (is_int) snprintf(buf, sizeof(buf), "%s: %d",   label, (int)val);
    else        snprintf(buf, sizeof(buf), "%s: %.2f", label, val);
    draw_string_colored(img, buf, rx, ry - 14, 0xFFE0E0E0);

    int ty = ry + 6;
    draw_rounded_rect_fill(img, rx, ty, SLIDER_W, 4, 2, 0xFF404040);

    double t = (mx > mn) ? (val - mn) / (mx - mn) : 0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int hx = rx + (int)(t * (SLIDER_W - 12));
    draw_rounded_rect_fill(img, hx, ty - 4, 12, 14, RADIUS_HANDLE, 0xFFC0C0C0);
}

/* ═══════════════════════════════════════════════════════════════════════
   Grid
   ═══════════════════════════════════════════════════════════════════════ */
void ui_draw_grid(img_t *img, double x0, double x1, double y0, double y1) {
    double dx = x1 - x0, dy = y1 - y0;
    double sx = img->width  / dx;
    double sy = img->height / dy;

    int32_t cx = (int32_t)((0 - x0) * sx);
    int32_t cy = (int32_t)((0 - y0) * sy);

    for (int i = (int)ceil(x0); i <= (int)floor(x1); i++) {
        if (i == 0) continue;
        int32_t x = (int32_t)((i - x0) * sx);
        draw_rect_blend(img, x, 0, 1, (int32_t)img->height, 0x18FFFFFF);
    }
    for (int j = (int)ceil(y0); j <= (int)floor(y1); j++) {
        if (j == 0) continue;
        int32_t y = (int32_t)((j - y0) * sy);
        draw_rect_blend(img, 0, y, (int32_t)img->width, 1, 0x18FFFFFF);
    }
    if (cx >= 0 && cx < (int32_t)img->width)
        draw_rect_fill(img, cx - 1, 0, 2, (int32_t)img->height, 0x60FFFFFF);
    if (cy >= 0 && cy < (int32_t)img->height)
        draw_rect_fill(img, 0, cy - 1, (int32_t)img->width, 2, 0x60FFFFFF);
}

/* ═══════════════════════════════════════════════════════════════════════
   Entities
   ═══════════════════════════════════════════════════════════════════════ */
void ui_draw_entities(img_t *img, Simulation *sim,
                      double x_range[2], double y_range[2],
                      singularity_t *selected, singularity_t *move_target,
                      UIFlags flags) {
    if (!flags.hud_visible || flags.circle_mode == CIRCLE_NONE) return;

    double dx = x_range[1] - x_range[0];
    double dy = y_range[1] - y_range[0];
    double sx = img->width  / dx;
    double sy = img->height / dy;

    singularity_array_t *arrs[2]  = {&sim->zeros, &sim->poles};
    int                  types[2] = {3,            1};  /* channel: 3=blue(zero), 1=red(pole) */

    for (int k = 0; k < 2; k++) {
        for (size_t i = 0; i < arrs[k]->size; i++) {
            singularity_t *e = &arrs[k]->data[i];
            int32_t cx = (int32_t)((creal(e->val) - x_range[0]) * sx);
            int32_t cy = (int32_t)((cimag(e->val) - y_range[0]) * sy);

            double math_R = (cabs(e->m) > 1e-6)
                          ? pow(cabs(e->c / e->m), 1.0 / e->e) : 0;
            int32_t radius = (int32_t)(math_R * sx);
            if (radius < 2) radius = 2;

            int ch = types[k];
            int is_highlighted = (e == selected || e == move_target);

            /* Highlight ring (outer glow) */
            if (is_highlighted) {
                draw_circle_glow(img, cx, cy, radius + 4, ch);
                fill_circle_glow(img, cx, cy, 4, ch);
            }

            /* Circle itself */
            switch (flags.circle_mode) {
                case CIRCLE_SOLID:
                    draw_circle_aa_dynamic(img, cx, cy, radius, ch);
                    fill_circle_dynamic(img, cx, cy, 3, ch);
                    break;
                case CIRCLE_TRANSPARENT:
                    draw_circle_aa_alpha(img, cx, cy, radius, ch, 80);
                    fill_circle_dynamic(img, cx, cy, 3, ch);
                    break;
                case CIRCLE_DASHED:
                    draw_circle_dashed_aa_dynamic(img, cx, cy, radius, ch);
                    fill_circle_dynamic(img, cx, cy, 3, ch);
                    break;
                case CIRCLE_DOT_ONLY:
                    fill_circle_dynamic(img, cx, cy, 4, ch);
                    break;
                default:
                    break;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Tooltip
   ═══════════════════════════════════════════════════════════════════════ */
void ui_draw_tooltip(img_t *img, int mx, int my, singularity_t *s, int type) {
    int w = 240, h = 100;
    int x = mx + 20, y = my - 10;
    if (x + w > (int)img->width)  x = mx - w - 20;
    if (y + h > (int)img->height) y = (int)img->height - h;
    if (y < 0) y = 0;

    draw_rounded_rect_blend(img, x, y, w, h, RADIUS_POPUP, 0xE0101010);

    char buf[64];
    const char *typestr = (type == 1) ? "Zero" : "Pole";
    uint32_t tc = (type == 1) ? 0xFF9090FF : 0xFFFF9090;
    draw_string_colored(img, typestr, x + 12, y + 10, tc);
    snprintf(buf, sizeof(buf), "Pos: %.2f%+.2fi", creal(s->val), cimag(s->val));
    draw_string_colored(img, buf, x + 12, y + 30, 0xFFCCCCCC);
    snprintf(buf, sizeof(buf), "e: %d", s->e);
    draw_string_colored(img, buf, x + 12, y + 48, 0xFFCCCCCC);
    snprintf(buf, sizeof(buf), "m: %.1f%+.1fi", creal(s->m), cimag(s->m));
    draw_string_colored(img, buf, x + 12, y + 66, 0xFFFFA0A0);
    snprintf(buf, sizeof(buf), "c: %.1f%+.1fi", creal(s->c), cimag(s->c));
    draw_string_colored(img, buf, x + 130, y + 66, 0xFFA0A0FF);
}

/* ═══════════════════════════════════════════════════════════════════════
   Popup panel
   ═══════════════════════════════════════════════════════════════════════ */
void ui_draw_popup(img_t *img, Popup *p) {
    if (!p->is_open || !p->entity) return;

    int px = p->x, py = p->y;
    clamp_rect(&px, &py, POPUP_W, POPUP_H, (int)img->width, (int)img->height);

    draw_rounded_rect_blend(img, px, py, POPUP_W, POPUP_H, RADIUS_POPUP, 0xE0181818);

    uint32_t hcol = (p->entity_type == 1) ? 0xFF005090 : 0xFF900000;
    draw_rounded_rect_fill(img, px+2, py+2, POPUP_W-4, 32, RADIUS_POPUP-2, hcol|0xFF000000);
    const char *title = (p->entity_type == 1) ? "ZERO" : "POLE";
    draw_string_colored(img, title, px + 12, py + 10, 0xFFFFFFFF);

    int btn_x = px + POPUP_W - 100, btn_y = py + 6;
    draw_rounded_rect_fill(img, btn_x, btn_y, 80, 24, RADIUS_BTN, 0xFF303030);
    draw_string_colored(img, (p->edit_mode == 0) ? "CART" : "POLAR",
                        btn_x + 10, btn_y + 8, 0xFFCCCCCC);

    int y_off = SLIDER_Y_START;
    singularity_t *e = p->entity;

    if (p->edit_mode == 0) {
        draw_slider_widget(img, px+SLIDER_X_OFF, py+y_off, "m.Re", creal(e->m), -5, 5, 0); y_off+=SLIDER_Y_STEP;
        draw_slider_widget(img, px+SLIDER_X_OFF, py+y_off, "m.Im", cimag(e->m), -5, 5, 0); y_off+=SLIDER_Y_STEP;
        draw_slider_widget(img, px+SLIDER_X_OFF, py+y_off, "c.Re", creal(e->c), -5, 5, 0); y_off+=SLIDER_Y_STEP;
        draw_slider_widget(img, px+SLIDER_X_OFF, py+y_off, "c.Im", cimag(e->c), -5, 5, 0); y_off+=SLIDER_Y_STEP;
        draw_slider_widget(img, px+SLIDER_X_OFF, py+y_off, "Exp",  (double)e->e, 1, 10, 1);
    } else {
        double complex ratio = (cabs(e->m) > 1e-6) ? (-e->c / e->m) : 0;
        double cur_e   = (double)e->e;
        double cur_R   = pow(cabs(ratio), 1.0 / cur_e);
        double cur_Ang = carg(ratio) * 180.0 / M_PI;
        draw_slider_widget(img, px+SLIDER_X_OFF, py+y_off, "Rad", cur_R,   0,    5,   0); y_off+=SLIDER_Y_STEP;
        draw_slider_widget(img, px+SLIDER_X_OFF, py+y_off, "Rot", cur_Ang, -180, 180, 0); y_off+=SLIDER_Y_STEP;
        draw_slider_widget(img, px+SLIDER_X_OFF, py+y_off, "Exp", cur_e,   1,    10,  1);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   HUD overlay
   ═══════════════════════════════════════════════════════════════════════ */
static const char *circle_mode_name(CircleMode m) {
    switch (m) {
        case CIRCLE_SOLID:       return "Solid";
        case CIRCLE_TRANSPARENT: return "Transparent";
        case CIRCLE_DASHED:      return "Dashed";
        case CIRCLE_DOT_ONLY:    return "Dot";
        case CIRCLE_NONE:        return "None";
        default:                 return "?";
    }
}

void ui_draw_hud(img_t *img, float fps, const RenderCfg *cfg,
                 const UIFlags *flags, int cursor_mode, int is_fullscreen) {
    if (!flags->hud_visible) return;

    char buf[80];
    snprintf(buf, sizeof(buf), "FPS: %.1f", fps);
    draw_string_dynamic(img, buf, 10, 10, 2);

    static const char *nmap_names[] = {"Linear", "Log", "Steps"};
    snprintf(buf, sizeof(buf), "Norm: %s", nmap_names[cfg->n_map]);
    draw_string_dynamic(img, buf, 10, 22, 2);

    if (cfg->n_map == 2) {
        snprintf(buf, sizeof(buf), "Steps: %d", cfg->steps);
        draw_string_dynamic(img, buf, 10, 34, 2);
    }
    snprintf(buf, sizeof(buf), "CMap: %d | %s | %s",
             cfg->c_map,
             cursor_mode == 0 ? "SELECT" : "MOVE",
             is_fullscreen ? "FS" : "WIN");
    draw_string_dynamic(img, buf, 10, 46, 2);

    snprintf(buf, sizeof(buf), "Circle: %s  [H=HUD C=Circle S=Set]",
             circle_mode_name(flags->circle_mode));
    draw_string_dynamic(img, buf, 10, 58, 2);

    snprintf(buf, sizeof(buf), "[P=Pic O=Orb U=Pls F=Fld D=Dom A=Aud 1-9=Pre]");
    draw_string_dynamic(img, buf, 10, 70, 2);

    if (flags->audio_enabled) {
        double freq, amp;
        audio_get_params(&freq, &amp);
        snprintf(buf, sizeof(buf), "Audio: %.1f Hz | Amp: %.2f", freq, amp);
        draw_string_dynamic(img, buf, 10, 82, 2);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Settings panel
   ═══════════════════════════════════════════════════════════════════════ */
static const int SETTINGS_H = 380;

void ui_draw_settings(img_t *img, SettingsPanel *sp,
                      RenderCfg *cfg, UIFlags *flags,
                      int screen_w, int screen_h, int is_fullscreen,
                      int *request_fs_toggle) {
    (void)request_fs_toggle;
    if (!sp->is_open) return;

    int px = (screen_w - SETTINGS_W) / 2;
    int py = (screen_h - SETTINGS_H) / 2;
    clamp_rect(&px, &py, SETTINGS_W, SETTINGS_H, (int)img->width, (int)img->height);

    draw_rounded_rect_blend(img, px, py, SETTINGS_W, SETTINGS_H, RADIUS_POPUP, 0xF0141414);
    draw_rounded_rect_fill (img, px+2, py+2, SETTINGS_W-4, 30, RADIUS_POPUP-2, 0xFF202060);
    draw_string_colored(img, "SETTINGS", px+12, py+10, 0xFFFFFFFF);

    /* Close button */
    draw_rounded_rect_fill(img, px+SETTINGS_W-36, py+6, 28, 22, 4, 0xFF602020);
    draw_string_colored(img, "X", px+SETTINGS_W-26, py+12, 0xFFFFFFFF);

    int y = py + 44;
    char buf[80];

    /* Window section */
    draw_string_colored(img, "-- Window --", px+12, y, 0xFF8888CC); y += 14;
    snprintf(buf, sizeof(buf), "Resolution: %d x %d", screen_w, screen_h);
    draw_string_colored(img, buf, px+12, y, 0xFFCCCCCC); y += 14;
    snprintf(buf, sizeof(buf), "Mode: %s", is_fullscreen ? "Fullscreen" : "Windowed");
    draw_string_colored(img, buf, px+12, y, 0xFFCCCCCC); y += 16;
    draw_rounded_rect_fill(img, px+12, y, 120, 22, 4, 0xFF304060);
    draw_string_colored(img, "Toggle FS [Alt+Enter]", px+16, y+7, 0xFFAAAAFF); y += 32;

    /* Render section */
    draw_string_colored(img, "-- Rendering --", px+12, y, 0xFF88CC88); y += 14;

    /* Colour map row */
    draw_string_colored(img, "Colour Map:", px+12, y, 0xFFCCCCCC);
    for (int i = 0; i < 6; i++) {
        int bx = px + 12 + i * 42;
        uint32_t bc = (i == cfg->c_map) ? 0xFF506080 : 0xFF303030;
        draw_rounded_rect_fill(img, bx, y+12, 36, 18, 3, bc);
        snprintf(buf, sizeof(buf), "%d", i);
        draw_string_colored(img, buf, bx+14, y+17, 0xFFCCCCCC);
    }
    y += 36;

    /* Normalisation row */
    draw_string_colored(img, "Normalise:", px+12, y, 0xFFCCCCCC);
    static const char *nm[] = {"Lin","Log","Stp"};
    for (int i = 0; i < 3; i++) {
        int bx = px + 12 + i * 60;
        uint32_t bc = (i == cfg->n_map) ? 0xFF506080 : 0xFF303030;
        draw_rounded_rect_fill(img, bx, y+12, 52, 18, 3, bc);
        draw_string_colored(img, nm[i], bx+14, y+17, 0xFFCCCCCC);
    }
    y += 36;

    /* Steps slider */
    draw_slider_widget(img, px+SLIDER_X_OFF, y+14, "Steps", (double)cfg->steps, 1, 64, 1);
    y += 40;

    /* Circle mode row */
    draw_string_colored(img, "Entity Style:", px+12, y, 0xFFCCCCCC);
    static const char *cm[] = {"Sol","Trn","Dsh","Dot","None"};
    for (int i = 0; i < CIRCLE_MODE_COUNT; i++) {
        int bx = px + 12 + i * 56;
        uint32_t bc = (i == (int)flags->circle_mode) ? 0xFF506080 : 0xFF303030;
        draw_rounded_rect_fill(img, bx, y+12, 50, 18, 3, bc);
        draw_string_colored(img, cm[i], bx+10, y+17, 0xFFCCCCCC);
    }
    y += 36;

    /* HUD toggle */
    uint32_t hud_bc = flags->hud_visible ? 0xFF304050 : 0xFF502020;
    draw_rounded_rect_fill(img, px+12, y, 100, 22, 4, hud_bc);
    draw_string_colored(img, flags->hud_visible ? "HUD: ON [H]" : "HUD: OFF [H]",
                        px+16, y+7, 0xFFCCCCCC);
}

/* ═══════════════════════════════════════════════════════════════════════
   Popup interaction
   ═══════════════════════════════════════════════════════════════════════ */
int popup_hit_slider(const Popup *p, int mx, int my, int *out_idx) {
    if (!p->is_open || !p->entity) return 0;
    int px = p->x, py = p->y;
    int max_s = (p->edit_mode == 0) ? 5 : 3;
    for (int idx = 0; idx < max_s; idx++) {
        int y_off = SLIDER_Y_START + idx * SLIDER_Y_STEP;
        int ry = py + y_off - 14;
        int rx = px + SLIDER_X_OFF;
        if (mx >= rx && mx <= rx + SLIDER_W && my >= ry && my <= ry + 30) {
            *out_idx = idx;
            return 1;
        }
    }
    return 0;
}

int popup_hit_mode_btn(const Popup *p, int mx, int my) {
    if (!p->is_open) return 0;
    int btn_x = p->x + POPUP_W - 100, btn_y = p->y + 6;
    return (mx >= btn_x && mx <= btn_x + 80 && my >= btn_y && my <= btn_y + 24);
}

void popup_apply_slider(Popup *p, int mx) {
    if (!p->entity || p->dragging_slider < 0) return;
    apply_slider(p->entity, p->dragging_slider, mx, p->x, p->edit_mode);
}

/* ═══════════════════════════════════════════════════════════════════════
   Settings interaction
   ═══════════════════════════════════════════════════════════════════════ */
int settings_hit(SettingsPanel *sp, RenderCfg *cfg, UIFlags *flags,
                 int mx, int my, int screen_w, int screen_h,
                 int is_fullscreen, int *cmap_delta, int *nmap_delta) {
    (void)is_fullscreen;
    if (!sp->is_open) return SETTINGS_HIT_NONE;
    *cmap_delta = 0; *nmap_delta = 0;

    int px = (screen_w - SETTINGS_W) / 2;
    int py = (screen_h - SETTINGS_H) / 2;
    if (px < 0) px = 0;
    if (py < 0) py = 0;

    /* Close button */
    if (mx >= px+SETTINGS_W-36 && mx <= px+SETTINGS_W-8 &&
        my >= py+6 && my <= py+28) {
        sp->is_open = 0;
        return SETTINGS_HIT_CLOSE;
    }

    int y = py + 44;

    /* Window section */
    y += 14; /* label */
    y += 14; /* resolution text */
    y += 14; /* mode text */
    /* Fullscreen toggle button */
    if (mx >= px+12 && mx <= px+132 && my >= y && my <= y+22)
        return SETTINGS_HIT_FS;
    y += 32;

    /* Render section */
    y += 14; /* label */

    /* Colour map buttons */
    for (int i = 0; i < 6; i++) {
        int bx = px + 12 + i * 42;
        if (mx >= bx && mx <= bx+36 && my >= y+12 && my <= y+30) {
            *cmap_delta = i - cfg->c_map;
            cfg->c_map = i;
            return SETTINGS_HIT_CMAP;
        }
    }
    y += 36;

    /* Normalisation buttons */
    for (int i = 0; i < 3; i++) {
        int bx = px + 12 + i * 60;
        if (mx >= bx && mx <= bx+52 && my >= y+12 && my <= y+30) {
            *nmap_delta = i - cfg->n_map;
            cfg->n_map = i;
            return SETTINGS_HIT_NMAP;
        }
    }
    y += 36;

    /* Steps slider */
    int srx = px + SLIDER_X_OFF;
    if (mx >= srx && mx <= srx + SLIDER_W && my >= y && my <= y+30) {
        double t = (double)(mx - srx) / SLIDER_W;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        cfg->steps = 1 + (int)(t * 63 + 0.5);
        sp->drag_steps = 1;
        return SETTINGS_HIT_STEPS;
    }
    y += 40;

    /* Circle mode buttons */
    for (int i = 0; i < CIRCLE_MODE_COUNT; i++) {
        int bx = px + 12 + i * 56;
        if (mx >= bx && mx <= bx+50 && my >= y+12 && my <= y+30) {
            flags->circle_mode = (CircleMode)i;
            return SETTINGS_HIT_NONE;
        }
    }
    y += 36;

    /* HUD toggle button */
    if (mx >= px+12 && mx <= px+112 && my >= y && my <= y+22) {
        flags->hud_visible = !flags->hud_visible;
        return SETTINGS_HIT_NONE;
    }

    return SETTINGS_HIT_NONE;
}

/* ── Vector Field / Phase Trails ─────────────────────────────────────── */
void ui_draw_field_lines(img_t *img, Simulation *sim, double x_range[2], double y_range[2]) {
    int lines_x = 40;
    int lines_y = 30;
    
    double step_size = (x_range[1] - x_range[0]) / img->width;
    int max_steps = 150;
    
    for (int iy = 0; iy < lines_y; iy++) {
        for (int ix = 0; ix < lines_x; ix++) {
            double rx = x_range[0] + (x_range[1] - x_range[0]) * ((ix + 0.5) / lines_x);
            double ry = y_range[0] + (y_range[1] - y_range[0]) * ((iy + 0.5) / lines_y);
            double complex pos = rx + ry * I;
            
            for (int step = 0; step < max_steps; step++) {
                double complex H = sim_eval_H(sim, pos);
                double mag = cabs(H);
                if (mag < 1e-6 || mag > 1e6) break;
                
                double complex dir = H / mag;
                
                double complex next_pos = pos + dir * step_size;
                
                uint32_t px, py;
                complex_to_screen_choords(pos, &px, &py, x_range, y_range, img->width, img->height);
                
                if (px < img->width && py < img->height) {
                    uint32_t idx = py * img->width + px;
                    uint32_t bg = img->data[idx];
                    uint32_t br = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bb = bg & 0xFF;
                    uint32_t a = 64; 
                    uint32_t ia = 256 - a;
                    img->data[idx] = (0xFF << 24) |
                                     (((255 * a + br * ia) >> 8) << 16) |
                                     (((255 * a + bg_g * ia) >> 8) << 8) |
                                     ((255 * a + bb * ia) >> 8);
                }
                
                pos = next_pos;
            }
        }
    }
}

/* ── Domain Coloring ─────────────────────────────────────────────────── */
void ui_draw_domain_overlay(img_t *img, Simulation *sim,
                            double x_range[2], double y_range[2]) {
    double dx = (x_range[1] - x_range[0]) / img->width;
    double dy = (y_range[1] - y_range[0]) / img->height;
    
    int step = 2; // evaluate every 2x2 pixels for performance
    
    for (int y = 0; y < img->height; y += step) {
        double im = y_range[0] + y * dy;
        for (int x = 0; x < img->width; x += step) {
            double re = x_range[0] + x * dx;
            double complex H = sim_eval_H(sim, re + im * I);
            
            double u = creal(H);
            double v = cimag(H);
            
            double warp_scale = 5.0; 
            int grid_u = (int)floor(u * warp_scale);
            int grid_v = (int)floor(v * warp_scale);
            
            if ((grid_u + grid_v) % 2 == 0) continue; 
            
            uint32_t a = 32; 
            uint32_t ia = 256 - a;
            for (int dy_bl = 0; dy_bl < step && y+dy_bl < img->height; dy_bl++) {
                for (int dx_bl = 0; dx_bl < step && x+dx_bl < img->width; dx_bl++) {
                    uint32_t idx = (y+dy_bl) * img->width + (x+dx_bl);
                    uint32_t bg = img->data[idx];
                    uint32_t br = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bb = bg & 0xFF;
                    img->data[idx] = (0xFF << 24) |
                                     (((0 * a + br * ia) >> 8) << 16) |
                                     (((0 * a + bg_g * ia) >> 8) << 8) |
                                     ((0 * a + bb * ia) >> 8);
                }
            }
        }
    }
}
