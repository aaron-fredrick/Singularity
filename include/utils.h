#ifndef _UTILS_HH
#define _UTILS_HH

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h> 
#include <complex.h>

int gcd(int a, int b);
double random_uniform(double min, double max);

void screen_choords_to_complex(double x, double y, double complex *c, double x_range[2], double y_range[2], uint16_t width, uint16_t height);

#endif