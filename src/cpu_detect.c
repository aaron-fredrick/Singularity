#include "cpu_detect.h"
#include <stdio.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif

static void run_cpuid(uint32_t eax, uint32_t ecx, uint32_t* regs) {
#ifdef _MSC_VER
    __cpuidex((int*)regs, (int)eax, (int)ecx);
#else
    __cpuid_count(eax, ecx, regs[0], regs[1], regs[2], regs[3]);
#endif
}

cpu_features_t detect_cpu_features(void) {
    cpu_features_t features = CPU_FEATURE_NONE;
    uint32_t regs[4];

    run_cpuid(0, 0, regs);
    uint32_t max_level = regs[0];

    if (max_level >= 1) {
        run_cpuid(1, 0, regs);
        
        if (regs[3] & (1 << 26)) {
            features |= CPU_FEATURE_SSE2;
        }
        
        if (regs[2] & (1 << 28)) {
            features |= CPU_FEATURE_AVX;
        }
    }

    if (max_level >= 7) {
        run_cpuid(7, 0, regs);
        
        if (regs[1] & (1 << 5)) {
            features |= CPU_FEATURE_AVX2;
        }
        
        if (regs[1] & (1 << 16)) {
            features |= CPU_FEATURE_AVX512F;
        }
    }

    return features;
}

const char* get_simd_level_name(cpu_features_t features) {
    if (features & CPU_FEATURE_AVX512F) {
        return "AVX-512";
    } else if (features & CPU_FEATURE_AVX2) {
        return "AVX2";
    } else if (features & CPU_FEATURE_AVX) {
        return "AVX";
    } else if (features & CPU_FEATURE_SSE2) {
        return "SSE2";
    } else {
        return "Scalar (no SIMD)";
    }
}
