#include "simd_sse.h"

// --- SSE family (128-bit) ---
static void compute_s4_from_index(
	uint16_t height,
	uint16_t width,
	float y_range[2],
	float x_range[2],
	uint32_t start_index, // starting linear index
	float out_re[4],	  // output real parts
	float out_im[4]		  // output imaginary parts
)
{
	float dy = (y_range[1] - y_range[0]) / (height - 1);
	float dx = (x_range[1] - x_range[0]) / (width - 1);

	for (int i = 0; i < 4; i++)
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
__attribute__((target("sse")))
static void sse_complex_mul(
	__m128 ar, __m128 ai,
	__m128 br, __m128 bi,
	__m128 *out_r, __m128 *out_i)
{
	__m128 tmp_r = _mm_sub_ps(_mm_mul_ps(ar, br), _mm_mul_ps(ai, bi));
	__m128 tmp_i = _mm_add_ps(_mm_mul_ps(ar, bi), _mm_mul_ps(ai, br));
	*out_r = tmp_r;
	*out_i = tmp_i;
}

// Divide two complex SIMD numbers: (ar + i ai) / (br + i bi)
__attribute__((target("sse")))
static void sse_complex_div(
	__m128 ar, __m128 ai,
	__m128 br, __m128 bi,
	__m128 *out_r, __m128 *out_i)
{
	// denominator = br^2 + bi^2
	__m128 br2 = _mm_mul_ps(br, br);
	__m128 bi2 = _mm_mul_ps(bi, bi);
	__m128 denom = _mm_add_ps(br2, bi2);

	// real numerator: (ar*br + ai*bi)
	__m128 num_r = _mm_add_ps(_mm_mul_ps(ar, br), _mm_mul_ps(ai, bi));

	// imag numerator: (ai*br - ar*bi)
	__m128 num_i = _mm_sub_ps(_mm_mul_ps(ai, br), _mm_mul_ps(ar, bi));

	// divide both by denominator
	*out_r = _mm_div_ps(num_r, denom);
	*out_i = _mm_div_ps(num_i, denom);
}

// Store 1-4 lanes from v into dst
__attribute__((target("sse")))
static void sse_store_n_floats(float *dst, __m128 v, int n)
{
	if (n <= 0 || n > 3)
		return; // invalid

	// lane 0 always exists
	dst[0] = _mm_cvtss_f32(v);

	if (n >= 2)
		dst[1] = _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)));

	if (n >= 3)
		dst[2] = _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));
}

