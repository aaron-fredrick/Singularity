#ifndef SIMULATION_H
#define SIMULATION_H

#include "transfer_function.h"
#include <complex.h>

typedef struct {
    singularity_array_t zeros;
    singularity_array_t poles;

    /* Persistent SOA buffers — grown with realloc, never freed until shutdown */
    singularity_soa_t zeros_soa;
    singularity_soa_t poles_soa;
    size_t            soa_cap_z;
    size_t            soa_cap_p;

    /* Output buffers for normalised H (persistent) */
    float  **H_simd;
    double  *nH_re;
    double  *nH_im;
    size_t   nH_cap;

    /* s-grid (rebuilt on window resize) */
    s_grid_soa_t s_grid;
} Simulation;

void  sim_init(Simulation *s, double x_range[2], double y_range[2],
               int grid_w, int grid_h);
void  sim_rebuild_grid(Simulation *s, double x_range[2], double y_range[2],
                       int grid_w, int grid_h);
void  sim_randomise(Simulation *s, double x_range[2], double y_range[2]);

singularity_t *sim_add_zero(Simulation *s, double complex pos);
singularity_t *sim_add_pole(Simulation *s, double complex pos);
int            sim_delete_entity(Simulation *s, singularity_t *entity);
singularity_t *sim_find_entity(Simulation *s, double complex pos,
                                double threshold, int *out_type);

void sim_compute(Simulation *s, int grid_w, int grid_h);
void sim_destroy(Simulation *s);

double complex sim_eval_H(Simulation *s, double complex pos);

#endif /* SIMULATION_H */
