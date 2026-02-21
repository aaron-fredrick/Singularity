#ifndef _SIMD_AVX_HH
#define _SIMD_AVX_HH

#include "transfer_function.h"
#include <immintrin.h>

float **avx_compute_H(
    float x_range[2], float y_range[2],
    new_singularity_array_t zeros_arr, new_singularity_array_t poles_arr,
    uint16_t height, uint16_t width,
    float **H);

#endif