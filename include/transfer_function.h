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

// for s
double complex *generate_s_grid(uint16_t height, uint16_t width, double y_range[2], double x_range[2]);

// for poles and zeros
typedef struct
{
    double complex val;
    uint8_t e;
    double m;
    double c;
} singularity_t;

typedef struct
{
    singularity_t *data;
    size_t size;
    size_t capacity;
} singularity_array_t;

double complex *generate_s_grid(uint16_t height, uint16_t width, double y_range[2], double x_range[2]);

void create_singularities(singularity_array_t *arr, size_t initial_capacity);
void add_singularity(singularity_array_t *arr, singularity_t s);
void remove_singularity(singularity_array_t *arr, size_t index);
void free_singularities(singularity_array_t *arr);

double complex *compute_H(double complex *s_grid, singularity_array_t zeros_arr, singularity_array_t poles_arr, uint16_t height, uint16_t width, double complex *H);

double complex *normalize_H_complex(const double complex *H, size_t size, double complex *normalized);
double complex *normalize_H_log_complex(const double complex *H, size_t size, double complex *normalized);
double complex *normalize_H_log_complex_steps(const double complex *H, size_t size, int steps, double complex *normalized);

void H_g_img(double complex *n_H, img_t H_img);
void H_c1_img(double complex *n_H, img_t H_img);
void H_c2_img(double complex *n_H, img_t H_img);
void H_c3_img(double complex *n_H, img_t H_img);
void H_c4_img(double complex *n_H, img_t H_img);
void H_c5_img(double complex *n_H, img_t H_img);

#endif