#ifndef UI_H
#define UI_H

#include "img_utils.h"
#include "transfer_function.h"
#include "simulation.h"

/* ── Circle display mode (C key cycles) ─────────────────────────────── */
typedef enum {
    CIRCLE_SOLID       = 0, /* filled dot + solid AA circle        */
    CIRCLE_TRANSPARENT = 1, /* filled dot + low-alpha circle       */
    CIRCLE_DASHED      = 2, /* filled dot + dashed circle          */
    CIRCLE_DOT_ONLY    = 3, /* filled dot, no circle               */
    CIRCLE_NONE        = 4, /* nothing drawn                       */
    CIRCLE_MODE_COUNT  = 5
} CircleMode;

/* ── Runtime display flags ───────────────────────────────────────────── */
typedef struct {
    int        hud_visible;  /* H key: grid + FPS + entities visible */
    CircleMode circle_mode;  /* C key: entity rendering style        */
    int        screenshot_queued; /* 1 if screenshot requested */
    int        orbit_mode;   /* O key: auto rotate entities   */
    int        pulse_mode;   /* U key: pulse entity magnitude */
    int        fieldlines_visible; /* F key: draw phase trails */
    int        domain_overlay;   /* D key: domain coloring grid */
    int        audio_enabled;    /* A key: sonification */
} UIFlags;

/* ── Property popup panel ────────────────────────────────────────────── */
typedef struct {
    int            is_open;
    int            x, y;
    singularity_t *entity;
    int            entity_type;     /* 1=zero, 2=pole */
    int            edit_mode;       /* 0=cartesian, 1=polar */
    int            dragging_slider; /* -1 = none */
} Popup;

/* ── Settings panel ──────────────────────────────────────────────────── */
typedef struct {
    int is_open;
    int drag_steps;  /* -1 = not dragging */
} SettingsPanel;

/* ── Render config ───────────────────────────────────────────────────── */
typedef struct {
    int c_map;   /* colour map 0-5          */
    int n_map;   /* normalisation mode 0-2  */
    int steps;   /* step count for log-steps */
} RenderCfg;

/* ── Drawing functions ───────────────────────────────────────────────── */

void ui_draw_grid(img_t *img,
                  double x0, double x1, double y0, double y1);

/* Draw all entities; selected/move_target get a highlight ring */
void ui_draw_entities(img_t *img,
                      Simulation *sim,
                      double x_range[2], double y_range[2],
                      singularity_t *selected,
                      singularity_t *move_target,
                      UIFlags flags);

void ui_draw_tooltip(img_t *img, int mx, int my,
                     singularity_t *s, int type);

void ui_draw_popup(img_t *img, Popup *p);

void ui_draw_hud(img_t *img, float fps, const RenderCfg *cfg,
                 const UIFlags *flags, int cursor_mode, int is_fullscreen);

/* settings_toggle_fs: if non-NULL and user presses fs button, set *=1 */
void ui_draw_settings(img_t *img, SettingsPanel *sp,
                      RenderCfg *cfg, UIFlags *flags,
                      int screen_w, int screen_h, int is_fullscreen,
                      int *request_fs_toggle);

/* ── Popup interaction ───────────────────────────────────────────────── */

/* Returns 1 and sets *out_idx if (mx,my) hits a slider area */
int  popup_hit_slider(const Popup *p, int mx, int my, int *out_idx);
/* Returns 1 if (mx,my) hits the mode toggle button */
int  popup_hit_mode_btn(const Popup *p, int mx, int my);
/* Apply slider drag at screen-x mx to entity */
void popup_apply_slider(Popup *p, int mx);

/* ── Settings interaction ────────────────────────────────────────────── */
/* Returns action flags (bitmask) for what settings widget was clicked */
#define SETTINGS_HIT_NONE     0
#define SETTINGS_HIT_FS       (1<<0)
#define SETTINGS_HIT_CMAP     (1<<1)
#define SETTINGS_HIT_NMAP     (1<<2)
#define SETTINGS_HIT_STEPS    (1<<3)
#define SETTINGS_HIT_CLOSE    (1<<4)
int settings_hit(SettingsPanel *sp, RenderCfg *cfg, UIFlags *flags,
                 int mx, int my, int screen_w, int screen_h,
                 int is_fullscreen, int *cmap_delta, int *nmap_delta);

/* ── Vector Field / Phase Trails ─────────────────────────────────────── */
void ui_draw_field_lines(img_t *img, Simulation *sim, 
                         double x_range[2], double y_range[2]);

/* ── Domain Coloring ─────────────────────────────────────────────────── */
void ui_draw_domain_overlay(img_t *img, Simulation *sim,
                            double x_range[2], double y_range[2]);

#endif /* UI_H */
