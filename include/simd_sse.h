#ifndef _SIMD_SSE_HH
#define _SIMD_SSE_HH

#include "transfer_function.h"
#include "img_utils.h"
#include <xmmintrin.h>
#include <emmintrin.h>

float **sse2_compute_H(
    float x_range[2], float y_range[2],
    new_singularity_array_t zeros_arr, new_singularity_array_t poles_arr,
    uint16_t height, uint16_t width,
    float **H);

float **sse_normalize_H(
    const float **H, size_t size,
    float **normalized);

#endif