__attribute__((target("sse2"))) float **sse2_compute_H(float x_range[2], float y_range[2], new_singularity_array_t zeros_arr, new_singularity_array_t poles_arr, uint16_t height, uint16_t width, float **H)
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

	float sr4[4], si4[4];

	for (uint32_t h_i = 0, h_di = 4; h_i < length;)
	{
		// load s (4 regs, total 4)
		__m128 num_r = _mm_set1_ps(1.0f);
		__m128 num_i = _mm_set1_ps(1.0f);
		__m128 den_r = _mm_set1_ps(1.0f);
		__m128 den_i = _mm_set1_ps(1.0f);
		compute_s4_from_index(height, width, y_range, x_range, h_i, sr4, si4);

		// load s (2 regs, total 6)
		__m128 sr4_v = _mm_loadu_ps(sr4); // load 4 real parts
		__m128 si4_v = _mm_loadu_ps(si4); // load 4 imaginary parts

		for (size_t z_i = 0; z_i < zeros_arr.count; z_i += 1)
		{
			// terms (2 regs, total 8)
			__m128 term_r, term_i;

			// load 1 zeros (2 regs, total 10)
			__m128 zr_v = _mm_set1_ps(zeros_arr.r[z_i]);
			__m128 zi_v = _mm_set1_ps(zeros_arr.i[z_i]);

			term_r = _mm_sub_ps(sr4_v, zr_v); // term_r now holds s - z_r
			term_i = _mm_sub_ps(si4_v, zi_v); // term_i now holds s - z_i

			// --- do exponentiation per lane (binary loop) ---
			__m128 res_r = _mm_set1_ps(1.0f);
			__m128 res_i = _mm_set1_ps(0.0f);
			__m128 base_r = term_r;
			__m128 base_i = term_i;
			int exp = zeros_arr.e[z_i];
			while (exp > 0) // O(logn)
			{
				if (exp & 1)
				{
					sse_complex_mul(res_r, res_i, base_r, base_i, &res_r, &res_i);
				}
				sse_complex_mul(base_r, base_i, base_r, base_i, &base_r, &base_i);
				exp >>= 1;
			}
			term_r = res_r;
			term_i = res_i;

			// (z.m * prod + z.c), with m,c real, (2 regs, total 14)
			__m128 m_v = _mm_set1_ps(zeros_arr.m[z_i]);
			__m128 c_v = _mm_set1_ps(zeros_arr.c[z_i]);

			term_r = _mm_mul_ps(m_v, term_r);
			term_i = _mm_mul_ps(m_v, term_i);

			term_r = _mm_add_ps(term_r, c_v);
			term_i = term_i; // TODO: add c to imaginary part?

			// num *= (that), applied per lane (2 regs, total 16)
			sse_complex_mul(num_r, num_i, term_r, term_i, &num_r, &num_i);
		}
		// local regs released (-10 regs, total 6)

		for (size_t p_i = 0; p_i < poles_arr.count; p_i += 1)
		{
			// terms (2 regs, total 8)
			__m128 term_r, term_i;

			// load 1 poles (2 regs, total 10)
			__m128 pr_v = _mm_set1_ps(poles_arr.r[p_i]);
			__m128 pi_v = _mm_set1_ps(poles_arr.i[p_i]);

			term_r = _mm_sub_ps(sr4_v, pr_v); // term_r now holds s - p_r
			term_i = _mm_sub_ps(si4_v, pi_v); // term_i now holds s - p_i

			// --- do exponentiation per lane (binary loop) ---
			__m128 res_r = _mm_set1_ps(1.0f);
			__m128 res_i = _mm_set1_ps(0.0f);
			__m128 base_r = term_r;
			__m128 base_i = term_i;
			int exp = poles_arr.e[p_i];
			while (exp > 0) // O(logn)
			{
				if (exp & 1)
				{
					sse_complex_mul(res_r, res_i, base_r, base_i, &res_r, &res_i);
				}
				sse_complex_mul(base_r, base_i, base_r, base_i, &base_r, &base_i);
				exp >>= 1;
			}
			term_r = res_r;
			term_i = res_i;

			// (p.m * prod + p.c), with m,c real, (2 regs, total 14)
			__m128 m_v = _mm_set1_ps(poles_arr.m[p_i]);
			__m128 c_v = _mm_set1_ps(poles_arr.c[p_i]);

			term_r = _mm_mul_ps(m_v, term_r);
			term_i = _mm_mul_ps(m_v, term_i);

			term_r = _mm_add_ps(term_r, c_v);
			term_i = term_i; // TODO: add c to imaginary part?

			// den *= (that), applied per lane (2 regs, total 16)
			sse_complex_mul(den_r, den_i, term_r, term_i, &den_r, &den_i);
		}
		// local regs released (-10 regs, total 6)

		__m128 Hr, Hi;
		sse_complex_div(num_r, num_i, den_r, den_i, &Hr, &Hi);

		if (h_i + 4 > length)
		{
			sse_store_n_floats(H[0] + h_i, Hr, length - h_i);
			sse_store_n_floats(H[1] + h_i, Hi, length - h_i);
		}
		else
		{
			_mm_storeu_ps(H[0] + h_i, Hr);
			_mm_storeu_ps(H[1] + h_i, Hi);
		}

		if (h_di + h_i > length)
			h_di = length - h_i; // adjust for last few elements

		h_i += h_di;
	}

	return H;
}

