#include "transfer_function.h"
#include "cpu_detect.h"
#include "simd_sse.h"
#include "simd_avx.h"
#include "simd_avx512.h"
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <string.h>

double complex *generate_s_grid(uint16_t height, uint16_t width, double y_range[2], double x_range[2])
{
	double complex *s_grid = (double complex *)malloc(sizeof(double complex) * height * width);
	if (!s_grid) return NULL;

	double y_start = y_range[0];
	double y_end = y_range[1];
	double x_start = x_range[0];
	double x_end = x_range[1];

	double dy = (y_end - y_start) / (height - 1);
	double dx = (x_end - x_start) / (width - 1);

	for (uint16_t y = 0; y < height; ++y)
	{
		for (uint16_t x = 0; x < width; ++x)
		{
			double re = x_start + x * dx;
			double im = y_start + y * dy;
			s_grid[y * width + x] = re + im * I;
		}
	}
	return s_grid;
}

s_grid_soa_t generate_s_grid_soa(uint16_t height, uint16_t width, double y_range[2], double x_range[2])
{
	s_grid_soa_t grid;
	uint32_t length = (uint32_t)height * width;
	grid.length = length;
	
	grid.real = (float *)malloc(sizeof(float) * length);
	grid.imag = (float *)malloc(sizeof(float) * length);
	
	if (!grid.real || !grid.imag) {
		free(grid.real); free(grid.imag);
		grid.real = NULL; grid.imag = NULL; grid.length = 0;
		return grid;
	}
	
	float y_start = (float)y_range[0];
	float y_end = (float)y_range[1];
	float x_start = (float)x_range[0];
	float x_end = (float)x_range[1];
	
	float dy = (y_end - y_start) / (height - 1);
	float dx = (x_end - x_start) / (width - 1);
	
	for (uint16_t y = 0; y < height; ++y)
	{
		for (uint16_t x = 0; x < width; ++x)
		{
			uint32_t idx = y * width + x;
			grid.real[idx] = x_start + x * dx;
			grid.imag[idx] = y_start + y * dy;
		}
	}
	
	return grid;
}

void free_s_grid_soa(s_grid_soa_t *grid)
{
	if (grid) {
		free(grid->real); free(grid->imag);
		grid->real = NULL; grid->imag = NULL; grid->length = 0;
	}
}

void create_singularities(singularity_array_t *arr, size_t initial_capacity)
{
	arr->data = malloc(initial_capacity * sizeof(singularity_t));
	arr->size = 0;
	arr->capacity = initial_capacity;
}

void add_singularity(singularity_array_t *arr, singularity_t s)
{
	if (arr->size == arr->capacity) {
		arr->capacity += 2;
		arr->data = realloc(arr->data, arr->capacity * sizeof(singularity_t));
	}
	arr->data[arr->size++] = s;
}

void remove_singularity(singularity_array_t *arr, size_t index)
{
	if (index >= arr->size) return;
	for (size_t i = index; i < arr->size - 1; ++i) {
		arr->data[i] = arr->data[i + 1];
	}
	arr->size--;
}

void free_singularities(singularity_array_t *arr)
{
	free(arr->data);
	arr->data = NULL;
	arr->size = 0;
	arr->capacity = 0;
}

