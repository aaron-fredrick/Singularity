#ifndef _TF_HH
#define _TF_HH

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

// for s
double complex *generate_s_grid(uint16_t height, uint16_t width, double y_range[2], double x_range[2]);

// for poles and zeros
typedef struct
{
    double complex val;
    uint8_t e;
    double m;
    double complex c;
} singularity_t;

typedef struct
{
    singularity_t *data;
    size_t size;
    size_t capacity;
} singularity_array_t;

void create_singularities(singularity_array_t *arr, size_t initial_capacity);
void add_singularity(singularity_array_t *arr, singularity_t s);
void remove_singularity(singularity_array_t *arr, size_t index);
void free_singularities(singularity_array_t *arr);

#endif