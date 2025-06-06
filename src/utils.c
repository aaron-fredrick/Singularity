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