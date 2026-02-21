#include "simd_avx512.h"
#include <stdlib.h>

static void compute_s16_from_index(
    uint16_t height,
    uint16_t width,
    float y_range[2],
    float x_range[2],
    uint32_t start_index,
    float out_re[16],
    float out_im[16]
)
{
    float dy = (y_range[1] - y_range[0]) / (height - 1);
    float dx = (x_range[1] - x_range[0]) / (width - 1);

    for (int i = 0; i < 16; i++)
    {
        uint32_t idx = start_index + i;
        if (idx >= height * width)
        {
            out_re[i] = 0.0f;
            out_im[i] = 0.0f;
            continue;
        }

        uint16_t y = idx / width;
        uint16_t x = idx % width;

        out_re[i] = (float)(x_range[0] + x * dx);
        out_im[i] = (float)(y_range[0] + y * dy);
    }
}

__attribute__((target("avx512f"))) static inline void avx512_complex_mul(
    __m512 ar, __m512 ai,
    __m512 br, __m512 bi,
    __m512 *out_r, __m512 *out_i)
{
    *out_r = _mm512_sub_ps(_mm512_mul_ps(ar, br), _mm512_mul_ps(ai, bi));
    *out_i = _mm512_add_ps(_mm512_mul_ps(ar, bi), _mm512_mul_ps(ai, br));
}

__attribute__((target("avx512f"))) static inline void avx512_complex_div(
    __m512 ar, __m512 ai,
    __m512 br, __m512 bi,
    __m512 *out_r, __m512 *out_i)
{
    __m512 br2 = _mm512_mul_ps(br, br);
    __m512 bi2 = _mm512_mul_ps(bi, bi);
    __m512 denom = _mm512_add_ps(br2, bi2);

    __m512 num_r = _mm512_add_ps(_mm512_mul_ps(ar, br), _mm512_mul_ps(ai, bi));
    __m512 num_i = _mm512_sub_ps(_mm512_mul_ps(ai, br), _mm512_mul_ps(ar, bi));

    *out_r = _mm512_div_ps(num_r, denom);
    *out_i = _mm512_div_ps(num_i, denom);
}

__attribute__((target("avx512f"))) static void avx512_store_n_floats(float *dst, __m512 v, int n)
{
    if (n <= 0 || n > 15)
        return;

    float tmp[16];
    _mm512_storeu_ps(tmp, v);
    for (int i = 0; i < n; i++)
    {
        dst[i] = tmp[i];
    }
}

