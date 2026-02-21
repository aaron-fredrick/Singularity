# Singularity Architecture Update: Structure of Arrays (SoA)

## Overview
The Singularity engine has been refactored to utilize a Structure of Arrays (SoA) memory layout throughout the entire pipeline, from grid generation to visualization. This change improves performance by leveraging SIMD optimizations and eliminating complex number arithmetic overhead.

## Key Changes

### Data Structures
- **Grid (`s_grid_soa_t`)**: The complex plane grid is stored as separate `float *real` and `float *imag` arrays.
- **Entities (`singularity_soa_t`)**: Poles and Zeros are stored as separate arrays for Real, Imaginary, Magnitude, Constant, and Exponent components. This structure is passed directly to the compute kernel.

### Compute Pipeline
1. **Direct SoA Compute**: The `compute_H_simd_direct` function takes `singularity_soa_t` inputs and processes them against the `s_grid_soa_t` grid using AVX-512/AVX/SSE2 intrinsics.
2. **Output**: The compute kernel outputs `float **H` where `H[0]` is the Real component array and `H[1]` is the Imaginary component array.

### Normalization & Visualization
- **Normalization**: Functions like `normalize_H_soa` now operate on separate `double *re` and `double *im` arrays, avoiding `double complex` overhead and expensive trigonometric functions (`cabs`, `carg`, `cexp`).
- **Color Mapping**: All `H_c*_img` functions accept separate Real and Imaginary arrays, enabling efficient vectorized color calculations.

### Performance
- **SIMD Efficiency**: Data is aligned for vector registers (256-bit/512-bit) without stride overhead.
- **Memory Bandwidth**: Access patterns are linear and cache-friendly.
- **Math Optimization**: Replaced `cabs/carg` with optimized algebraic equivalents where possible.

## Legacy Code
- `compute_H` (scalar complex) and `compute_H_auto` (dispatcher) have been removed in favor of the unified SoA pipeline.