new_singularity_array_t convert_soa_to_simd(singularity_soa_t soa)
{
	new_singularity_array_t simd_arr;
	simd_arr.count = soa.size;
	
	if (soa.size == 0) {
		simd_arr.r = NULL; simd_arr.i = NULL; simd_arr.e = NULL;
		simd_arr.m_r = NULL; simd_arr.m_i = NULL; simd_arr.c_r = NULL; simd_arr.c_i = NULL;
		return simd_arr;
	}
	
	simd_arr.r = (float *)malloc(sizeof(float) * soa.size);
	simd_arr.i = (float *)malloc(sizeof(float) * soa.size);
	simd_arr.e = (uint8_t *)malloc(sizeof(uint8_t) * soa.size);
	simd_arr.m_r = (float *)malloc(sizeof(float) * soa.size);
	simd_arr.m_i = (float *)malloc(sizeof(float) * soa.size);
	simd_arr.c_r = (float *)malloc(sizeof(float) * soa.size);
	simd_arr.c_i = (float *)malloc(sizeof(float) * soa.size);
	
	for (size_t j = 0; j < soa.size; j++) {
		simd_arr.r[j] = (float)soa.real[j];
		simd_arr.i[j] = (float)soa.imag[j];
		simd_arr.e[j] = soa.e[j];
		simd_arr.m_r[j] = (float)soa.m_re[j];
		simd_arr.m_i[j] = (float)soa.m_im[j];
		simd_arr.c_r[j] = (float)soa.c_re[j];
		simd_arr.c_i[j] = (float)soa.c_im[j];
	}
	
	return simd_arr;
}

void free_simd_array(new_singularity_array_t *arr)
{
	free(arr->r); free(arr->i); free(arr->e);
	free(arr->m_r); free(arr->m_i); free(arr->c_r); free(arr->c_i);
	arr->r = NULL; arr->i = NULL; arr->e = NULL;
	arr->m_r = NULL; arr->m_i = NULL; arr->c_r = NULL; arr->c_i = NULL;
	arr->count = 0;
}

float **compute_H_simd_direct(s_grid_soa_t s_grid, singularity_soa_t zeros_soa, singularity_soa_t poles_soa, uint16_t height, uint16_t width, float **H)
{
	static cpu_features_t features = CPU_FEATURE_NONE;
	static int detected = 0;
	if (!detected) {
		features = detect_cpu_features();
		detected = 1;
	}
	
	uint32_t length = height * width;
	new_singularity_array_t simd_zeros = convert_soa_to_simd(zeros_soa);
	new_singularity_array_t simd_poles = convert_soa_to_simd(poles_soa);
	
	float x_range[2] = {s_grid.real[0], s_grid.real[length - 1]};
	float y_range[2] = {s_grid.imag[0], s_grid.imag[length - 1]};
	
	if (features & CPU_FEATURE_AVX512F) {
		H = avx512_compute_H(x_range, y_range, simd_zeros, simd_poles, height, width, H);
	} else if (features & CPU_FEATURE_AVX) {
		H = avx_compute_H(x_range, y_range, simd_zeros, simd_poles, height, width, H);
	} else if (features & CPU_FEATURE_SSE2) {
		H = sse2_compute_H(x_range, y_range, simd_zeros, simd_poles, height, width, H);
	}
	
	free_simd_array(&simd_zeros);
	free_simd_array(&simd_poles);
	
	return H;
}

void normalize_H_soa(double *in_re, double *in_im, size_t size, double *out_re, double *out_im)
{
	double max_mag = DBL_MIN;

	for (size_t i = 0; i < size; ++i) {
		double mag = sqrt(in_re[i]*in_re[i] + in_im[i]*in_im[i]);
		if (mag > max_mag) max_mag = mag;
	}

	double denom = max_mag + 1e-6;
    
	for (size_t i = 0; i < size; ++i) {
		double mag = sqrt(in_re[i]*in_re[i] + in_im[i]*in_im[i]);
		double scale = mag > 1e-9 ? (mag / denom) / mag : 0.0;
		if (mag / denom > 1.0) scale = 1.0 / mag;
		out_re[i] = in_re[i] * scale;
		out_im[i] = in_im[i] * scale;
	}
}

