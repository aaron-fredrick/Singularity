#include "utils.h"

int gcd(int a, int b)
{
	while (b != 0)
	{
		int temp = b;
		b = a % b;
		a = temp;
	}
	return a;
}

double random_uniform(double min, double max)
{
	return min + (rand() / (RAND_MAX + 1.0)) * (max - min);
}

void screen_choords_to_complex(double x, double y, double complex *c, double x_range[2], double y_range[2], uint16_t width, uint16_t height)
{
	double x_start = x_range[0];
	double x_end = x_range[1];
	double y_start = y_range[0];
	double y_end = y_range[1];

	double re = x_start + (x / (double)(width - 1)) * (x_end - x_start);
	double im = y_start + (y / (double)(height - 1)) * (y_end - y_start);

	*c = re + im * I;
}