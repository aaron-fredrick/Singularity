#include "simulation.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <complex.h>

#define N_ZEROS_DEFAULT 3
#define N_POLES_DEFAULT 3

/* ── SOA helpers ─────────────────────────────────────────────────────── */

static void ensure_soa_cap(singularity_soa_t *soa, size_t *cap, size_t need) {
    if (need <= *cap) return;
    size_t nc = (need < 8) ? 8 : need * 2;
    soa->real = realloc(soa->real, nc * sizeof(double));
    soa->imag = realloc(soa->imag, nc * sizeof(double));
    soa->m_re = realloc(soa->m_re, nc * sizeof(double));
    soa->m_im = realloc(soa->m_im, nc * sizeof(double));
    soa->c_re = realloc(soa->c_re, nc * sizeof(double));
    soa->c_im = realloc(soa->c_im, nc * sizeof(double));
    soa->e    = realloc(soa->e,    nc * sizeof(uint8_t));
    *cap = nc;
}

static void sync_soa(singularity_soa_t *soa, size_t *cap,
                     const singularity_array_t *arr) {
    ensure_soa_cap(soa, cap, arr->size);
    soa->size = arr->size;
    for (size_t i = 0; i < arr->size; i++) {
        soa->real[i] = creal(arr->data[i].val);
        soa->imag[i] = cimag(arr->data[i].val);
        soa->m_re[i] = creal(arr->data[i].m);
        soa->m_im[i] = cimag(arr->data[i].m);
        soa->c_re[i] = creal(arr->data[i].c);
        soa->c_im[i] = cimag(arr->data[i].c);
        soa->e[i]    = arr->data[i].e;
    }
}

static void ensure_nH_cap(Simulation *s, size_t need) {
    if (need <= s->nH_cap) return;
    s->nH_re  = realloc(s->nH_re, need * sizeof(double));
    s->nH_im  = realloc(s->nH_im, need * sizeof(double));
    s->nH_cap = need;
}

static void free_soa(singularity_soa_t *soa) {
    free(soa->real); free(soa->imag);
    free(soa->m_re); free(soa->m_im);
    free(soa->c_re); free(soa->c_im);
    free(soa->e);
    memset(soa, 0, sizeof(*soa));
}

/* ── Random entity factory ───────────────────────────────────────────── */

static singularity_t make_random(double x_range[2], double y_range[2]) {
    singularity_t s = {0};
    s.val = random_uniform(x_range[0], x_range[1])
          + random_uniform(y_range[0], y_range[1]) * I;
    s.e   = (uint8_t)random_uniform(1, 4);
    s.m   = random_uniform(0.5, 2.0) + random_uniform(-1, 1) * I;
    s.c   = random_uniform(0.5, 2.0) + random_uniform(-1, 1) * I;
    return s;
}

/* ── Public API ─────────────────────────────────────────────────────── */

void sim_init(Simulation *s, double x_range[2], double y_range[2],
              int grid_w, int grid_h) {
    memset(s, 0, sizeof(*s));
    create_singularities(&s->zeros, N_ZEROS_DEFAULT);
    create_singularities(&s->poles, N_POLES_DEFAULT);
    sim_randomise(s, x_range, y_range);
    sim_rebuild_grid(s, x_range, y_range, grid_w, grid_h);
}

void sim_rebuild_grid(Simulation *s, double x_range[2], double y_range[2],
                      int grid_w, int grid_h) {
    free_s_grid_soa(&s->s_grid);
    s->s_grid = generate_s_grid_soa((uint16_t)grid_h, (uint16_t)grid_w,
                                     y_range, x_range);
    ensure_nH_cap(s, (size_t)grid_w * grid_h);
}

void sim_randomise(Simulation *s, double x_range[2], double y_range[2]) {
    s->zeros.size = 0;
    s->poles.size = 0;
    for (int i = 0; i < N_ZEROS_DEFAULT; i++)
        add_singularity(&s->zeros, make_random(x_range, y_range));
    for (int i = 0; i < N_POLES_DEFAULT; i++)
        add_singularity(&s->poles, make_random(x_range, y_range));
}

singularity_t *sim_add_zero(Simulation *s, double complex pos) {
    singularity_t z = {0};
    z.val = pos; z.e = 1;
    z.m   = 1.0 + 0.0 * I;
    z.c   = 0.5 + 0.0 * I;
    add_singularity(&s->zeros, z);
    return &s->zeros.data[s->zeros.size - 1];
}

