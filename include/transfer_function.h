#ifndef _TF_HH
#define _TF_HH

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <complex.h>
#include "img_utils.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// for s
double complex *generate_s_grid(uint16_t height, uint16_t width, double y_range[2], double x_range[2]);

// SoA (Structure of Arrays) version for better SIMD performance
typedef struct {
    float *real;
    float *imag;
    uint32_t length;
} s_grid_soa_t;

s_grid_soa_t generate_s_grid_soa(uint16_t height, uint16_t width, double y_range[2], double x_range[2]);
void free_s_grid_soa(s_grid_soa_t *grid);

// for poles and zeros
typedef struct

{
    double complex val;
    uint8_t e;
    double complex m;
    double complex c;
} singularity_t;

typedef struct
{
    singularity_t *data;
    size_t size;
    size_t capacity;
} singularity_array_t;

// SoA structure for singularities (double precision)
typedef struct {
    double *real;
    double *imag;
    double *m_re;
    double *m_im;
    double *c_re;
    double *c_im;
    uint8_t *e;
    size_t size;
} singularity_soa_t;

// SoA structure for SIMD (float precision)
typedef struct {
    float *r;
    float *i;
    uint8_t *e;
    float *m_r;
    float *m_i;
    float *c_r;
    float *c_i;
    size_t count;
} new_singularity_array_t;

/* generate_s_grid declared above (line 16) */

void create_singularities(singularity_array_t *arr, size_t initial_capacity);
void add_singularity(singularity_array_t *arr, singularity_t s);
void remove_singularity(singularity_array_t *arr, size_t index);
void free_singularities(singularity_array_t *arr);

// Normalize using SoA (real/imag arrays)
void normalize_H_soa(double *in_re, double *in_im, size_t size, double *out_re, double *out_im);
void normalize_H_log_soa(double *in_re, double *in_im, size_t size, double *out_re, double *out_im);
void normalize_H_log_steps_soa(double *in_re, double *in_im, size_t size, int steps, double *out_re, double *out_im);

// Visualization using SoA
void H_g_img(double *re, double *im, size_t size, img_t H_img);
void H_c1_img(double *re, double *im, size_t size, img_t H_img);
void H_c2_img(double *re, double *im, size_t size, img_t H_img);
void H_c3_img(double *re, double *im, size_t size, img_t H_img);
void H_c4_img(double *re, double *im, size_t size, img_t H_img);
void H_c5_img(double *re, double *im, size_t size, img_t H_img);

// Compute using SoA singularities
float **compute_H_simd_direct(s_grid_soa_t grid, singularity_soa_t zeros, singularity_soa_t poles, uint16_t height, uint16_t width, float **H);

// SIMD data structure conversion
new_singularity_array_t convert_to_simd_array(const singularity_array_t *arr);
void free_simd_array(new_singularity_array_t *arr);

double complex *compute_H(double complex *s_grid, singularity_array_t zeros_arr, singularity_array_t poles_arr, uint16_t height, uint16_t width, double complex *H);

// Auto-dispatch function that selects best SIMD implementation
double complex *compute_H_auto(double complex *s_grid, singularity_array_t zeros_arr, singularity_array_t poles_arr, uint16_t height, uint16_t width, double complex *H);



#endif