/*  float **sse2_compute_H(float x_range[2], float y_range[2], new_singularity_array_t zeros_arr, new_singularity_array_t poles_arr, uint16_t height, uint16_t width, float **H)
{
	uint32_t length = height * width;

	float y_start = y_range[0];
	float y_end = y_range[1];
	float x_start = x_range[0];
	float x_end = x_range[1];

	// Calculate the step sizes
	float dy = (y_end - y_start) / (height - 1);
	float dx = (x_end - x_start) / (width - 1);

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

	size_t i = 0;
	for (float sr = x_start; sr <= x_end; sr += dx)
	{
		for (float si = y_start; si <= y_end; si += dy)
		{
			float num_r = 1.0f, num_i = 0.0f, den_r = 1.0f, den_i = 0.0f;

			// Zeros
			for (size_t z_i = 0; z_i < zeros_arr.count; z_i += 4)
			{
				// load 4 zeros (2 regs, total 2)
				__m128 zr_v = _mm_loadu_ps(zeros_arr.r + z_i);
				__m128 zi_v = _mm_loadu_ps(zeros_arr.i + z_i);

				// term = s - z (2 regs, total 4)
				__m128 term_r = _mm_set1_ps(sr);   // sr -> term_r
				__m128 term_i = _mm_set1_ps(si);   // si -> term_i
				term_r = _mm_sub_ps(term_r, zr_v); // term_r now holds s - z_r
				term_i = _mm_sub_ps(term_i, zi_v); // term_i now holds s - z_i

				// --- do exponentiation per lane (scalar loop) ---
				float tr[4], ti[4];
				float pr[4], pi[4];
				_mm_storeu_ps(tr, term_r);
				_mm_storeu_ps(ti, term_i);

				for (int lane = 0; lane < 4; lane++)
				{
					float r = 1.0f, im = 0.0f;
					for (int k = 0; k < zeros_arr.e[z_i + lane]; k++)
					{
						float tmp_r = r * tr[lane] - im * ti[lane];
						float tmp_i = r * ti[lane] + im * tr[lane];
						r = tmp_r;
						im = tmp_i;
					}
					pr[lane] = r;
					pi[lane] = im;
				}

				term_r = _mm_loadu_ps(pr);
				term_i = _mm_loadu_ps(pi);

				// (z.m * prod + z.c), with m,c real, (2 regs, total 6)
				__m128 m_v = _mm_loadu_ps(zeros_arr.m + z_i);
				__m128 c_v = _mm_loadu_ps(zeros_arr.c + z_i);

				term_r = _mm_mul_ps(m_v, term_r);
				term_i = _mm_mul_ps(m_v, term_i);

				term_r = _mm_add_ps(term_r, c_v);
				term_i = term_i;

				// num *= (that), applied per lane
				float nr[4], ni[4];
				_mm_storeu_ps(nr, term_r);
				_mm_storeu_ps(ni, term_i);

				for (int lane = 0; lane < 4; lane++)
				{
					float tmp_r = (num_r)*nr[lane] - (num_i)*ni[lane];
					float tmp_i = (num_r)*ni[lane] + (num_i)*nr[lane];
					num_r = tmp_r;
					num_i = tmp_i;
				}
			}
			// TODO: tail of zeros

			// Poles
			for (size_t p_i = 0; p_i < poles_arr.count; p_i += 4)
			{
				// load 4 poles (2 regs, total 2)
				__m128 pr_v = _mm_loadu_ps(poles_arr.r + p_i);
				__m128 pi_v = _mm_loadu_ps(poles_arr.i + p_i);

				// term = s - z (2 regs, total 4)
				__m128 term_r = _mm_set1_ps(sr);   // sr -> term_r
				__m128 term_i = _mm_set1_ps(si);   // si -> term_i
				term_r = _mm_sub_ps(term_r, pr_v); // term_r now holds s - p_r
				term_i = _mm_sub_ps(term_i, pi_v); // term_i now holds s - p_i

				// --- do exponentiation per lane (scalar loop) ---
				float tr[4], ti[4];
				float pr[4], pi[4];
				_mm_storeu_ps(tr, term_r);
				_mm_storeu_ps(ti, term_i);

				for (int lane = 0; lane < 4; lane++)
				{
					float r = 1.0f, im = 0.0f;
					for (int k = 0; k < poles_arr.e[p_i + lane]; k++)
					{
						float tmp_r = r * tr[lane] - im * ti[lane];
						float tmp_i = r * ti[lane] + im * tr[lane];
						r = tmp_r;
						im = tmp_i;
					}
					pr[lane] = r;
					pi[lane] = im;
				}

				term_r = _mm_loadu_ps(pr);
				term_i = _mm_loadu_ps(pi);

				// (p.m * prod + p.c), with m,c real, (2 regs, total 6)
				__m128 m_v = _mm_loadu_ps(poles_arr.m + p_i);
				__m128 c_v = _mm_loadu_ps(poles_arr.c + p_i);

				term_r = _mm_mul_ps(m_v, term_r);
				term_i = _mm_mul_ps(m_v, term_i);

				term_r = _mm_add_ps(term_r, c_v);
				term_i = term_i;

				// den *= (that), applied per lane
				float dr[4], di[4];
				_mm_storeu_ps(dr, term_r);
				_mm_storeu_ps(di, term_i);

				for (int lane = 0; lane < 4; lane++)
				{
					float tmp_r = (den_r)*dr[lane] - (den_i)*di[lane];
					float tmp_i = (den_r)*di[lane] + (den_i)*dr[lane];
					den_r = tmp_r;
					den_i = tmp_i;
				}
			}
			// TODO: tail of poles

			i++;

			H[0][i] = num_r / (den_r + 1e-15f); // Avoid division by zero
			H[1][i] = num_i / (den_i + 1e-15f); // Avoid division by zero
		}
	}

	return H;
} */