singularity_t *sim_add_pole(Simulation *s, double complex pos) {
    singularity_t p = {0};
    p.val = pos; p.e = 1;
    p.m   = 1.0 + 0.0 * I;
    p.c   = 0.5 + 0.0 * I;
    add_singularity(&s->poles, p);
    return &s->poles.data[s->poles.size - 1];
}

int sim_delete_entity(Simulation *s, singularity_t *entity) {
    singularity_array_t *arrs[2] = {&s->zeros, &s->poles};
    for (int k = 0; k < 2; k++) {
        for (size_t i = 0; i < arrs[k]->size; i++) {
            if (&arrs[k]->data[i] == entity) {
                remove_singularity(arrs[k], i);
                return 1;
            }
        }
    }
    return 0;
}

singularity_t *sim_find_entity(Simulation *s, double complex pos,
                                double threshold, int *out_type) {
    singularity_array_t *arrs[2] = {&s->poles, &s->zeros};
    int types[2] = {2, 1};
    for (int k = 0; k < 2; k++) {
        for (size_t i = 0; i < arrs[k]->size; i++) {
            if (cabs(arrs[k]->data[i].val - pos) < threshold) {
                if (out_type) *out_type = types[k];
                return &arrs[k]->data[i];
            }
        }
    }
    return NULL;
}

void sim_compute(Simulation *s, int grid_w, int grid_h) {
    sync_soa(&s->zeros_soa, &s->soa_cap_z, &s->zeros);
    sync_soa(&s->poles_soa, &s->soa_cap_p, &s->poles);
    size_t n = (size_t)grid_w * grid_h;
    ensure_nH_cap(s, n);
    s->H_simd = compute_H_simd_direct(s->s_grid, s->zeros_soa, s->poles_soa,
                                       (uint16_t)grid_h, (uint16_t)grid_w, s->H_simd);
    for (size_t i = 0; i < n; i++) {
        s->nH_re[i] = (double)s->H_simd[0][i];
        s->nH_im[i] = (double)s->H_simd[1][i];
    }
}

void sim_destroy(Simulation *s) {
    free_singularities(&s->zeros);
    free_singularities(&s->poles);
    free_soa(&s->zeros_soa);
    free_soa(&s->poles_soa);
    if (s->H_simd) { free(s->H_simd[0]); free(s->H_simd[1]); free(s->H_simd); }
    free(s->nH_re);
    free(s->nH_im);
    free_s_grid_soa(&s->s_grid);
}

double complex sim_eval_H(Simulation *s, double complex pos) {
    double pr = creal(pos), pi = cimag(pos);
    double Hr = 1.0, Hi = 0.0;
    
    /* Zeros */
    for (size_t i = 0; i < s->zeros.size; i++) {
        singularity_t *z = &s->zeros.data[i];
        double zr = creal(z->val), zi = cimag(z->val);
        double tr = pr - zr, ti = pi - zi;
        
        double fr = 1.0, fi = 0.0;
        int e = z->e;
        for (int j = 0; j < e; j++) {
            double nfr = fr * tr - fi * ti;
            double nfi = fr * ti + fi * tr;
            fr = nfr; fi = nfi;
        }
        
        double mr = creal(z->m), mi = cimag(z->m);
        double cr = creal(z->c), ci = cimag(z->c);
        
        double tfr = fr * mr - fi * mi + cr;
        double tfi = fr * mi + fi * mr + ci;
        
        double nHr = Hr * tfr - Hi * tfi;
        double nHi = Hr * tfi + Hi * tfr;
        Hr = nHr; Hi = nHi;
    }
    
    /* Poles */
    for (size_t i = 0; i < s->poles.size; i++) {
        singularity_t *p = &s->poles.data[i];
        double pr_pole = creal(p->val), pi_pole = cimag(p->val);
        double tr = pr - pr_pole, ti = pi - pi_pole;
        
        double fr = 1.0, fi = 0.0;
        int e = p->e;
        for (int j = 0; j < e; j++) {
            double nfr = fr * tr - fi * ti;
            double nfi = fr * ti + fi * tr;
            fr = nfr; fi = nfi;
        }
        
        double mr = creal(p->m), mi = cimag(p->m);
        double cr = creal(p->c), ci = cimag(p->c);
        
        double den_r = fr * mr - fi * mi + cr;
        double den_i = fr * mi + fi * mr + ci;
        
        double den_mag2 = den_r * den_r + den_i * den_i;
        if (den_mag2 > 1e-24) {
            double nHr = (Hr * den_r + Hi * den_i) / den_mag2;
            double nHi = (Hi * den_r - Hr * den_i) / den_mag2;
            Hr = nHr; Hi = nHi;
        } else {
            Hr = 1e12; Hi = 1e12;
        }
    }
    
    return Hr + Hi * I;
}
