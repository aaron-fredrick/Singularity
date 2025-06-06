#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <time.h>
#include "transfer_function.h"
#include "img_utils.h"
#include "utils.h"

#define N_ZEROS 3
#define N_POLES 3
#define TARGET_FPS 30
#define FRAME_DURATION_MS (1000 / TARGET_FPS)

int main(int argc, char **argv)
{
	/* data variables for transfer function */
	double x_range[2], y_range[2], range_factor = 1;
	double complex *s = NULL, *H = NULL;
	double complex *n_H = NULL;
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

	uint16_t grid_width = screen_width / 4;	  // 1/4 of the screen width
	uint16_t grid_height = screen_height / 4; // 1/4 of the screen height

	s = generate_s_grid(grid_height, grid_width, y_range, x_range);

	create_img(grid_width, grid_height, &H_img);

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

	srand((unsigned int)time(NULL));

	create_singularities(&zeros, N_ZEROS);
	create_singularities(&poles, N_POLES);

	printf("[");
	for (size_t i = 0; i < N_ZEROS; i++)
	{
		singularity_t z = {0};
		z.val = random_uniform(x_range[0], x_range[1]) + random_uniform(y_range[0], y_range[1]) * I; // Example zeros at imaginary axis
		z.e = (uint8_t)random_uniform(1, 5);														 // Order of the zero
		z.m = random_uniform(0, 5);																	 // Magnitude
		z.c = random_uniform(0, 5);																	 // Constant term
		add_singularity(&zeros, z);

		printf("complex(%f,%f),", creal(z.val), cimag(z.val));
	}
	printf("]\n");

	printf("[");
	for (size_t i = 0; i < N_POLES; i++)
	{
		singularity_t p = {0};
		p.val = random_uniform(x_range[0], x_range[1]) + random_uniform(y_range[0], y_range[1]) * I; // Example poles at imaginary axis
		p.e = (uint8_t)random_uniform(1, 5);														 // Order of the pole
		p.m = random_uniform(0, 5);																	 // Magnitude
		p.c = random_uniform(0, 5);																	 // Constant term
		add_singularity(&poles, p);

		printf("complex(%f,%f),", creal(p.val), cimag(p.val));
	}
	printf("]\n");

	printf("Generated %zu zeros and %zu poles.\n", zeros.size, poles.size);

	int running = 1;
	int c_map = 0, n_map = 0;
	SDL_Event e;
	while (running)
	{
		Uint32 frame_start = SDL_GetTicks();

		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
			{
				running = 0;
			}
			else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_TAB)
			{
				c_map = (c_map + 1) % 6; // Cycle through color maps
				printf("Color map changed to %d\n", c_map);
			}
			else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE)
			{
				n_map = (n_map + 1) % 2; // Cycle through color maps
				printf("Normalisation map changed to %d\n", n_map);
			}
		}

		H = compute_H(s, zeros, poles, grid_height, grid_width, H);

		switch (n_map)
		{
		case 0:
			n_H = normalize_H_log_complex(H, grid_height * grid_width, n_H);
			break;

		case 1:
			n_H = normalize_H_complex(H, grid_height * grid_width, n_H);
			break;

		default:
			n_H = normalize_H_log_complex(H, grid_height * grid_width, n_H);
			break;
		}

		switch (c_map)
		{
		case 0:
			H_g_img(n_H, H_img); // Grayscale
			break;
		case 1:
			H_c1_img(n_H, H_img); // C1 color map
			break;

		case 2:
			H_c2_img(n_H, H_img); // C2 color map
			break;
		case 3:
			H_c3_img(n_H, H_img); // C3 color map
			break;

		case 4:
			H_c4_img(n_H, H_img); // C4 color map
			break;

		case 5:
			H_c5_img(n_H, H_img); // C5 color map
			break;

		default:
			break;
		}

		SDL_UpdateTexture(H_texture, NULL, H_img.data, H_img.width * sizeof(uint32_t));

		SDL_RenderClear(renderer);						 // Clear the screen
		SDL_RenderCopy(renderer, H_texture, NULL, NULL); // Stretch to fullscreen
		SDL_RenderPresent(renderer);					 // Show it

		Uint32 frame_time = SDL_GetTicks() - frame_start;

		if (frame_time < FRAME_DURATION_MS)
		{
			SDL_Delay(FRAME_DURATION_MS - frame_time);
		}
	}

	free_singularities(&zeros);
	free_singularities(&poles);

	free(s);
	free(H);
	free(n_H);

	free(H_img.data);

	SDL_DestroyTexture(H_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	printf("Exiting program...\n");

	return 0;
}