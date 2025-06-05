#ifndef _IMG_UTILS_HH
#define _IMG_UTILS_HH

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

typedef struct {
	uint32_t * data;
	size_t height;
	size_t width;
} img_t;


img_t * create_img(size_t width, size_t height, img_t * img);

#endif