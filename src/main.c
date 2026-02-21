/*
 * SINGULARITY — main.c (orchestrator)
 *
 * Key shortcuts:
 *   Alt+Enter   Fullscreen / windowed toggle
 *   Escape      Quit
 *   G           Toggle Select ↔ Move cursor mode
 *   Z           (Move mode) Add zero at mouse position
 *   X           (Move mode) Add pole at mouse position
 *   Delete      Delete selected entity
 *   Ctrl+Z      Undo
 *   Ctrl+Y      Redo  (also Ctrl+Shift+Z)
 *   Ctrl+R      Randomise all
 *   Tab         Cycle colour map (0-5)
 *   Up/Down     Increase / decrease step count
 *   H           Toggle HUD visibility
 *   C           Cycle circle display mode
 *   S / F1      Open / close settings panel
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "app.h"
#include "simulation.h"
#include "transfer_function.h"
#include "ui.h"
#include "utils.h"
#include "presets.h"
#include "audio.h"
#include "undo.h"
#include "ui.h"
#include "transfer_function.h"
#include "utils.h"
#include "presets.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TARGET_FPS          60
#define FRAME_DURATION_MS   (1000 / TARGET_FPS)

/* ── Helpers ─────────────────────────────────────────────────────────── */

static void randomise_sim(Simulation *sim, App *app) {
    sim_randomise(sim, app->x_range, app->y_range);
}

/* Invalidate any pointer-to-entity after array changes */
static void clear_selection(Popup *popup, singularity_t **move_target, int *selected_count) {
    popup->is_open     = 0;
    popup->entity      = NULL;
    popup->dragging_slider = -1;
    *move_target       = NULL;
    *selected_count    = 0;
}

static void do_resize(App *app, Simulation *sim, int w, int h) {
    app_handle_resize(app, w, h);
    sim_rebuild_grid(sim, app->x_range, app->y_range, app->grid_w, app->grid_h);
}

