#include "SDL.h"
#include "Simulation.hpp"

#include <iostream>
#include <string>
#include <algorithm>

#define DEBUG

/*
TODO:
rename solution and repo
build boost bcp, look into property config, fix boost config
smooth drawing instead of spotty
reevalute random batching
make all simulation arrays into a single array of objects
rearrange function definitions in simulation
*/

const Uint32 SCREEN_WIDTH = 1000;
const Uint32 SCREEN_HEIGHT = 1000;
const Uint32 TICKS_PER_FRAME = 30;

void SDL_Cleanup(std::string _error, SDL_Window *_win = nullptr, SDL_Renderer *_ren = nullptr, SDL_Texture *_tex = nullptr)
{
	if(_win) { SDL_DestroyWindow(_win); }
	if(_ren) { SDL_DestroyRenderer(_ren); }
	if(!_error.empty()) { std::cout << "SDL error: " << _error << std::endl; }
}

int main(int argc, char **argv)
{
	if(SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		SDL_Cleanup("initalization");
		return EXIT_FAILURE;
	}

	SDL_Window *win = SDL_CreateWindow("Cellular Automata", 750, 100, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if(!win)
	{
		SDL_Cleanup("window creation");
		return EXIT_FAILURE;
	}

	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	if(!ren)
	{
		SDL_Cleanup("renderer creation", win);
		return EXIT_FAILURE;
	}

	SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
	if(!tex)
	{
		SDL_Cleanup("texture creation", win, ren);
		return EXIT_FAILURE;
	}

	Simulation sim(SCREEN_WIDTH, SCREEN_HEIGHT, tex);

	SDL_Event e;
	SDL_Point cursor = {0, 0};
	Uint32 lastTick = 0;
	Uint8 mouseButton = 0;
	Uint16 drawRadius = 15;
	Simulation::Material material = Simulation::Material::SAND;
	bool quit = false;
	while(!quit)
	{
		while(SDL_PollEvent(&e))
		{
			switch(e.type)
			{
			case SDL_QUIT:
				quit = true;
				break;

			case SDL_KEYDOWN:
				switch(e.key.keysym.sym)
				{
				case SDLK_ESCAPE:
					quit = true;
					break;

				default:
					if(e.key.keysym.sym >= 48 && e.key.keysym.sym <= 57)
					{
						material = static_cast<Simulation::Material>(e.key.keysym.sym - 48);
					}
					break;
				}
				break;

			case SDL_MOUSEMOTION:
				cursor.x = e.button.x;
				cursor.y = e.button.y;
				break;

			case SDL_MOUSEBUTTONDOWN:
				mouseButton = e.button.button;
				break;

			case SDL_MOUSEBUTTONUP:
				mouseButton = 0;
				break;

			case SDL_MOUSEWHEEL:
				drawRadius = std::clamp(drawRadius + e.wheel.y, 3, 70);
				break;
			}
		}
		
		if(mouseButton)
		{
			sim.setCellRadius(cursor, drawRadius, material);
		}

		sim.update();
		SDL_UpdateTexture(tex, nullptr, sim.getDrawBuffer(), SCREEN_WIDTH * sizeof(Uint32));

		SDL_RenderClear(ren);
		SDL_RenderCopy(ren, tex, nullptr, nullptr);

		SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
		for(int i = 0; i < drawRadius * 2; ++i)
		{
			int dist = round(sqrt(pow(drawRadius, 2) - pow(abs(drawRadius - i), 2)));
			SDL_RenderDrawPoint(ren, cursor.x - drawRadius + i, cursor.y - dist);
			SDL_RenderDrawPoint(ren, cursor.x - drawRadius + i, cursor.y + dist);
			SDL_RenderDrawPoint(ren, cursor.x - dist, cursor.y - drawRadius + i);
			SDL_RenderDrawPoint(ren, cursor.x + dist, cursor.y - drawRadius + i);
		}
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

		SDL_RenderPresent(ren);

		Uint32 renderTime = SDL_GetTicks() - lastTick;
#ifdef DEBUG
		std::cout << renderTime << "ms" << std::endl;
#endif
		SDL_Delay(TICKS_PER_FRAME - std::min(renderTime, TICKS_PER_FRAME));
		lastTick = SDL_GetTicks();
	}

	SDL_Cleanup(std::string(), win, ren, tex);
	return EXIT_SUCCESS;
}