#include "SDL.h"
#include "SDL_ttf.h"
#include "Simulation.hpp"
#include "Drawing.hpp"

#include <iostream>
#include <string>
#include <algorithm>

//#define DEBUG

/*
TODO:
rename solution and repo
build boost bcp, look into property config, fix boost config
make fire work down
remove iostrema from everywhere
erase
reset
pause
rename window
remove debug
*/

const Uint32 SCREEN_WIDTH = 1500;
const Uint32 SCREEN_HEIGHT = 800;
const Uint32 SIMULATION_WIDTH = 1200;
const Uint32 SIMULATION_HEIGHT = 800;
const Uint32 INFO_UI_HEIGHT = 100;
const Uint32 TICKS_PER_FRAME = 30;

const std::string FONT_PATH = "../../Assets/Fipps-Regular.ttf";
const Uint8 STATIC_TEXTURES = 2;
const Uint8 SIMULATION_TEXTURE = 0;
const Uint8 SELECTION_TEXTURE = 1;

void SDL_Cleanup(std::string _error, SDL_Window *_win = nullptr, SDL_Renderer *_ren = nullptr, 
		SDL_Texture *_tex[STATIC_TEXTURES] = nullptr, TTF_Font *_font = nullptr)
{
	if(!_error.empty())
	{
		std::string error = "SDL error at: " + _error + "\nerror: " +SDL_GetError();
		//std::cout << error << std::endl;
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL ERROR", error.c_str(), _win);
	}

	if(_win) { SDL_DestroyWindow(_win); }
	if(_ren) { SDL_DestroyRenderer(_ren); }
	if(_tex)
	{
		for(int i = 0; i < STATIC_TEXTURES; ++i)
		{
			if(_tex[i]) { SDL_DestroyTexture(_tex[i]); }
		}
	}
	if(_font) { TTF_CloseFont(_font); }
	TTF_Quit();
	SDL_Quit();
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

	SDL_Rect simulationRect = {0, 0, SIMULATION_WIDTH, SIMULATION_HEIGHT};
	SDL_Rect selectionRect = {SIMULATION_WIDTH, INFO_UI_HEIGHT, SCREEN_WIDTH - SIMULATION_WIDTH, SCREEN_HEIGHT - INFO_UI_HEIGHT};
	SDL_Rect infoRect = {SIMULATION_WIDTH, 0, SCREEN_WIDTH - SIMULATION_WIDTH, INFO_UI_HEIGHT};

	SDL_Texture *tex[STATIC_TEXTURES];
	tex[SIMULATION_TEXTURE] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SIMULATION_WIDTH, SIMULATION_HEIGHT);
	tex[SELECTION_TEXTURE] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, selectionRect.w, selectionRect.h);
	for(int i = 0; i < STATIC_TEXTURES; ++i)
	{
		if(!tex[i])
		{
			SDL_Cleanup("texture creation", win, ren, tex);
			return EXIT_FAILURE;
		}
	}

	TTF_Init();
	TTF_Font *font = TTF_OpenFont(FONT_PATH.c_str(), 12);
	if(!font)
	{
		SDL_Cleanup("font creation", win, ren, tex);
		return EXIT_FAILURE;
	}

	Simulation sim(SIMULATION_WIDTH, SIMULATION_HEIGHT, tex[SIMULATION_TEXTURE]);

	SDL_Surface *textSurface = TTF_RenderText_Solid(font, "Wood  Oil  Plasma", {255, 255, 255, 255});

	SDL_Event e;
	SDL_Point cursor = {0, 0};
	SDL_Point lastCursor = cursor;
	Uint32 lastTick = 0;
	Uint8 mouseButton = 0;
	Uint16 drawRadius = 15;
	Simulation::Material material = Simulation::Material::SAND;
	bool paused = false;
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

		if(mouseButton) { sim.setCellLine(cursor, lastCursor, drawRadius, material); }

		if(!paused) { sim.update(); }
		SDL_UpdateTexture(tex[SIMULATION_TEXTURE], nullptr, sim.getDrawBuffer(), SIMULATION_WIDTH * sizeof(Uint32));

		SDL_RenderClear(ren);
		SDL_RenderCopy(ren, tex[SIMULATION_TEXTURE], nullptr, &simulationRect);
		SDL_RenderCopy(ren, tex[SELECTION_TEXTURE], nullptr, &selectionRect);

		Drawing::drawCircle(ren, &cursor, &simulationRect, drawRadius);

		SDL_RenderPresent(ren);

		lastCursor = cursor;

		Uint32 renderTime = SDL_GetTicks() - lastTick;
#ifdef DEBUG
		std::cout << renderTime << "ms" << std::endl;
#endif
		SDL_Delay(TICKS_PER_FRAME - std::min(renderTime, TICKS_PER_FRAME));
		lastTick = SDL_GetTicks();
	}

	SDL_Cleanup(std::string(), win, ren, tex, font);
	return EXIT_SUCCESS;
}