static void save_screenshot(App *app) {
    static int shot_counter = 1;
    char filename[64];
    snprintf(filename, sizeof(filename), "screenshot_%04d.bmp", shot_counter++);
    
    SDL_Surface *sshot = SDL_CreateRGBSurfaceWithFormat(0, app->screen_w, app->screen_h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (sshot) {
        SDL_RenderReadPixels(app->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, sshot->pixels, sshot->pitch);
        SDL_SaveBMP(sshot, filename);
        SDL_FreeSurface(sshot);
        printf("Saved %s\n", filename);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   main
   ═══════════════════════════════════════════════════════════════════════ */
int main(int argc, char **argv) {
    (void)argc; (void)argv;

    App        app;
    Simulation sim;
    UndoStack  undo;

    /* Runtime state */
    UIFlags      flags     = { .hud_visible = 1, .circle_mode = CIRCLE_SOLID };
    RenderCfg    rcfg      = { .c_map = 0, .n_map = 1, .steps = 5 };
    Popup        popup     = { .is_open = 0, .dragging_slider = -1 };
    SettingsPanel settings = { .is_open = 0, .drag_steps = -1 };

    singularity_t *move_target   = NULL;
    singularity_t *hover_entity  = NULL;
    int            hover_type    = 0;
    int            cursor_mode   = 0; /* 0=select 1=move */

    /* FPS */
    uint32_t frame_count    = 0;
    uint32_t last_fps_time  = 0;
    float    current_fps    = 0.0f;

    /* Selection Box & Group Move State */
    int is_selecting = 0;
    int sel_sx = 0, sel_sy = 0;
    int sel_ex = 0, sel_ey = 0;

    /* We track which entities are 'selected'. Note: size_t since indices can change on delete */
    #define MAX_SELECTED 128
    singularity_t* selected_group[MAX_SELECTED];
    int selected_count = 0;

    /* Animation state */
    double orbit_angle = 0.0;
    double pulse_t     = 0.0;

    /* Init SDL, window, renderer */
    srand((unsigned int)time(NULL));
    if (app_init(&app) != 0) return 1;
    audio_init();

    sim_init(&sim, app.x_range, app.y_range, app.grid_w, app.grid_h);
    undo_init(&undo);
    undo_push(&undo, &sim.zeros, &sim.poles); /* initial snapshot */

    int running = 1;
    SDL_Event e;

    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        int mx, my;
        SDL_GetMouseState(&mx, &my);

        /* ── Event loop ──────────────────────────────────────────────── */
        while (SDL_PollEvent(&e)) {

            if (e.type == SDL_MOUSEMOTION) {
                /* No need to update local mx,my here as we pull at start of frame */
            } else if (e.type == SDL_MOUSEBUTTONDOWN ||
                       e.type == SDL_MOUSEBUTTONUP) {
                mx = e.button.x; my = e.button.y;
            }

            /* ── Window events ───────────────────────────────────────── */
            if (e.type == SDL_QUIT) {
                running = 0;

            } else if (e.type == SDL_WINDOWEVENT &&
                       e.window.event == SDL_WINDOWEVENT_RESIZED) {
                do_resize(&app, &sim, e.window.data1, e.window.data2);
                clear_selection(&popup, &move_target, &selected_count);

            /* ── Keyboard ────────────────────────────────────────────── */
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key  = e.key.keysym.sym;
                SDL_Keymod  mods = SDL_GetModState();
                int ctrl  = (mods & KMOD_CTRL)  != 0;
                int shift = (mods & KMOD_SHIFT) != 0;
                int alt   = (mods & KMOD_ALT)   != 0;

                if (key == SDLK_ESCAPE) {
                    running = 0;

                /* Alt+Enter — fullscreen toggle */
                } else if (key == SDLK_RETURN && alt) {
                    app_toggle_fullscreen(&app);
                    int w, h;
                    SDL_GetWindowSize(app.window, &w, &h);
                    do_resize(&app, &sim, w, h);
                    clear_selection(&popup, &move_target, &selected_count);

                /* Ctrl+Z — undo */
                } else if (key == SDLK_z && ctrl && !shift) {
                    if (undo_undo(&undo, &sim.zeros, &sim.poles))
                        clear_selection(&popup, &move_target, &selected_count);

                /* Ctrl+Y or Ctrl+Shift+Z — redo */
                } else if ((key == SDLK_y && ctrl) ||
                           (key == SDLK_z && ctrl && shift)) {
                    if (undo_redo(&undo, &sim.zeros, &sim.poles))
                        clear_selection(&popup, &move_target, &selected_count);

                /* Ctrl+R — randomise */
                } else if (key == SDLK_r && ctrl) {
                    undo_push(&undo, &sim.zeros, &sim.poles);
                    randomise_sim(&sim, &app);
                    clear_selection(&popup, &move_target, &selected_count);

                /* G — toggle cursor mode */
                } else if (key == SDLK_g) {
                    cursor_mode = !cursor_mode;
                    popup.is_open = 0;

                /* 1-9 — Presets */
                } else if (key >= SDLK_1 && key <= SDLK_9) {
                    undo_push(&undo, &sim.zeros, &sim.poles);
                    load_preset(&sim, key - SDLK_0);
                    app.cam_zoom = 1.0;
                    app.cam_cx = 0.0;
                    app.cam_cy = 0.0;
                    do_resize(&app, &sim, app.screen_w, app.screen_h);
                    clear_selection(&popup, &move_target, &selected_count);

                /* Z — add zero */
                } else if (key == SDLK_z && !ctrl) {
                    double complex cpos;
                    screen_choords_to_complex(mx, my, &cpos,
                        app.x_range, app.y_range, app.screen_w, app.screen_h);
                    undo_push(&undo, &sim.zeros, &sim.poles);
                    sim_add_zero(&sim, cpos);

                /* X — add pole */
                } else if (key == SDLK_x) {
                    double complex cpos;
                    screen_choords_to_complex(mx, my, &cpos,
                        app.x_range, app.y_range, app.screen_w, app.screen_h);
                    undo_push(&undo, &sim.zeros, &sim.poles);
                    sim_add_pole(&sim, cpos);

                /* Delete — remove selected entity */
                } else if (key == SDLK_DELETE && popup.entity) {
                    undo_push(&undo, &sim.zeros, &sim.poles);
                    sim_delete_entity(&sim, popup.entity);
                    clear_selection(&popup, &move_target, &selected_count);

                /* Tab — cycle colour map */
                } else if (key == SDLK_TAB) {
                    rcfg.c_map = (rcfg.c_map + 1) % 6;

                /* Up/Down — step count */
                } else if (key == SDLK_UP) {
                    if (++rcfg.steps > 64) rcfg.steps = 64;
                } else if (key == SDLK_DOWN) {
                    if (--rcfg.steps < 1) rcfg.steps = 1;

                /* Backspace — cycle normalisation */
                } else if (key == SDLK_BACKSPACE) {
                    rcfg.n_map = (rcfg.n_map + 1) % 3;

                /* H — toggle HUD */
                } else if (key == SDLK_h) {
                    flags.hud_visible = !flags.hud_visible;

                /* C — cycle circle mode */
                } else if (key == SDLK_c && !ctrl) {
                    flags.circle_mode = (CircleMode)((flags.circle_mode + 1) % CIRCLE_MODE_COUNT);

                /* P — save screenshot */
                } else if (key == SDLK_p && !ctrl) {
                    /* We actually save after the render frame finishes below,
                       so we just set a flag here */
                    flags.screenshot_queued = 1;

                /* O — toggle orbit */
                } else if (key == SDLK_o && !ctrl) {
                    flags.orbit_mode = !flags.orbit_mode;

                /* U — toggle pulse */
                } else if (key == SDLK_u && !ctrl) {
                    flags.pulse_mode = !flags.pulse_mode;

                /* F — toggle field lines */
                } else if (key == SDLK_f && !ctrl) {
                    flags.fieldlines_visible = !flags.fieldlines_visible;

                /* D — toggle domain overlay */
                } else if (key == SDLK_d && !ctrl) {
                    flags.domain_overlay = !flags.domain_overlay;

                /* A — toggle audio */
                } else if (key == SDLK_a && !ctrl) {
                    flags.audio_enabled = !flags.audio_enabled;
                    audio_set_enabled(flags.audio_enabled);

                /* S / F1 — settings panel */
                } else if (key == SDLK_s || key == SDLK_F1) {
                    settings.is_open = !settings.is_open;
                }

            /* ── Mouse Wheel (Zoom) ──────────────────────────────────── */
            } else if (e.type == SDL_MOUSEWHEEL) {
                if (e.wheel.y != 0) {
                    double zoom_factor = (e.wheel.y > 0) ? 1.1 : (1.0 / 1.1);

                    /* Find complex coord under mouse before zoom */
                    double complex cpos_before;
                    screen_choords_to_complex(mx, my, &cpos_before,
                                              app.x_range, app.y_range, app.screen_w, app.screen_h);

                    /* Apply zoom */
                    app.cam_zoom *= zoom_factor;
                    if (app.cam_zoom < 0.01) app.cam_zoom = 0.01;
                    if (app.cam_zoom > 1000.0) app.cam_zoom = 1000.0;
                    
                    /* Recompute ranges so we can map screen back to complex */
                    do_resize(&app, &sim, app.screen_w, app.screen_h);

                    /* Find complex coord under mouse after zoom (if center didn't move) */
                    double complex cpos_after;
                    screen_choords_to_complex(mx, my, &cpos_after,
                                              app.x_range, app.y_range, app.screen_w, app.screen_h);

                    /* Offset camera so cpos_before == cpos_after */
                    app.cam_cx += creal(cpos_before) - creal(cpos_after);
                    app.cam_cy += cimag(cpos_before) - cimag(cpos_after);

                    /* Final recompute of ranges with shifted camera */
                    do_resize(&app, &sim, app.screen_w, app.screen_h);
                }

            /* ── Mouse button down ──────────────────────────────────── */
            } else if (e.type == SDL_MOUSEBUTTONDOWN &&
                       e.button.button == SDL_BUTTON_LEFT) {

                /* --- Settings panel clicks first -------------------- */
                if (settings.is_open) {
                    int cd = 0, nd = 0, req_fs = 0;
                    int hit = settings_hit(&settings, &rcfg, &flags,
                                           mx, my, app.screen_w, app.screen_h,
                                           app.is_fullscreen, &cd, &nd);
                    if (hit == SETTINGS_HIT_FS) {
                        app_toggle_fullscreen(&app);
                        int w, h; SDL_GetWindowSize(app.window, &w, &h);
                        do_resize(&app, &sim, w, h);
                        clear_selection(&popup, &move_target, &selected_count);
                    }
                    (void)req_fs; (void)cd; (void)nd;
                    goto next_event; /* consumed */
                }

                /* --- Select mode ------------------------------------ */
                if (cursor_mode == 0) {
                    /* Clicking inside open popup */
                    if (popup.is_open) {
                        if (mx >= popup.x && mx < popup.x + 340 &&
                            my >= popup.y && my < popup.y + 320) {
                            if (popup_hit_mode_btn(&popup, mx, my)) {
                                popup.edit_mode = !popup.edit_mode;
                            } else {
                                int idx;
                                if (popup_hit_slider(&popup, mx, my, &idx)) {
                                    popup.dragging_slider = idx;
                                    popup_apply_slider(&popup, mx);
                                }
                            }
                            goto next_event;
                        } else {
                            popup.is_open = 0; popup.entity = NULL;
                        }
                    }

                    /* Click on entity → open popup */
                    double complex cpos;
                    screen_choords_to_complex(mx, my, &cpos,
                        app.x_range, app.y_range, app.screen_w, app.screen_h);
                    int etype = 0;
                    singularity_t *hit_e = sim_find_entity(&sim, cpos, 0.5, &etype);
                    if (hit_e) {
                        popup.entity      = hit_e;
                        popup.entity_type = etype;
                        popup.is_open     = 1;
                        popup.edit_mode   = 0;
                        popup.dragging_slider = -1;
                        int px_try = mx + 40;
                        if (px_try + 340 > app.screen_w) px_try = mx - 340 - 40;
                        if (px_try < 0) px_try = 5;
                        popup.x = px_try;
                        popup.y = my - 20;
                        if (popup.y + 320 > app.screen_h) popup.y = app.screen_h - 320;
                        if (popup.y < 0) popup.y = 0;
                    } else {
                        popup.is_open = 0; popup.entity = NULL;
                    }

                /* --- Move mode -------------------------------------- */
                } else {
                    double complex cpos;
                    screen_choords_to_complex(mx, my, &cpos,
                        app.x_range, app.y_range, app.screen_w, app.screen_h);
                    move_target = sim_find_entity(&sim, cpos, 0.5, NULL);
                    
                    /* If we didn't click on a selected member but we hit someone else, make them the only selection */
                    if (move_target) {
                        int in_group = 0;
                        for(int i=0; i<selected_count; i++) {
                            if (selected_group[i] == move_target) { in_group = 1; break; }
                        }
                        if (!in_group) {
                            selected_group[0] = move_target;
                            selected_count = 1;
                        }
                    } else {
                        selected_count = 0; /* Clicked empty space */
                    }
                }

            /* ── Mouse button down (RIGHT) for Selection Box ───────── */
            } else if (e.type == SDL_MOUSEBUTTONDOWN &&
                       e.button.button == SDL_BUTTON_RIGHT && cursor_mode == 1) {
                is_selecting = 1;
                sel_sx = mx; sel_sy = my;
                sel_ex = mx; sel_ey = my;
                selected_count = 0;

            /* ── Mouse button up ─────────────────────────────────────── */
            } else if (e.type == SDL_MOUSEBUTTONUP &&
                       e.button.button == SDL_BUTTON_LEFT) {
                if (move_target) {
                    undo_push(&undo, &sim.zeros, &sim.poles);
                    move_target = NULL;
                }
                if (popup.dragging_slider >= 0) {
                    undo_push(&undo, &sim.zeros, &sim.poles);
                    popup.dragging_slider = -1;
                }
                settings.drag_steps = -1;

            } else if (e.type == SDL_MOUSEBUTTONUP &&
                       e.button.button == SDL_BUTTON_RIGHT && cursor_mode == 1) {
                if (is_selecting) {
                    is_selecting = 0;
                    selected_count = 0;
                    
                    /* Build AABB in complex space */
                    double complex c1, c2;
                    screen_choords_to_complex(sel_sx, sel_sy, &c1, app.x_range, app.y_range, app.screen_w, app.screen_h);
                    screen_choords_to_complex(sel_ex, sel_ey, &c2, app.x_range, app.y_range, app.screen_w, app.screen_h);
                    
                    double min_r = fmin(creal(c1), creal(c2));
                    double max_r = fmax(creal(c1), creal(c2));
                    double min_i = fmin(cimag(c1), cimag(c2));
                    double max_i = fmax(cimag(c1), cimag(c2));

                    /* Find intersecting entities */
                    for (int k = 0; k < 2; k++) {
                        singularity_array_t *arr = (k == 0) ? &sim.zeros : &sim.poles;
                        for (size_t i = 0; i < arr->size; i++) {
                            double r = creal(arr->data[i].val);
                            double im = cimag(arr->data[i].val);
                            if (r >= min_r && r <= max_r && im >= min_i && im <= max_i) {
                                if (selected_count < MAX_SELECTED) {
                                    selected_group[selected_count++] = &arr->data[i];
                                }
                            }
                        }
                    }
                }

            /* ── Mouse motion ─────────────────────────────────────────  */
            } else if (e.type == SDL_MOUSEMOTION) {
                /* Drag slider in popup */
                if (popup.dragging_slider >= 0 && popup.entity)
                    popup_apply_slider(&popup, mx);

                /* Drag entity/group in move mode */
                if (move_target && cursor_mode == 1) {
                    double complex cpos;
                    screen_choords_to_complex(e.motion.x, e.motion.y, &cpos,
                        app.x_range, app.y_range, app.screen_w, app.screen_h);
                    
                    double complex diff = cpos - move_target->val;
                    for (int i = 0; i < selected_count; i++) {
                        selected_group[i]->val += diff;
                    }
                }

                /* Selection box update */
                if (is_selecting && cursor_mode == 1) {
                    sel_ex = mx;
                    sel_ey = my;
                }

                /* Middle-click pan */
                if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
                    /* Convert pixel delta to complex delta using current range sizes */
                    double dx = app.x_range[1] - app.x_range[0];
                    double dy = app.y_range[1] - app.y_range[0];
                    app.cam_cx -= (double)e.motion.xrel * (dx / app.screen_w);
                    app.cam_cy -= (double)e.motion.yrel * (dy / app.screen_h);
                    /* Important: Recompute grids immediately */
                    do_resize(&app, &sim, app.screen_w, app.screen_h);
                }

                /* Drag steps slider inside settings */
                if (settings.drag_steps >= 0) {
                    int srx = (app.screen_w - 340)/2 + 20;
                    double t = (double)(mx - srx) / 300.0;
                    if (t < 0) t = 0;
                    if (t > 1) t = 1;
                    rcfg.steps = 1 + (int)(t * 63 + 0.5);
                }
            }

            next_event:;
        } /* end event loop */

        /* ── Cursor update ───────────────────────────────────────────── */
        {
            int cmx, cmy; SDL_GetMouseState(&cmx, &cmy);
            double complex cpos;
            screen_choords_to_complex(cmx, cmy, &cpos,
                app.x_range, app.y_range, app.screen_w, app.screen_h);
            hover_entity = sim_find_entity(&sim, cpos, 0.5, &hover_type);

            if (hover_entity)              SDL_SetCursor(app.cursor_hand);
            else if (cursor_mode == 1)     SDL_SetCursor(app.cursor_move);
            else                           SDL_SetCursor(app.cursor_arrow);
        }

        /* ── Animations ──────────────────────────────────────────────── */
        double dt = 1.0 / 60.0; /* Approximate dt for smooth animation */
        if (current_fps > 10.0) dt = 1.0 / current_fps;

        if (flags.orbit_mode) {
            double rotation_speed = 0.5; /* radians per second */
            double rot = rotation_speed * dt;
            orbit_angle += rot;
            double complex rot_factor = cexp(I * rot);
            for (size_t i = 0; i < sim.zeros.size; i++) 
                sim.zeros.data[i].val *= rot_factor;
            for (size_t i = 0; i < sim.poles.size; i++) 
                sim.poles.data[i].val *= rot_factor;
        }

        if (flags.pulse_mode) {
            double pulse_speed = 3.0;
            pulse_t += pulse_speed * dt;
            double pulse_factor = 1.0 + 0.3 * sin(pulse_t);
            for (size_t i = 0; i < sim.zeros.size; i++) 
                sim.zeros.data[i].m = 1.0 * pulse_factor; /* Assuming base m is 1.0 for now, would be better to store base_m */
            for (size_t i = 0; i < sim.poles.size; i++) 
                sim.poles.data[i].m = 1.0 * pulse_factor;
        } else if (pulse_t > 0.0) {
            /* Reset to 1.0 when turned off */
            pulse_t = 0.0;
            for (size_t i = 0; i < sim.zeros.size; i++) sim.zeros.data[i].m = 1.0;
            for (size_t i = 0; i < sim.poles.size; i++) sim.poles.data[i].m = 1.0;
        }

        if (flags.audio_enabled) {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            double complex cpos;
            screen_choords_to_complex(mx, my, &cpos,
                app.x_range, app.y_range, app.screen_w, app.screen_h);
                
            double complex H = sim_eval_H(&sim, cpos);
            double mag = cabs(H);
            
            double freq = 440.0;
            if (mag > 0.001) {
                // Pitch based on log-magnitude (octaves)
                freq = 440.0 * pow(2.0, log10(mag) / 2.0);
            }
            if (freq < 20.0) freq = 20.0;
            if (freq > 2000.0) freq = 2000.0;
            
            double min_dist = 1e9;
            for (size_t i = 0; i < sim.zeros.size; i++) {
                double d = cabs(cpos - sim.zeros.data[i].val);
                if (d < min_dist) min_dist = d;
            }
            for (size_t i = 0; i < sim.poles.size; i++) {
                double d = cabs(cpos - sim.poles.data[i].val);
                if (d < min_dist) min_dist = d;
            }
            
            double amp = 1.0 / (1.0 + min_dist * 5.0); // Fade out quickly
            audio_set_params(freq, amp);
        }

        /* ── Math render (low-res) ───────────────────────────────────── */
        sim_compute(&sim, app.grid_w, app.grid_h);

        switch (rcfg.n_map) {
            case 0: normalize_H_soa(sim.nH_re, sim.nH_im,
                        (size_t)app.grid_w * app.grid_h, sim.nH_re, sim.nH_im); break;
            case 1: normalize_H_log_soa(sim.nH_re, sim.nH_im,
                        (size_t)app.grid_w * app.grid_h, sim.nH_re, sim.nH_im); break;
            case 2: normalize_H_log_steps_soa(sim.nH_re, sim.nH_im,
                        (size_t)app.grid_w * app.grid_h, rcfg.steps,
                        sim.nH_re, sim.nH_im); break;
        }

        switch (rcfg.c_map) {
            case 0: H_g_img (sim.nH_re, sim.nH_im, (size_t)app.grid_w*app.grid_h, app.H_img); break;
            case 1: H_c1_img(sim.nH_re, sim.nH_im, (size_t)app.grid_w*app.grid_h, app.H_img); break;
            case 2: H_c2_img(sim.nH_re, sim.nH_im, (size_t)app.grid_w*app.grid_h, app.H_img); break;
            case 3: H_c3_img(sim.nH_re, sim.nH_im, (size_t)app.grid_w*app.grid_h, app.H_img); break;
            case 4: H_c4_img(sim.nH_re, sim.nH_im, (size_t)app.grid_w*app.grid_h, app.H_img); break;
            case 5: H_c5_img(sim.nH_re, sim.nH_im, (size_t)app.grid_w*app.grid_h, app.H_img); break;
        }

        /* ── UI render (full-res) ────────────────────────────────────── */
        memset(app.UI_img.data, 0, app.screen_w * app.screen_h * sizeof(uint32_t));

        if (flags.hud_visible)
            ui_draw_grid(&app.UI_img, app.x_range[0], app.x_range[1],
                                      app.y_range[0], app.y_range[1]);

        if (flags.domain_overlay) {
            ui_draw_domain_overlay(&app.UI_img, &sim, app.x_range, app.y_range);
        }

        if (flags.fieldlines_visible) {
            ui_draw_field_lines(&app.UI_img, &sim, app.x_range, app.y_range);
        }

        ui_draw_entities(&app.UI_img, &sim,
                         app.x_range, app.y_range,
                         popup.entity, move_target, flags);

        /* Highlight selected group members */
        for (int i = 0; i < selected_count; i++) {
            uint32_t cx, cy;
            complex_to_screen_choords(selected_group[i]->val, &cx, &cy, app.x_range, app.y_range, app.screen_w, app.screen_h);
            draw_circle_glow_color(&app.UI_img, cx, cy, 20, 0xAAFFFF00);
        }

        /* Draw active selection box */
        if (is_selecting) {
            int x_min = (sel_sx < sel_ex) ? sel_sx : sel_ex;
            int x_max = (sel_sx > sel_ex) ? sel_sx : sel_ex;
            int y_min = (sel_sy < sel_ey) ? sel_sy : sel_ey;
            int y_max = (sel_sy > sel_ey) ? sel_sy : sel_ey;
            for (int x = x_min; x <= x_max; x+=4) {
                if (x >= 0 && x < app.screen_w && y_min >= 0 && y_min < app.screen_h) app.UI_img.data[y_min * app.screen_w + x] = 0xFFFFFFFF;
                if (x >= 0 && x < app.screen_w && y_max >= 0 && y_max < app.screen_h) app.UI_img.data[y_max * app.screen_w + x] = 0xFFFFFFFF;
            }
            for (int y = y_min; y <= y_max; y+=4) {
                if (x_min >= 0 && x_min < app.screen_w && y >= 0 && y < app.screen_h) app.UI_img.data[y * app.screen_w + x_min] = 0xFFFFFFFF;
                if (x_max >= 0 && x_max < app.screen_w && y >= 0 && y < app.screen_h) app.UI_img.data[y * app.screen_w + x_max] = 0xFFFFFFFF;
            }
        }

        if (hover_entity && !popup.is_open)
            ui_draw_tooltip(&app.UI_img, mx, my, hover_entity, hover_type);

        ui_draw_hud(&app.UI_img, current_fps, &rcfg, &flags,
                    cursor_mode, app.is_fullscreen);
        ui_draw_popup(&app.UI_img, &popup);

        int req_fs = 0;
        ui_draw_settings(&app.UI_img, &settings, &rcfg, &flags,
                         app.screen_w, app.screen_h, app.is_fullscreen, &req_fs);

        /* ── Composite & present ─────────────────────────────────────── */
        SDL_UpdateTexture(app.H_tex,  NULL, app.H_img.data,  app.H_img.width  * sizeof(uint32_t));
        SDL_UpdateTexture(app.UI_tex, NULL, app.UI_img.data, app.UI_img.width * sizeof(uint32_t));
        SDL_RenderClear(app.renderer);
        SDL_RenderCopy(app.renderer, app.H_tex,  NULL, NULL);
        SDL_RenderCopy(app.renderer, app.UI_tex, NULL, NULL);

        if (flags.screenshot_queued) {
            save_screenshot(&app);
            flags.screenshot_queued = 0;
        }

        SDL_RenderPresent(app.renderer);

        /* ── Frame timing & FPS ──────────────────────────────────────── */
        Uint32 elapsed = SDL_GetTicks() - frame_start;
        frame_count++;
        uint32_t now = SDL_GetTicks();
        if (now - last_fps_time >= 500) {
            current_fps  = frame_count * 1000.0f / (float)(now - last_fps_time);
            frame_count  = 0;
            last_fps_time = now;
        }
        if (elapsed < FRAME_DURATION_MS)
            SDL_Delay(FRAME_DURATION_MS - elapsed);
    }

    /* ── Cleanup ─────────────────────────────────────────────────────── */
    undo_destroy(&undo);
    sim_destroy(&sim);
    audio_shutdown();
    app_destroy(&app);
    printf("Bye.\n");
    return 0;
}