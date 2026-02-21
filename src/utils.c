#include "utils.h"

int gcd(int a, int b)
{
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

double random_uniform(double min, double max)
{
    return min + (rand() / (RAND_MAX + 1.0)) * (max - min);
}

void screen_choords_to_complex(double x, double y, double complex *c,
                                double x_range[2], double y_range[2],
                                int width, int height)
{
    double re = x_range[0] + (x / (double)(width  - 1)) * (x_range[1] - x_range[0]);
    double im = y_range[0] + (y / (double)(height - 1)) * (y_range[1] - y_range[0]);
    *c = re + im * I;
}

void complex_to_screen_choords(double complex c, uint32_t *x, uint32_t *y,
                                double x_range[2], double y_range[2],
                                int width, int height)
{
    double re = creal(c);
    double im = cimag(c);
    
    double dx = (re - x_range[0]) / (x_range[1] - x_range[0]);
    double dy = (im - y_range[0]) / (y_range[1] - y_range[0]);
    
    int ix = (int)(dx * (width - 1) + 0.5);
    int iy = (int)(dy * (height - 1) + 0.5);
    
    if (ix < 0) ix = 0;
    if (ix >= width) ix = width - 1;
    if (iy < 0) iy = 0;
    if (iy >= height) iy = height - 1;
    
    *x = ix;
    *y = iy;
}