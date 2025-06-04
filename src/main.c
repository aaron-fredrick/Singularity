#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include "transfer_function.h"

int main(int argc, char **argv)
{
	double complex *s = NULL;
	singularity_array_t zeros, poles;

	create_singularities(&zeros, 5);
	create_singularities(&poles, 5);

	return 0;
}