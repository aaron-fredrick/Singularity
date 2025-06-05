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
		arr->capacity = arr->capacity ? arr->capacity * 2 : 4;
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
	double max_val = 0.0;

	printf("Computing H for %u points..., height %u, width %u\n", length, height, width);

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

		double abs_val = cabs(H[i]);
		if (abs_val > max_val)
			max_val = abs_val;
	}

	// Normalize H
	if (max_val > 0.0)
	{
		for (uint32_t i = 0; i < height * width; i++)
		{
			H[i] /= max_val;
		}
	}

	return H;
}

void H_g_img(double complex *H, img_t H_img)
{
	for (uint32_t i = 0; i < H_img.height * H_img.width; i++)
	{
		uint32_t c = cabs(H[i]) * 255u;

		(H_img.data)[i] = (255<<24) | (c << 16) | (c << 8) | c; // ARGB format
	}
}

uint32_t *H_c1_img(double complex *H, uint16_t height, uint16_t width, uint32_t *img)
{
	if (img != NULL)
	{
		img = malloc(sizeof(uint32_t) * height * width);
		if (!img)
			return NULL;
	}
	for (uint32_t i = 0; i < height * width; i++)
	{
		uint32_t r = (uint32_t)(creal(H[i]) * 255u) << 16;
		uint32_t b = (uint32_t)(cimag(H[i]) * 255u);

		img[i] = (255<<24) | r | 0ul | b; // ARGB format
	}

	return img;
}