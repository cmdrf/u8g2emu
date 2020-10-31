/* u8g2emu.c

MIT License

Copyright (c) 2020 Fabian Herb

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "u8g2emu.h"

#include <SDL.h>

typedef struct
{
	int current_page;
	int data_enabled;
	SDL_Window* window;
	SDL_Surface* page_surface;
	SDL_Palette* palette;
} u8g2emu_t;

static u8g2emu_t u8g2emu = {0};

int u8g2emu_Setup(u8g2emu_t* emu)
{
	emu->current_page = 0;
	emu->data_enabled = 0;

	SDL_Init(SDL_INIT_VIDEO);

	emu->window = SDL_CreateWindow(
			"u8g2emu",                  // window title
			SDL_WINDOWPOS_UNDEFINED,           // initial x position
			SDL_WINDOWPOS_UNDEFINED,           // initial y position
			256,                               // width, in pixels
			128,                               // height, in pixels
			0                  // flags - see below
	);

	if (emu->window == NULL)
	{
		// In the case that the window could not be made...
		fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
		return 1;
	}

	emu->palette = SDL_AllocPalette(2);
	SDL_Color colors[2];
	colors[0].r = 0;
	colors[0].g = 0;
	colors[0].b = 0;
	colors[0].a = 255;
	colors[1].r = 255;
	colors[1].g = 255;
	colors[1].b = 255;
	colors[1].a = 255;
	SDL_SetPaletteColors(emu->palette, colors, 0, 2);

	emu->page_surface = SDL_CreateRGBSurfaceWithFormat(0, 128, 8, 1, SDL_PIXELFORMAT_INDEX8);
	if(!emu->page_surface)
	{
		fprintf(stderr, "Could not create surface: %s\n", SDL_GetError());
		return 1;
	}
	if(SDL_SetSurfacePalette(emu->page_surface, emu->palette))
	{
		fprintf(stderr, "Could not set surface palette: %s\n", SDL_GetError());
		return 1;
	}

	return 0;
}

void u8g2emu_Teardown(u8g2emu_t* emu)
{
	SDL_FreeSurface(emu->page_surface);
	SDL_FreePalette(emu->palette);

	// Close and destroy the window
	SDL_DestroyWindow(emu->window);

	// Clean up
	SDL_Quit();
}


uint8_t u8g2emu_msg_callback(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr)
{
	if(!u8g2emu.window)
		u8g2emu_Setup(&u8g2emu);

	const uint8_t* bytes = (const uint8_t*)arg_ptr;
	switch (msg)
	{
	case U8X8_MSG_BYTE_SEND:
		if(u8g2emu.data_enabled)
		{
			SDL_LockSurface(u8g2emu.page_surface);
			uint8_t* pixels = (uint8_t*)u8g2emu.page_surface->pixels;
			for(int i = 0; i < 128; ++i)
				for(int bit = 0; bit < 8; ++bit)
					pixels[bit * 128 + i] = (bytes[i] & (1 << bit)) ? 1 : 0;
			SDL_UnlockSurface(u8g2emu.page_surface);

			SDL_Surface* cs = SDL_ConvertSurface(u8g2emu.page_surface, SDL_GetWindowSurface(u8g2emu.window)->format, 0);
			if(!cs)
				fprintf(stderr, "SDL_ConvertSurface: %s\n", SDL_GetError());

			SDL_Rect dstrect;
			dstrect.x = 0;
			dstrect.y = u8g2emu.current_page * 8 * 2;
			dstrect.w = 256;
			dstrect.h = 16;

			if(SDL_BlitScaled(cs,
							   NULL,
							   SDL_GetWindowSurface(u8g2emu.window),
							   &dstrect))
				fprintf(stderr, "Error: %s\n", SDL_GetError());
			SDL_FreeSurface(cs);

			if(u8g2emu.current_page == 7)
				SDL_UpdateWindowSurface(u8g2emu.window);
		}
		else // Commands
		{
			for(unsigned int i = 0; i < arg_int; ++i)
			{
				if(bytes[i] >= 0xb0 && bytes[i] < 0xb8) // Set page address
				{
					u8g2emu.current_page = bytes[i] - 0xb0;
				}
			}
		}
		break;

	case U8X8_MSG_BYTE_SET_DC:
		u8g2emu.data_enabled = arg_int;
		break;

	case U8X8_MSG_BYTE_START_TRANSFER:

		break;
	case U8X8_MSG_BYTE_END_TRANSFER:
		break;
	}
	return 1;
}

uint8_t u8g2emu_gpio_and_delay(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
	return 1;
}

uint8_t u8x8_GetMenuEvent(u8x8_t* u8x8)
{
	SDL_Event event;
	SDL_WaitEvent(&event);
	if(event.type == SDL_KEYDOWN)
	{
		switch(event.key.keysym.sym)
		{
		case SDLK_UP:
			return U8X8_MSG_GPIO_MENU_UP;
		case SDLK_DOWN:
			return U8X8_MSG_GPIO_MENU_DOWN;
		case SDLK_LEFT:
			return U8X8_MSG_GPIO_MENU_PREV;
		case SDLK_RIGHT:
			return U8X8_MSG_GPIO_MENU_NEXT;
		case SDLK_RETURN:
			return U8X8_MSG_GPIO_MENU_SELECT;
		case SDLK_ESCAPE:
			return U8X8_MSG_GPIO_MENU_HOME;
		}
	}

	return 0;
}

void u8g2emu_PumpEvents()
{
	SDL_PumpEvents();
}
