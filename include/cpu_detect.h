#ifndef _CPU_DETECT_HH
#define _CPU_DETECT_HH

#include <stdint.h>

typedef enum {
    CPU_FEATURE_NONE = 0,
    CPU_FEATURE_SSE2 = 1 << 0,
    CPU_FEATURE_AVX = 1 << 1,
    CPU_FEATURE_AVX2 = 1 << 2,
    CPU_FEATURE_AVX512F = 1 << 3,
} cpu_features_t;

cpu_features_t detect_cpu_features(void);
const char* get_simd_level_name(cpu_features_t features);

#endif