// Fast Histogram-based 99th Percentile
static double find_99th_percentile(double *log_mag, size_t size) {
    if (size == 0) return 1.0;
    
    double min_v = 1e30, max_v = -1e30;
    for (size_t i = 0; i < size; i++) {
        if (log_mag[i] < min_v) min_v = log_mag[i];
        if (log_mag[i] > max_v) max_v = log_mag[i];
    }
    
    if (max_v <= min_v) return max_v;
    
    #define HIST_BINS 1024
    int histogram[HIST_BINS] = {0};
    double range = max_v - min_v;
    double bin_width = range / HIST_BINS;
    
    for (size_t i = 0; i < size; i++) {
        int bin = (int)((log_mag[i] - min_v) / bin_width);
        if (bin < 0) bin = 0;
        if (bin >= HIST_BINS) bin = HIST_BINS - 1;
        histogram[bin]++;
    }
    
    size_t count = 0;
    size_t target = (size_t)(size * 0.99);
    for (int k = 0; k < HIST_BINS; k++) {
        count += histogram[k];
        if (count >= target) {
            return min_v + (k + 1) * bin_width;
        }
    }
    return max_v;
}

void normalize_H_log_soa(double *in_re, double *in_im, size_t size, double *out_re, double *out_im)
{
	double *log_mag = (double *)malloc(size * sizeof(double));
	if (!log_mag) return;

	for (size_t i = 0; i < size; ++i) {
		log_mag[i] = log1p(sqrt(in_re[i]*in_re[i] + in_im[i]*in_im[i]));
	}

    // Replace qsort with histogram approach
	double max_val = find_99th_percentile(log_mag, size);

	double denom = max_val + 1e-6;
    
	for (size_t i = 0; i < size; ++i) {
		double orig_mag = sqrt(in_re[i]*in_re[i] + in_im[i]*in_im[i]);
		double new_mag = log_mag[i] / denom;
		if (new_mag > 1.0) new_mag = 1.0;
		if (new_mag < 0.0) new_mag = 0.0;
		
		double scale = orig_mag > 1e-9 ? new_mag / orig_mag : 0.0;
		out_re[i] = in_re[i] * scale;
		out_im[i] = in_im[i] * scale;
	}
	free(log_mag);
}

void normalize_H_log_steps_soa(double *in_re, double *in_im, size_t size, int steps, double *out_re, double *out_im)
{
	if (steps <= 0) return;
	double *log_mag = (double *)malloc(size * sizeof(double));
	if (!log_mag) return;

	for (size_t i = 0; i < size; ++i) {
		log_mag[i] = log1p(sqrt(in_re[i]*in_re[i] + in_im[i]*in_im[i]));
	}

	double max_val = find_99th_percentile(log_mag, size);
	double denom = max_val + 1e-6;
	double step_size = 1.0 / steps;

	for (size_t i = 0; i < size; ++i) {
		double orig_mag = sqrt(in_re[i]*in_re[i] + in_im[i]*in_im[i]);
		double raw_mag = log_mag[i] / denom;
		if (raw_mag > 1.0) raw_mag = 1.0;
		if (raw_mag < 0.0) raw_mag = 0.0;

		int step_index = (int)(raw_mag * steps + 0.5);
		double quantized_mag = step_index * step_size;

		double scale = orig_mag > 1e-9 ? quantized_mag / orig_mag : 0.0;
		out_re[i] = in_re[i] * scale;
		out_im[i] = in_im[i] * scale;
	}
	free(log_mag);
}

static inline uint8_t clamp(float x) {
	return (uint8_t)(x < 0 ? 0 : (x > 255 ? 255 : x));
}

void hsv_to_rgb_uint8(uint8_t H, uint8_t S, uint8_t V, uint8_t *R, uint8_t *G, uint8_t *B)
{
	float h = H * 2.0f; 
	float s = S / 255.0f;
	float v = V / 255.0f;
	float c = v * s;
	float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
	float m = v - c;
	float r = 0, g = 0, b = 0;

	if (h < 60) { r = c; g = x; b = 0; }
	else if (h < 120) { r = x; g = c; b = 0; }
	else if (h < 180) { r = 0; g = c; b = x; }
	else if (h < 240) { r = 0; g = x; b = c; }
	else if (h < 300) { r = x; g = 0; b = c; }
	else { r = c; g = 0; b = x; }

	*R = clamp((r + m) * 255.0f);
	*G = clamp((g + m) * 255.0f);
	*B = clamp((b + m) * 255.0f);
}

