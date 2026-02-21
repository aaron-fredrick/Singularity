#include "presets.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void load_preset(Simulation *sim, int n) {
    if (!sim) return;
    
    /* Clear current entities */
    sim->zeros.size = 0;
    sim->poles.size = 0;

    switch (n) {
        case 1: {
            /* Preset 1: Butterworth Pattern (Poles on left unit semicircle) */
            int order = 5;
            for (int k = 1; k <= order; k++) {
                double angle = M_PI * (2 * k + order - 1) / (2 * order);
                sim_add_pole(sim, cos(angle) + sin(angle) * I);
            }
            break;
        }
        case 2: {
            /* Preset 2: Simple Resonator (Conjugate pole pair + zero at origin) */
            sim_add_zero(sim, 0.0 + 0.0 * I);
            sim_add_pole(sim, -0.2 + 1.0 * I);
            sim_add_pole(sim, -0.2 - 1.0 * I);
            break;
        }
        case 3: {
            /* Preset 3: Symmetric Cross (4 Zeros, 4 Poles) */
            sim_add_zero(sim,  1.0 + 0.0 * I);
            sim_add_zero(sim, -1.0 + 0.0 * I);
            sim_add_zero(sim,  0.0 + 1.0 * I);
            sim_add_zero(sim,  0.0 - 1.0 * I);
            
            sim_add_pole(sim,  1.5 + 1.5 * I);
            sim_add_pole(sim, -1.5 + 1.5 * I);
            sim_add_pole(sim,  1.5 - 1.5 * I);
            sim_add_pole(sim, -1.5 - 1.5 * I);
            break;
        }
        case 4: {
            /* Preset 4: Ring of Poles */
            int count = 8;
            for (int k = 0; k < count; k++) {
                double angle = 2.0 * M_PI * k / count;
                sim_add_pole(sim, cos(angle) * 1.5 + sin(angle) * 1.5 * I);
            }
            break;
        }
        case 5: {
            /* Preset 5: Alternating Ring (Zero/Pole) */
            int count = 6;
            for (int k = 0; k < count * 2; k++) {
                double angle = M_PI * k / count;
                double complex pos = cos(angle) * 2.0 + sin(angle) * 2.0 * I;
                if (k % 2 == 0) sim_add_zero(sim, pos);
                else            sim_add_pole(sim, pos);
            }
            break;
        }
        case 6: {
            /* Preset 6: Elliptic Filter Pattern (Poles on ellipse, zeros on imaginary axis) */
            sim_add_pole(sim, -0.1 + 0.9 * I);
            sim_add_pole(sim, -0.1 - 0.9 * I);
            sim_add_pole(sim, -0.4 + 0.4 * I);
            sim_add_pole(sim, -0.4 - 0.4 * I);
            sim_add_pole(sim, -0.6 + 0.0 * I);
            
            sim_add_zero(sim, 0.0 + 1.5 * I);
            sim_add_zero(sim, 0.0 - 1.5 * I);
            sim_add_zero(sim, 0.0 + 2.5 * I);
            sim_add_zero(sim, 0.0 - 2.5 * I);
            break;
        }
        case 7: {
            /* Preset 7: Multi-order pole at origin, surrounded by zeros */
            singularity_t *p = sim_add_pole(sim, 0.0 + 0.0 * I);
            p->e = 4; /* Order 4 */
            
            sim_add_zero(sim,  2.0 + 0.0 * I);
            sim_add_zero(sim, -2.0 + 0.0 * I);
            sim_add_zero(sim,  0.0 + 2.0 * I);
            sim_add_zero(sim,  0.0 - 2.0 * I);
            break;
        }
        case 8: {
            /* Preset 8: Checkerboard (Grid of alternating entities) */
            for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {
                    if (x == 0 && y == 0) continue;
                    double complex pos = (double)x * 1.5 + (double)y * 1.5 * I;
                    if ((x + y) % 2 == 0) sim_add_pole(sim, pos);
                    else                  sim_add_zero(sim, pos);
                }
            }
            break;
        }
        case 9: {
            /* Preset 9: Unit Circle Poles + Center Zero (All-Pass-like structure) */
            sim_add_zero(sim, 0.0 + 0.0 * I);
            for (int k = 0; k < 12; k++) {
                double angle = 2.0 * M_PI * k / 12.0;
                sim_add_pole(sim, cos(angle) + sin(angle) * I);
            }
            break;
        }
        default:
            break;
    }
}
