#include "app.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRID_DIVISOR    4
#define DEFAULT_WIN_W   1280
#define DEFAULT_WIN_H   720

static void compute_ranges(App *a) {
    /* Define a base scale: how many units the vertical axis covers in landscape */
    const double BASE_Y = 10.0;
    double aspect = (double)a->screen_w / (double)a->screen_h;

    if (aspect >= 1.0) {
        /* Landscape or square: fix vertical, expand horizontal */
        a->y_range[0] = a->cam_cy - (BASE_Y / a->cam_zoom);
        a->y_range[1] = a->cam_cy + (BASE_Y / a->cam_zoom);
        a->x_range[0] = a->cam_cx - (BASE_Y * aspect / a->cam_zoom);
        a->x_range[1] = a->cam_cx + (BASE_Y * aspect / a->cam_zoom);
    } else {
        /* Portrait: fix horizontal, expand vertical */
        a->x_range[0] = a->cam_cx - (BASE_Y / a->cam_zoom);
        a->x_range[1] = a->cam_cx + (BASE_Y / a->cam_zoom);
        a->y_range[0] = a->cam_cy - (BASE_Y / (aspect * a->cam_zoom));
        a->y_range[1] = a->cam_cy + (BASE_Y / (aspect * a->cam_zoom));
    }
}

static void alloc_framebuffers(App *a) {
    free(a->H_img.data);
    free(a->UI_img.data);
    a->H_img.data  = NULL;
    a->UI_img.data = NULL;

    a->grid_w = a->screen_w / GRID_DIVISOR;
    a->grid_h = a->screen_h / GRID_DIVISOR;
    create_img((size_t)a->grid_w,   (size_t)a->grid_h,   &a->H_img);
    create_img((size_t)a->screen_w, (size_t)a->screen_h, &a->UI_img);
    a->UI_img.bg_ref = &a->H_img;
}

static void rebuild_textures(App *a) {
    if (a->H_tex)  SDL_DestroyTexture(a->H_tex);
    if (a->UI_tex) SDL_DestroyTexture(a->UI_tex);

    a->H_tex = SDL_CreateTexture(a->renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  (int)a->H_img.width, (int)a->H_img.height);
    a->UI_tex = SDL_CreateTexture(a->renderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   (int)a->UI_img.width, (int)a->UI_img.height);
    SDL_SetTextureBlendMode(a->UI_tex, SDL_BLENDMODE_BLEND);
}

int app_init(App *a) {
    memset(a, 0, sizeof(*a));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    a->cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    a->cursor_hand  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    a->cursor_move  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);

    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
        fprintf(stderr, "SDL_GetCurrentDisplayMode Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    a->screen_w    = dm.w;
    a->screen_h    = dm.h;
    a->windowed_w  = DEFAULT_WIN_W;
    a->windowed_h  = DEFAULT_WIN_H;
    a->is_fullscreen = 1;

    a->cam_zoom = 1.0;
    a->cam_cx   = 0.0;
    a->cam_cy   = 0.0;

    a->window = SDL_CreateWindow("SINGULARITY",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  a->screen_w, a->screen_h,
                                  SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_RESIZABLE);
    if (!a->window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    a->renderer = SDL_CreateRenderer(a->window, -1, SDL_RENDERER_ACCELERATED);
    if (!a->renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(a->window);
        SDL_Quit();
        return 1;
    }

    compute_ranges(a);
    alloc_framebuffers(a);
    rebuild_textures(a);
    return 0;
}

void app_toggle_fullscreen(App *a) {
    if (a->is_fullscreen) {
        SDL_SetWindowFullscreen(a->window, 0);
        SDL_SetWindowSize(a->window, a->windowed_w, a->windowed_h);
        SDL_SetWindowPosition(a->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        a->is_fullscreen = 0;
    } else {
        SDL_GetWindowSize(a->window, &a->windowed_w, &a->windowed_h);
        SDL_SetWindowFullscreen(a->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        a->is_fullscreen = 1;
    }
}

void app_handle_resize(App *a, int w, int h) {
    if (w <= 0 || h <= 0) return;
    a->screen_w = w;
    a->screen_h = h;
    compute_ranges(a);
    alloc_framebuffers(a);
    rebuild_textures(a);
}

void app_destroy(App *a) {
    free(a->H_img.data);
    free(a->UI_img.data);
    if (a->H_tex)  SDL_DestroyTexture(a->H_tex);
    if (a->UI_tex) SDL_DestroyTexture(a->UI_tex);
    if (a->renderer) SDL_DestroyRenderer(a->renderer);
    if (a->window)   SDL_DestroyWindow(a->window);
    SDL_FreeCursor(a->cursor_arrow);
    SDL_FreeCursor(a->cursor_hand);
    SDL_FreeCursor(a->cursor_move);
    SDL_Quit();
}