void H_g_img(double *re, double *im, size_t size, img_t H_img)
{
	for (uint32_t i = 0; i < size; i++)
	{
		uint32_t c = (uint32_t)(sqrt(re[i]*re[i] + im[i]*im[i]) * 255);
		if (c > 255) c = 255;
		(H_img.data)[i] = (255 << 24) | (c << 16) | (c << 8) | c; 
	}
}

void H_c1_img(double *re, double *im, size_t size, img_t H_img)
{
	for (uint32_t i = 0; i < size; i++)
	{
		uint32_t r = (uint32_t)(((re[i] + 1) / 2) * 255u);
		uint32_t b = (uint32_t)(((im[i] + 1) / 2) * 255u);
		if (r > 255) r = 255; 
		if (b > 255) b = 255;
		(H_img.data)[i] = (255 << 24) | (r << 16) | b; 
	}
}

void H_c2_img(double *re, double *im, size_t size, img_t H_img)
{
	for (uint32_t i = 0; i < size; i++)
	{
		double mag = sqrt(re[i]*re[i] + im[i]*im[i]);
		uint32_t r = (uint32_t)(((re[i] + 1) / 2) * mag * 255u);
		uint32_t b = (uint32_t)(((im[i] + 1) / 2) * mag * 255u);
		if (r > 255) r = 255;
		if (b > 255) b = 255;
		(H_img.data)[i] = (255 << 24) | (r << 16) | b; 
	}
}

void H_c3_img(double *re, double *im, size_t size, img_t H_img)
{
	for (uint32_t i = 0; i < size; i++)
	{
		double mag = sqrt(re[i]*re[i] + im[i]*im[i]);
		uint32_t r = (uint32_t)(((re[i] + 1) / 2) * 255u);
		uint32_t g = (uint32_t)(mag * 255u); 
		uint32_t b = (uint32_t)(((im[i] + 1) / 2) * 255u);
		if (r > 255) r = 255;
		if (b > 255) b = 255;
		if (g > 255) g = 255;
		(H_img.data)[i] = (255 << 24) | (r << 16) | (g << 8) | b; 
	}
}

void H_c4_img(double *re, double *im, size_t size, img_t H_img)
{
	for (uint32_t i = 0; i < size; i++)
	{
		double mag = sqrt(re[i]*re[i] + im[i]*im[i]);
		uint32_t r = (uint32_t)(((re[i] + 1) / 2) * mag * 255u);
		uint32_t g = (uint32_t)((1 - mag) * 255u); 
		uint32_t b = (uint32_t)(((im[i] + 1) / 2) * mag * 255u);
		if (r > 255) r = 255;
		if (b > 255) b = 255;
		if (g > 255) g = 255;
		(H_img.data)[i] = (255 << 24) | (r << 16) | (g << 8) | b; 
	}
}

void H_c5_img(double *re, double *im, size_t size, img_t H_img)
{
	for (uint32_t i = 0; i < size; i++)
	{
		double mag = sqrt(re[i]*re[i] + im[i]*im[i]);
		uint8_t h = (uint8_t)(((re[i] + 1) / 2) * (2.f / 3.f) * 255);
		uint8_t s = (uint8_t)(((im[i] + 1) / 2) * 255);
		uint8_t v = (uint8_t)(mag * 255);

		uint8_t r, g, b;
		hsv_to_rgb_uint8(h, s, v, &r, &g, &b);
		(H_img.data)[i] = (255 << 24) | (uint32_t)r << 16 | (uint32_t)g << 8 | b; 
	}
}