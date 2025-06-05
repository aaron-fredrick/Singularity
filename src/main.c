#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include "transfer_function.h"
#include "img_utils.h"
#include "utils.h"

int main(int argc, char **argv)
{
	/* data variables for transfer function */
	double x_range[2], y_range[2], range_factor = 1;
	double complex *s = NULL, *H = NULL;
	singularity_array_t zeros, poles;

	/* variables for image buffers and rendering */
	img_t H_img;
	H_img.data = NULL;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
		return 1;
	}

	SDL_DisplayMode display_mode;
	if (SDL_GetCurrentDisplayMode(0, &display_mode) != 0)
	{
		fprintf(stderr, "SDL_GetCurrentDisplayMode Error: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	int screen_width = display_mode.w;
	int screen_height = display_mode.h;

	printf("Detected fullscreen size: %dx%d\n", screen_width, screen_height);

	int divisor = gcd(screen_width, screen_height);
	int ratio_w = screen_width / divisor;
	int ratio_h = screen_height / divisor;

	x_range[0] = -ratio_w * range_factor;
	x_range[1] = ratio_w * range_factor;
	y_range[0] = -ratio_h * range_factor;
	y_range[1] = ratio_h * range_factor;

	s = generate_s_grid(screen_height, screen_width, y_range, x_range);

	create_img(x_range[1] - x_range[0], y_range[1] - y_range[0], &H_img);

	SDL_Window *window = SDL_CreateWindow("SINGULARITY",
										  SDL_WINDOWPOS_CENTERED,
										  SDL_WINDOWPOS_CENTERED,
										  screen_width, screen_height,
										  SDL_WINDOW_FULLSCREEN); // or SDL_WINDOW_FULLSCREEN_DESKTOP

	if (!window)
	{
		fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
	{
		fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // "0" = nearest, "1" = linear

	SDL_Texture *H_texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		H_img.width,
		H_img.height);

	int running = 1;
	SDL_Event e;

	while (running)
	{
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
			{
				running = 0;
			}
		}

		H = compute_H(s, zeros, poles, y_range[1] - y_range[0], x_range[1] - x_range[0], H);

		printf("H computed, generating image...\n");

		H_g_img(H, H_img);

		SDL_UpdateTexture(H_texture, NULL, H_img.data, H_img.width * sizeof(uint32_t));

		SDL_RenderClear(renderer);					   // Clear the screen
		SDL_RenderCopy(renderer, H_texture, NULL, NULL); // Stretch to fullscreen
		SDL_RenderPresent(renderer);				   // Show it
	}

	return 0;
}