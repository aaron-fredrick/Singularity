#include "simd.h"

__attribute__((target("sse"))) float **sse_compute_H(float x_range[2], float y_range[2], new_singularity_array_t zeros_arr, new_singularity_array_t poles_arr, uint16_t height, uint16_t width, float **H)
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
					float tmp_r = (num_r) * nr[lane] - (num_i) * ni[lane];
					float tmp_i = (num_r) * ni[lane] + (num_i) * nr[lane];
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
					float tmp_r = (den_r) * dr[lane] - (den_i) * di[lane];
					float tmp_i = (den_r) * di[lane] + (den_i) * dr[lane];
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
}