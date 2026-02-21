#ifndef _UTILS_HH
#define _UTILS_HH

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <complex.h>

int    gcd(int a, int b);
double random_uniform(double min, double max);

/* screen_w / screen_h as int to support large resolutions on resize */
void screen_choords_to_complex(double x, double y, double complex *c,
                                double x_range[2], double y_range[2],
                                int width, int height);

void complex_to_screen_choords(double complex c, uint32_t *x, uint32_t *y,
                                double x_range[2], double y_range[2],
                                int width, int height);

#endif /* _UTILS_HH */