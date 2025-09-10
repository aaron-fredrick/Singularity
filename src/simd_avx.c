#include "simd_avx.h"

// --- AVX family (256-bit) ---
static void compute_s8_from_index(
	uint16_t height,
	uint16_t width,
	float y_range[2],
	float x_range[2],
	uint32_t start_index, // starting linear index
	float out_re[8],	  // output real parts
	float out_im[8]		  // output imaginary parts
)
{
	float dy = (y_range[1] - y_range[0]) / (height - 1);
	float dx = (x_range[1] - x_range[0]) / (width - 1);

	for (int i = 0; i < 8; i++)
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

// Multiply two complex SIMD numbers: (ar + i ai) * (br + i bi)
__attribute__((target("avx"))) static void avx_complex_mul(
	__m256 ar, __m256 ai,
	__m256 br, __m256 bi,
	__m256 *out_r, __m256 *out_i)
{
	__m256 tmp_r = _mm256_sub_ps(_mm256_mul_ps(ar, br), _mm256_mul_ps(ai, bi));
	__m256 tmp_i = _mm256_add_ps(_mm256_mul_ps(ar, bi), _mm256_mul_ps(ai, br));
	*out_r = tmp_r;
	*out_i = tmp_i;
}

// Divide two complex SIMD numbers: (ar + i ai) / (br + i bi)
__attribute__((target("avx"))) static void avx_complex_div(
	__m256 ar, __m256 ai,
	__m256 br, __m256 bi,
	__m256 *out_r, __m256 *out_i)
{
	// denominator = br^2 + bi^2
	__m256 br2 = _mm256_mul_ps(br, br);
	__m256 bi2 = _mm256_mul_ps(bi, bi);
	__m256 denom = _mm256_add_ps(br2, bi2);

	// real numerator: (ar*br + ai*bi)
	__m256 num_r = _mm256_add_ps(_mm256_mul_ps(ar, br), _mm256_mul_ps(ai, bi));

	// imag numerator: (ai*br - ar*bi)
	__m256 num_i = _mm256_sub_ps(_mm256_mul_ps(ai, br), _mm256_mul_ps(ar, bi));

	// divide both by denominator
	*out_r = _mm256_div_ps(num_r, denom);
	*out_i = _mm256_div_ps(num_i, denom);
}

// Store 1-4 lanes from v into dst
__attribute__((target("avx"))) static void avx_store_n_floats(float *dst, __m256 v, int n)
{
	if (n <= 0 || n > 7)
		return; // invalid

	float tmp[8];
	_mm256_storeu_ps(tmp, v); // store all 8 floats
	for (int i = 0; i < n; i++)
	{
		dst[i] = tmp[i];
	}
}

__attribute__((target("avx"))) float **avx_compute_H(
	float x_range[2], float y_range[2],
	new_singularity_array_t zeros_arr, new_singularity_array_t poles_arr,
	uint16_t height, uint16_t width,
	float **H)
{
	uint32_t length = height * width;

	// setting up H
	if (H == NULL)
	{
		H = (float **)malloc(sizeof(float *) * 2);
		if (!H)
		{
			return NULL;
		}
		H[0] = malloc(sizeof(float) * length);
		H[1] = malloc(sizeof(float) * length);
		if (!H[0] || !H[1])
		{
			free(H[0]);
			free(H[1]);
			free(H);
			return NULL;
		}
	}

	float sr8[8], si8[8];

	for (uint32_t h_i = 0, h_di = 8; h_i < length;)
	{
		// load s (4 regs, total 4)
		__m256 num_r = _mm256_set1_ps(1.0f);
		__m256 num_i = _mm256_set1_ps(1.0f);
		__m256 den_r = _mm256_set1_ps(1.0f);
		__m256 den_i = _mm256_set1_ps(1.0f);
		compute_s8_from_index(height, width, y_range, x_range, h_i, sr8, si8);

		// load s (2 regs, total 6)
		__m256 sr8_v = _mm256_loadu_ps(sr8); // load 4 real parts
		__m256 si8_v = _mm256_loadu_ps(si8); // load 4 imaginary parts

		for (size_t z_i = 0; z_i < zeros_arr.count; z_i += 1)
		{
			// terms (2 regs, total 8)
			__m256 term_r, term_i;

			// load 1 zeros (2 regs, total 10)
			__m256 zr_v = _mm256_set1_ps(zeros_arr.r[z_i]);
			__m256 zi_v = _mm256_set1_ps(zeros_arr.i[z_i]);

			term_r = _mm256_sub_ps(sr8_v, zr_v); // term_r now holds s - z_r
			term_i = _mm256_sub_ps(si8_v, zi_v); // term_i now holds s - z_i

			// --- do exponentiation per lane (binary loop) ---
			__m256 res_r = _mm256_set1_ps(1.0f);
			__m256 res_i = _mm256_set1_ps(0.0f);
			__m256 base_r = term_r;
			__m256 base_i = term_i;
			int exp = zeros_arr.e[z_i];
			while (exp > 0) // O(logn)
			{
				if (exp & 1)
				{
					avx_complex_mul(res_r, res_i, base_r, base_i, &res_r, &res_i);
				}
				avx_complex_mul(base_r, base_i, base_r, base_i, &base_r, &base_i);
				exp >>= 1;
			}
			term_r = res_r;
			term_i = res_i;

			// (z.m * prod + z.c), with m,c real, (2 regs, total 14)
			__m256 m_v = _mm256_set1_ps(zeros_arr.m[z_i]);
			__m256 c_v = _mm256_set1_ps(zeros_arr.c[z_i]);

			term_r = _mm256_mul_ps(m_v, term_r);
			term_i = _mm256_mul_ps(m_v, term_i);

			term_r = _mm256_add_ps(term_r, c_v);
			term_i = term_i; // TODO: add c to imaginary part?

			// num *= (that), applied per lane (2 regs, total 16)
			avx_complex_mul(num_r, num_i, term_r, term_i, &num_r, &num_i);
		}
		// local regs released (-10 regs, total 6)

		for (size_t p_i = 0; p_i < poles_arr.count; p_i += 1)
		{
			// terms (2 regs, total 8)
			__m256 term_r, term_i;

			// load 1 poles (2 regs, total 10)
			__m256 pr_v = _mm256_set1_ps(poles_arr.r[p_i]);
			__m256 pi_v = _mm256_set1_ps(poles_arr.i[p_i]);

			term_r = _mm256_sub_ps(sr8_v, pr_v); // term_r now holds s - p_r
			term_i = _mm256_sub_ps(si8_v, pi_v); // term_i now holds s - p_i

			// --- do exponentiation per lane (binary loop) ---
			__m256 res_r = _mm256_set1_ps(1.0f);
			__m256 res_i = _mm256_set1_ps(0.0f);
			__m256 base_r = term_r;
			__m256 base_i = term_i;
			int exp = poles_arr.e[p_i];
			while (exp > 0) // O(logn)
			{
				if (exp & 1)
				{
					avx_complex_mul(res_r, res_i, base_r, base_i, &res_r, &res_i);
				}
				avx_complex_mul(base_r, base_i, base_r, base_i, &base_r, &base_i);
				exp >>= 1;
			}
			term_r = res_r;
			term_i = res_i;

			// (p.m * prod + p.c), with m,c real, (2 regs, total 14)
			__m256 m_v = _mm256_set1_ps(poles_arr.m[p_i]);
			__m256 c_v = _mm256_set1_ps(poles_arr.c[p_i]);

			term_r = _mm256_mul_ps(m_v, term_r);
			term_i = _mm256_mul_ps(m_v, term_i);

			term_r = _mm256_add_ps(term_r, c_v);
			term_i = term_i; // TODO: add c to imaginary part?

			// den *= (that), applied per lane (2 regs, total 16)
			avx_complex_mul(den_r, den_i, term_r, term_i, &den_r, &den_i);
		}
		// local regs released (-10 regs, total 6)

		__m256 Hr, Hi;
		avx_complex_div(num_r, num_i, den_r, den_i, &Hr, &Hi);

		if (h_i + 4 > length)
		{
			avx_store_n_floats(H[0] + h_i, Hr, length - h_i);
			avx_store_n_floats(H[1] + h_i, Hi, length - h_i);
		}
		else
		{
			_mm256_storeu_ps(H[0] + h_i, Hr);
			_mm256_storeu_ps(H[1] + h_i, Hi);
		}

		if (h_di + h_i > length)
			h_di = length - h_i; // adjust for last few elements

		h_i += h_di;
	}

	return H;
}