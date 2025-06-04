#include "canvas.h"


uint32_t *create_canvas(uint16_t height, uint16_t width, uint8_t color[4])
{
	uint32_t length = height * width;

	uint32_t * canvas = malloc(sizeof(uint32_t) * length);

	for (uint32_t i = 0; i < length; i++)
	{
		canvas[i] = (((uint32_t)color[0])<< (8 * 3)) | (((uint32_t)color[1])<< (8 * 2)) | (((uint32_t)color[2])<< (8 * 1)) | (uint32_t)color[3];
	}

	return canvas;
}