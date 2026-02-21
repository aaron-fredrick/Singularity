#ifndef APP_H
#define APP_H

#include <SDL.h>
#include "img_utils.h"

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *H_tex;
    SDL_Texture  *UI_tex;
    img_t         H_img;
    img_t         UI_img;

    int screen_w, screen_h;   /* current window pixel dimensions */
    int grid_w,   grid_h;     /* low-res math grid (screen / GRID_DIVISOR) */
    int is_fullscreen;
    int windowed_w, windowed_h; /* saved windowed size for toggle */

    double x_range[2];  /* [x_min, x_max] */
    double y_range[2];  /* [y_min, y_max] */

    /* Camera for zoom/pan */
    double cam_cx, cam_cy; /* center in complex space */
    double cam_zoom;       /* multiplier: >1 = zoomed in */

    SDL_Cursor *cursor_arrow;
    SDL_Cursor *cursor_hand;
    SDL_Cursor *cursor_move;
} App;

/* Returns 0 on success, non-zero on failure */
int  app_init(App *a);
void app_toggle_fullscreen(App *a);
void app_handle_resize(App *a, int w, int h);
void app_destroy(App *a);

#endif /* APP_H */
