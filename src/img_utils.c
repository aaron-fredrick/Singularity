#include "img_utils.h"

img_t * create_img(size_t width, size_t height, img_t * img) {
	uint8_t malloced = 0;
	if (img == NULL) {
		img_t * img = malloc(sizeof(img_t));
		malloced = 1;
		if (!img) {
			return NULL; // Memory allocation failed
		}
	}
	
	img->width = width;
	img->height = height;
	img->data = malloc(width * height * sizeof(uint32_t));
	
	if (!img->data) {
		if (malloced) {
			free(img); // Free the img structure if data allocation fails
		}
		return NULL; // Memory allocation failed
	}
	
	return img;
}