__attribute__((target("avx512f"))) float **avx512_compute_H(
    float x_range[2], float y_range[2],
    new_singularity_array_t zeros_arr, new_singularity_array_t poles_arr,
    uint16_t height, uint16_t width,
    float **H)
{
    uint32_t length = height * width;

    if (H == NULL)
    {
        H = (float **)malloc(sizeof(float *) * 2);
        if (!H)
            return NULL;
        
        H[0] = (float *)malloc(sizeof(float) * length);
        H[1] = (float *)malloc(sizeof(float) * length);
        if (!H[0] || !H[1])
        {
            free(H[0]);
            free(H[1]);
            free(H);
            return NULL;
        }
    }

    float sr16[16], si16[16];

    for (uint32_t h_i = 0; h_i < length;)
    {
        uint32_t h_di = 16;
        
        __m512 num_r = _mm512_set1_ps(1.0f);
        __m512 num_i = _mm512_set1_ps(0.0f);
        __m512 den_r = _mm512_set1_ps(1.0f);
        __m512 den_i = _mm512_set1_ps(0.0f);
        
        compute_s16_from_index(height, width, y_range, x_range, h_i, sr16, si16);

        __m512 sr16_v = _mm512_loadu_ps(sr16);
        __m512 si16_v = _mm512_loadu_ps(si16);

        for (size_t z_i = 0; z_i < zeros_arr.count; z_i++)
        {
            __m512 zr_v = _mm512_set1_ps(zeros_arr.r[z_i]);
            __m512 zi_v = _mm512_set1_ps(zeros_arr.i[z_i]);

            __m512 term_r = _mm512_sub_ps(sr16_v, zr_v);
            __m512 term_i = _mm512_sub_ps(si16_v, zi_v);

            __m512 res_r = _mm512_set1_ps(1.0f);
            __m512 res_i = _mm512_set1_ps(0.0f);
            __m512 base_r = term_r;
            __m512 base_i = term_i;
            
            int exp = zeros_arr.e[z_i];
            while (exp > 0)
            {
                if (exp & 1)
                {
                    avx512_complex_mul(res_r, res_i, base_r, base_i, &res_r, &res_i);
                }
                avx512_complex_mul(base_r, base_i, base_r, base_i, &base_r, &base_i);
                exp >>= 1;
            }
            
            term_r = res_r;
            term_i = res_i;

            __m512 m_r_v = _mm512_set1_ps(zeros_arr.m_r[z_i]);
            __m512 m_i_v = _mm512_set1_ps(zeros_arr.m_i[z_i]);
            __m512 c_r_v = _mm512_set1_ps(zeros_arr.c_r[z_i]);
            __m512 c_i_v = _mm512_set1_ps(zeros_arr.c_i[z_i]);

            // Complex multiply term * m
            __m512 tm_r, tm_i;
            avx512_complex_mul(m_r_v, m_i_v, term_r, term_i, &tm_r, &tm_i);
            
            // Add c
            term_r = _mm512_add_ps(tm_r, c_r_v);
            term_i = _mm512_add_ps(tm_i, c_i_v);

            avx512_complex_mul(num_r, num_i, term_r, term_i, &num_r, &num_i);
        }

        for (size_t p_i = 0; p_i < poles_arr.count; p_i++)
        {
            __m512 pr_v = _mm512_set1_ps(poles_arr.r[p_i]);
            __m512 pi_v = _mm512_set1_ps(poles_arr.i[p_i]);

            __m512 term_r = _mm512_sub_ps(sr16_v, pr_v);
            __m512 term_i = _mm512_sub_ps(si16_v, pi_v);

            __m512 res_r = _mm512_set1_ps(1.0f);
            __m512 res_i = _mm512_set1_ps(0.0f);
            __m512 base_r = term_r;
            __m512 base_i = term_i;
            
            int exp = poles_arr.e[p_i];
            while (exp > 0)
            {
                if (exp & 1)
                {
                    avx512_complex_mul(res_r, res_i, base_r, base_i, &res_r, &res_i);
                }
                avx512_complex_mul(base_r, base_i, base_r, base_i, &base_r, &base_i);
                exp >>= 1;
            }
            
            term_r = res_r;
            term_i = res_i;

            __m512 m_r_v = _mm512_set1_ps(poles_arr.m_r[p_i]);
            __m512 m_i_v = _mm512_set1_ps(poles_arr.m_i[p_i]);
            __m512 c_r_v = _mm512_set1_ps(poles_arr.c_r[p_i]);
            __m512 c_i_v = _mm512_set1_ps(poles_arr.c_i[p_i]);

            // Complex multiply term * m
            __m512 tm_r, tm_i;
            avx512_complex_mul(m_r_v, m_i_v, term_r, term_i, &tm_r, &tm_i);
            
            // Add c
            term_r = _mm512_add_ps(tm_r, c_r_v);
            term_i = _mm512_add_ps(tm_i, c_i_v);

            avx512_complex_mul(den_r, den_i, term_r, term_i, &den_r, &den_i);
        }

        __m512 Hr, Hi;
        avx512_complex_div(num_r, num_i, den_r, den_i, &Hr, &Hi);

        if (h_i + 16 > length)
        {
            avx512_store_n_floats(H[0] + h_i, Hr, length - h_i);
            avx512_store_n_floats(H[1] + h_i, Hi, length - h_i);
        }
        else
        {
            _mm512_storeu_ps(H[0] + h_i, Hr);
            _mm512_storeu_ps(H[1] + h_i, Hi);
        }

        if (h_di + h_i > length)
            h_di = length - h_i;

        h_i += h_di;
    }

    return H;
}
