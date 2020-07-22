#include "SDL.h"
#include "SDL_ttf.h"
#include "Simulation.hpp"
#include "Drawing.hpp"

#include <iostream>
#include <string>
#include <algorithm>

#define DEBUG

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

const Uint32 TICKS_PER_FRAME = 30;
const Uint32 PERFORMANCE_POLL_RATE = 15;

const std::string FONT_PATH = "../../Assets/Fipps-Regular.ttf";
const Uint8 TEXTURE_COUNT = 4;
const Uint8 SIMULATION_TEXTURE = 0;
const Uint8 PERFORMANCE_UI_TEXTURE = 1;
const Uint8 TOOLS_UI_TEXTURE = 2;
const Uint8 ELEMENTS_UI_TEXTURE = 3;

const int UI_HORIZONTAL_MARGIN = 20;
const int UI_VERTICAL_MARGIN[TEXTURE_COUNT] = {0, 12, 50, 130};

const Uint16 MIN_DRAW_RADIUS = 3;
const Uint16 MAX_DRAW_RADIUS = 75;
const SDL_Color TEXT_COLOR = {255, 255, 255, 255};

//Free all sdl resources
void SDL_Cleanup(std::string _error, SDL_Window *_win = nullptr, SDL_Renderer *_ren = nullptr, 
		SDL_Texture *_tex[TEXTURE_COUNT] = nullptr, TTF_Font *_font = nullptr)
{
	if(!_error.empty())
	{
		std::string error = "SDL error at: " + _error + "\nerror: " +SDL_GetError();
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL ERROR", error.c_str(), _win);
	}

	if(_win) { SDL_DestroyWindow(_win); }
	if(_ren) { SDL_DestroyRenderer(_ren); }
	if(_tex)
	{
		for(int i = 0; i < TEXTURE_COUNT; ++i)
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
	//Create all SDL resources, terminating the program if any fail to create
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

	SDL_Texture *tex[TEXTURE_COUNT];
	tex[SIMULATION_TEXTURE] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SIMULATION_WIDTH, SIMULATION_HEIGHT);
	if(!tex[SIMULATION_TEXTURE])
	{
		SDL_Cleanup("texture creation", win, ren, tex);
		return EXIT_FAILURE;
	}

	TTF_Init();
	TTF_Font *font = TTF_OpenFont(FONT_PATH.c_str(), 16);
	if(!font)
	{
		SDL_Cleanup("font creation", win, ren, tex);
		return EXIT_FAILURE;
	}

	Simulation sim(SIMULATION_WIDTH, SIMULATION_HEIGHT, tex[SIMULATION_TEXTURE]);

	//Set up the UI spacing and textures
	SDL_Rect texRect[TEXTURE_COUNT];
	texRect[SIMULATION_TEXTURE] = {0, 0, SIMULATION_WIDTH, SIMULATION_HEIGHT};
	texRect[PERFORMANCE_UI_TEXTURE] = {SIMULATION_WIDTH + UI_HORIZONTAL_MARGIN, UI_VERTICAL_MARGIN[PERFORMANCE_UI_TEXTURE], 0, 0};
	texRect[TOOLS_UI_TEXTURE] = {SIMULATION_WIDTH + UI_HORIZONTAL_MARGIN, UI_VERTICAL_MARGIN[TOOLS_UI_TEXTURE], 0, 0};
	texRect[ELEMENTS_UI_TEXTURE] = {SIMULATION_WIDTH + UI_HORIZONTAL_MARGIN, UI_VERTICAL_MARGIN[ELEMENTS_UI_TEXTURE], 0, 0};

	SDL_Surface *surf = TTF_RenderText_Solid(font, "Pause  Erase  Reset", TEXT_COLOR);
	tex[TOOLS_UI_TEXTURE] = SDL_CreateTextureFromSurface(ren, surf);
	SDL_FreeSurface(surf);
	surf = TTF_RenderText_Solid(font, "Wood  Oil  Plasma", TEXT_COLOR);
	tex[ELEMENTS_UI_TEXTURE] = SDL_CreateTextureFromSurface(ren, surf);
	SDL_FreeSurface(surf);
	if(!tex[TOOLS_UI_TEXTURE] || !tex[ELEMENTS_UI_TEXTURE])
	{
		SDL_Cleanup("texture creation", win, ren, tex, font);
		return EXIT_FAILURE;
	}
	SDL_QueryTexture(tex[TOOLS_UI_TEXTURE], nullptr, nullptr, &texRect[TOOLS_UI_TEXTURE].w, &texRect[TOOLS_UI_TEXTURE].h);
	SDL_QueryTexture(tex[ELEMENTS_UI_TEXTURE], nullptr, nullptr, &texRect[ELEMENTS_UI_TEXTURE].w, &texRect[ELEMENTS_UI_TEXTURE].h);
	
	//Main loop
	SDL_Event e;
	SDL_Point cursor = {0, 0};
	SDL_Point lastCursor = cursor;
	Uint32 lastTick = 0;
	Uint32 lastRenderTime = 0;
	Uint8 mouseButton = 0;
	Uint16 drawRadius = 15;
	bool drawRadChanged;
	Simulation::Material material = Simulation::Material::SAND;
	bool paused = false;
	bool quit = false;
	while(!quit)
	{
		drawRadChanged = false;
		//User input
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
				drawRadius = std::clamp<Sint16>(drawRadius + e.wheel.y, MIN_DRAW_RADIUS, MAX_DRAW_RADIUS);
				drawRadChanged = true;
				break;
			}
		}

		if(SDL_PointInRect(&cursor, &texRect[SIMULATION_TEXTURE]))
		{
			SDL_ShowCursor(SDL_DISABLE);
			if(mouseButton) { sim.setCellLine(cursor, lastCursor, drawRadius, material); }
		}
		else
		{
			SDL_ShowCursor(SDL_ENABLE);
		}

		if(!paused) { sim.update(); }

		//Draw to the screen
		SDL_UpdateTexture(tex[SIMULATION_TEXTURE], nullptr, sim.getDrawBuffer(), SIMULATION_WIDTH * sizeof(Uint32));

		SDL_RenderClear(ren);

		if(SDL_GetTicks() % PERFORMANCE_POLL_RATE == 0 || drawRadChanged)
		{
			std::string text;
			text += std::to_string(std::min(1000 / lastRenderTime, 1000 / TICKS_PER_FRAME));
			text += "fps   pen size: ";
			text += std::to_string(drawRadius * 2).c_str();
			SDL_Surface *surf = TTF_RenderText_Solid(font, text.c_str(), TEXT_COLOR);
			SDL_Texture *tempTex = SDL_CreateTextureFromSurface(ren, surf);
			if(tempTex)
			{
				SDL_DestroyTexture(tex[PERFORMANCE_UI_TEXTURE]);
				tex[PERFORMANCE_UI_TEXTURE] = tempTex;
				SDL_QueryTexture(tex[PERFORMANCE_UI_TEXTURE], nullptr, nullptr, &texRect[PERFORMANCE_UI_TEXTURE].w, &texRect[PERFORMANCE_UI_TEXTURE].h);
			}
			SDL_FreeSurface(surf);
		}

		for(int i = 0; i < TEXTURE_COUNT; ++i) { SDL_RenderCopy(ren, tex[i], nullptr, &texRect[i]);  }

		Drawing::drawCircle(ren, &cursor, &texRect[SIMULATION_TEXTURE], drawRadius);

		SDL_RenderPresent(ren);

		lastCursor = cursor;

		//Calculate performance and limit program speed
		Uint32 renderTime = SDL_GetTicks() - lastTick;
#ifdef DEBUG
		std::cout << renderTime << "ms" << std::endl;
#endif
		SDL_Delay(TICKS_PER_FRAME - std::min(renderTime, TICKS_PER_FRAME));
		lastRenderTime = renderTime;
		lastTick = SDL_GetTicks();
	}

	SDL_Cleanup(std::string(), win, ren, tex, font);
	return EXIT_SUCCESS;
}