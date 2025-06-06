#include "transfer_function.h"

double complex *generate_s_grid(uint16_t height, uint16_t width, double y_range[2], double x_range[2])
{
	double complex *s_grid = (double complex *)malloc(sizeof(double complex) * height * width);

	if (!s_grid)
		return NULL;

	double y_start = y_range[0];
	double y_end = y_range[1];
	double x_start = x_range[0];
	double x_end = x_range[1];

	// Calculate the step sizes
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

void create_singularities(singularity_array_t *arr, size_t initial_capacity)
{
	arr->data = malloc(initial_capacity * sizeof(singularity_t));
	arr->size = 0;
	arr->capacity = initial_capacity;
}

void add_singularity(singularity_array_t *arr, singularity_t s)
{
	if (arr->size == arr->capacity)
	{
		arr->capacity += 2;
		arr->data = realloc(arr->data, arr->capacity * sizeof(singularity_t));
	}
	arr->data[arr->size++] = s;
}

void remove_singularity(singularity_array_t *arr, size_t index)
{
	if (index >= arr->size)
		return;
	for (size_t i = index; i < arr->size - 1; ++i)
	{
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

double complex *compute_H(double complex *s_grid, singularity_array_t zeros_arr, singularity_array_t poles_arr, uint16_t height, uint16_t width, double complex *H)
{
	uint32_t length = height * width;

	if (H == NULL)
	{
		H = malloc(sizeof(double complex) * length);
		if (!H)
			return NULL;
	}

	for (uint32_t i = 0; i < length; i++)
	{
		double complex s = s_grid[i];
		double complex num = 1.0 + 0.0 * I;
		double complex den = 1.0 + 0.0 * I;

		// Zeros
		for (size_t z_i = 0; z_i < zeros_arr.size; z_i++)
		{
			singularity_t z = zeros_arr.data[z_i];
			double complex term = s - z.val;
			double complex prod = 1.0 + 0.0 * I;

			for (uint8_t e = 0; e < z.e; e++)
			{
				prod *= term;
			}

			num *= (z.m * prod + z.c);
		}

		// Poles
		for (size_t p_i = 0; p_i < poles_arr.size; p_i++)
		{
			singularity_t p = poles_arr.data[p_i];
			double complex term = s - p.val;
			double complex prod = 1.0 + 0.0 * I;

			for (uint8_t e = 0; e < p.e; e++)
			{
				prod *= term;
			}

			den *= (p.m * prod + p.c);
		}

		H[i] = num / (den + 1e-15); // Avoid division by zero
	}

	return H;
}

double complex *normalize_H_complex(const double complex *H, size_t size, double complex *normalized)
{
	if (normalized == NULL)
	{
		normalized = (double complex *)malloc(sizeof(double complex) * size);
		if (!normalized)
			return NULL;
	}

	double max_mag = DBL_MIN;

	// First pass: find max magnitude
	for (size_t i = 0; i < size; ++i)
	{
		double mag = cabs(H[i]);
		if (mag > max_mag)
			max_mag = mag;
	}

	// Second pass: normalize preserving angle
	double denom = max_mag + 1e-6;
	for (size_t i = 0; i < size; ++i)
	{
		double mag = cabs(H[i]);
		double phase = carg(H[i]);
		double scaled_mag = mag / denom;

		// Clamp (optional)
		if (scaled_mag > 1.0)
			scaled_mag = 1.0;

		normalized[i] = scaled_mag * cexp(I * phase);
	}

	return normalized;
}

// Compare function for qsort
int compare_double(const void *a, const void *b)
{
	double diff = *(double *)a - *(double *)b;
	return (diff > 0) - (diff < 0);
}

// Normalize using log1p(magnitude) and 99th percentile scaling
double complex *normalize_H_log_complex(const double complex *H, size_t size, double complex *normalized)
{
	if (!H || size == 0)
		return NULL;

	double *log_mag = (double *)malloc(size * sizeof(double));
	if (!log_mag)
		return NULL;

	// Compute log1p of magnitude
	for (size_t i = 0; i < size; ++i)
	{
		log_mag[i] = log1p(cabs(H[i]));
	}

	// Copy for sorting
	double *sorted = (double *)malloc(size * sizeof(double));
	if (!sorted)
	{
		free(log_mag);
		return NULL;
	}
	memcpy(sorted, log_mag, size * sizeof(double));
	qsort(sorted, size, sizeof(double), compare_double);

	// Approximate 99th percentile
	size_t idx = (size_t)(0.99 * size);
	if (idx >= size)
		idx = size - 1;
	double max_val = sorted[idx];

	free(sorted);

	// Allocate result if needed
	if (normalized == NULL)
	{
		normalized = (double complex *)malloc(size * sizeof(double complex));
		if (!normalized)
		{
			free(log_mag);
			return NULL;
		}
	}

	double denom = max_val + 1e-6;
	for (size_t i = 0; i < size; ++i)
	{
		double mag = log_mag[i] / denom;
		if (mag > 1.0)
			mag = 1.0;
		if (mag < 0.0)
			mag = 0.0;

		// Preserve original phase
		double phase = carg(H[i]);
		normalized[i] = mag * cexp(I * phase);
	}

	free(log_mag);
	return normalized;
}

// Clamp helper
static inline uint8_t clamp(float x)
{
	return (uint8_t)(x < 0 ? 0 : (x > 255 ? 255 : x));
}

// Convert HSV (uint8 format) to RGB (uint8 format)
void hsv_to_rgb_uint8(uint8_t H, uint8_t S, uint8_t V, uint8_t *R, uint8_t *G, uint8_t *B)
{
	float h = H * 2.0f; // Scale H from [0,179] to [0,360]
	float s = S / 255.0f;
	float v = V / 255.0f;

	float c = v * s;
	float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
	float m = v - c;

	float r = 0, g = 0, b = 0;

	if (h < 60)
	{
		r = c;
		g = x;
		b = 0;
	}
	else if (h < 120)
	{
		r = x;
		g = c;
		b = 0;
	}
	else if (h < 180)
	{
		r = 0;
		g = c;
		b = x;
	}
	else if (h < 240)
	{
		r = 0;
		g = x;
		b = c;
	}
	else if (h < 300)
	{
		r = x;
		g = 0;
		b = c;
	}
	else
	{
		r = c;
		g = 0;
		b = x;
	}

	*R = clamp((r + m) * 255.0f);
	*G = clamp((g + m) * 255.0f);
	*B = clamp((b + m) * 255.0f);
}

// grayscale
void H_g_img(double complex *n_H, img_t H_img)
{
	for (uint32_t i = 0; i < H_img.height * H_img.width; i++)
	{
		uint32_t c = cabs(n_H[i]) * 255u;

		(H_img.data)[i] = (255 << 24) | (c << 16) | (c << 8) | c; // ARGB format
	}
}

// R and B only
void H_c1_img(double complex *n_H, img_t H_img)
{
	for (uint32_t i = 0; i < H_img.height * H_img.width; i++)
	{
		uint32_t r = (uint32_t)(((creal(n_H[i]) + 1) / 2) * 255u) << 16;
		uint32_t b = (uint32_t)(((cimag(n_H[i]) + 1) / 2) * 255u);

		(H_img.data)[i] = (255 << 24) | r | b; // ARGB format
	}
}

// R and B influenced by abs
void H_c2_img(double complex *n_H, img_t H_img)
{
	for (uint32_t i = 0; i < H_img.height * H_img.width; i++)
	{
		uint32_t r = (uint32_t)(((creal(n_H[i]) + 1) / 2) * cabs(n_H[i]) * 255u) << 16;
		uint32_t b = (uint32_t)(((cimag(n_H[i]) + 1) / 2) * cabs(n_H[i]) * 255u);

		(H_img.data)[i] = (255 << 24) | r | b; // ARGB format
	}
}

// R, G and B
void H_c3_img(double complex *n_H, img_t H_img)
{
	for (uint32_t i = 0; i < H_img.height * H_img.width; i++)
	{
		uint32_t r = (uint32_t)(((creal(n_H[i]) + 1) / 2) * 255u) << 16;
		uint32_t g = (uint32_t)(cabs(n_H[i]) * 255u) << 8; // Use magnitude for green channel
		uint32_t b = (uint32_t)(((cimag(n_H[i]) + 1) / 2) * 255u);

		(H_img.data)[i] = (255 << 24) | r | g | b; // ARGB format
	}
}


// R, G and B influenced by abs
void H_c4_img(double complex *n_H, img_t H_img)
{
	for (uint32_t i = 0; i < H_img.height * H_img.width; i++)
	{
		uint32_t r = (uint32_t)(((creal(n_H[i]) + 1) / 2) * cabs(n_H[i]) * 255u) << 16;
		uint32_t g = (uint32_t)(cabs(n_H[i]) * 255u) << 8; // Use magnitude for green channel
		uint32_t b = (uint32_t)(((cimag(n_H[i]) + 1) / 2) * cabs(n_H[i]) * 255u);

		(H_img.data)[i] = (255 << 24) | r | g | b; // ARGB format
	}
}

// HSV color map
void H_c5_img(double complex *n_H, img_t H_img)
{
	for (uint32_t i = 0; i < H_img.height * H_img.width; i++)
	{
		uint8_t h = (uint8_t)(((creal(n_H[i]) + 1) / 2) * (2.f / 3.f) * 255);
		uint8_t s = (uint8_t)(((cimag(n_H[i]) + 1) / 2) * 255);
		uint8_t v = (uint8_t)(cabs(n_H[i]) * 255);

		uint8_t r, g, b;
		hsv_to_rgb_uint8(h, s, v, &r, &g, &b);
		(H_img.data)[i] = (255 << 24) | (uint32_t)r << 16 | (uint32_t)g << 8 | b; // ARGB format
	}
}