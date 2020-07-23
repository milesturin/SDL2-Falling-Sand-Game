#include "SDL.h"
#include "SDL_ttf.h"
#include "Simulation.hpp"
#include "Graphics.hpp"

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
switch from bool flags to temp
freezing
improve button system and move from spaces to composite textures
window icon
*/

const Uint32 SCREEN_WIDTH = 1500;
const Uint32 SCREEN_HEIGHT = 800;
const Uint32 SIMULATION_WIDTH = 1150;
const Uint32 SIMULATION_HEIGHT = 800;

const Uint32 TICKS_PER_FRAME = 30;
const Uint32 PERFORMANCE_POLL_RATE = 15;

const std::string FONT_FILE_PATH = "../../Fipps-Regular.ttf";
const Uint8 TEXTURE_COUNT = 4;
const Uint8 SIMULATION_TEXTURE = 0;
const Uint8 INFO_UI_TEXTURE = 1;
const Uint8 TOOLS_UI_TEXTURE = 2;
const Uint8 MATERIALS_UI_TEXTURE = 3;

const Uint8 BUTTON_COUNT = static_cast<int>(Simulation::Material::TOTAL_MATERIALS) + 2;
const Uint8 BUTTONS_IN_ROW = 3;
const float BUTTON_MARGIN = 0.05;
const Uint8 TOOL_BUTTON_COUNT = 3;
const Uint8 PAUSE_BUTTON = 0;
const Uint8 ERASE_BUTTON = 1;
const Uint8 RESET_BUTTON = 2;

const int UI_HORIZONTAL_MARGIN = 20;
const int UI_VERTICAL_MARGIN[TEXTURE_COUNT] = {0, 12, 80, 150};

const Uint16 MIN_DRAW_RADIUS = 3;
const Uint16 MAX_DRAW_RADIUS = 75;

const SDL_Color TEXT_COLOR = {255, 255, 255, 255};
const SDL_Color CURSOR_COLOR = {255, 255, 255, 255};
const SDL_Color UI_PANEL_COLOR = {80, 80, 80, 255};
const SDL_Color UI_SELECT_COLOR = {255, 0, 0, 255};


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
	TTF_Font *font = TTF_OpenFont(FONT_FILE_PATH.c_str(), 16);
	if(!font)
	{
		SDL_Cleanup("font creation", win, ren, tex);
		return EXIT_FAILURE;
	}

	Simulation sim(SIMULATION_WIDTH, SIMULATION_HEIGHT, tex[SIMULATION_TEXTURE]);

	//Set up the UI spacing, buttons, and textures
	SDL_Rect texRect[TEXTURE_COUNT];
	texRect[SIMULATION_TEXTURE] = {0, 0, SIMULATION_WIDTH, SIMULATION_HEIGHT};
	texRect[INFO_UI_TEXTURE] = {SIMULATION_WIDTH + UI_HORIZONTAL_MARGIN, UI_VERTICAL_MARGIN[INFO_UI_TEXTURE], 0, 0};
	texRect[TOOLS_UI_TEXTURE] = {SIMULATION_WIDTH + UI_HORIZONTAL_MARGIN, UI_VERTICAL_MARGIN[TOOLS_UI_TEXTURE], 0, 0};
	texRect[MATERIALS_UI_TEXTURE] = {SIMULATION_WIDTH + UI_HORIZONTAL_MARGIN, UI_VERTICAL_MARGIN[MATERIALS_UI_TEXTURE], 0, 0};

	SDL_Surface *surf = TTF_RenderText_Solid(font, " Pause    Erase    Reset", TEXT_COLOR);
	tex[TOOLS_UI_TEXTURE] = SDL_CreateTextureFromSurface(ren, surf);
	SDL_FreeSurface(surf);
	surf = TTF_RenderText_Blended_Wrapped(font, sim.getFormattedNames().c_str(), TEXT_COLOR, SCREEN_WIDTH - texRect[MATERIALS_UI_TEXTURE].x - UI_HORIZONTAL_MARGIN);
	tex[MATERIALS_UI_TEXTURE] = SDL_CreateTextureFromSurface(ren, surf);
	SDL_FreeSurface(surf);
	if(!tex[TOOLS_UI_TEXTURE] || !tex[MATERIALS_UI_TEXTURE])
	{
		SDL_Cleanup("texture creation", win, ren, tex, font);
		return EXIT_FAILURE;
	}
	SDL_QueryTexture(tex[TOOLS_UI_TEXTURE], nullptr, nullptr, &texRect[TOOLS_UI_TEXTURE].w, &texRect[TOOLS_UI_TEXTURE].h);
	SDL_QueryTexture(tex[MATERIALS_UI_TEXTURE], nullptr, nullptr, &texRect[MATERIALS_UI_TEXTURE].w, &texRect[MATERIALS_UI_TEXTURE].h);
	tex[INFO_UI_TEXTURE] = nullptr;

	SDL_Rect buttons[BUTTON_COUNT];
	Uint32 buttonHSpace = SCREEN_WIDTH - SIMULATION_WIDTH - UI_HORIZONTAL_MARGIN * 2;
	Uint32 buttonHMargin = buttonHSpace * BUTTON_MARGIN;
	Uint32 buttonVSpace = texRect[MATERIALS_UI_TEXTURE].h;
	Uint32 buttonVMargin = buttonVSpace * BUTTON_MARGIN;
	Uint32 buttonWidth = (buttonHSpace - buttonHMargin * (BUTTONS_IN_ROW - 1)) / BUTTONS_IN_ROW;
	Uint32 buttonHeight = (buttonVSpace - buttonVMargin * (BUTTONS_IN_ROW - 1)) / ((BUTTON_COUNT - BUTTONS_IN_ROW) / BUTTONS_IN_ROW);
	for(int i = 0; i < BUTTON_COUNT; ++i)
	{
		buttons[i].x = SIMULATION_WIDTH + UI_HORIZONTAL_MARGIN + (buttonWidth + buttonHMargin) * (i % BUTTONS_IN_ROW);
		buttons[i].y = i < BUTTONS_IN_ROW ? texRect[TOOLS_UI_TEXTURE].y : texRect[MATERIALS_UI_TEXTURE].y + (buttonHeight + buttonVMargin) * ((i - TOOL_BUTTON_COUNT) / BUTTONS_IN_ROW);
		buttons[i].w = buttonWidth;
		buttons[i].h = buttonHeight;
	}
	
	//Main loop
	SDL_Event e;
	SDL_Point cursor = {0, 0};
	SDL_Point lastCursor = cursor;
	Uint32 lastTick = 0;
	Uint32 lastRenderTime = 0;
	Simulation::Material material = Simulation::Material::SAND;
	Uint16 drawRadius = 15;
	bool drawRadChanged;
	bool lmbPressed;
	bool lmbHeld = false;
	bool paused = false;
	bool quit = false;
	while(!quit)
	{
		lmbPressed = false;
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
				
				case SDLK_SPACE:
					paused = !paused;
					break;
				}
				break;

			case SDL_MOUSEMOTION:
				cursor.x = e.button.x;
				cursor.y = e.button.y;
				break;

			case SDL_MOUSEBUTTONDOWN:
				lmbPressed = true;
				lmbHeld = true;
				break;

			case SDL_MOUSEBUTTONUP:
				lmbHeld = false;
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
			if(lmbHeld) { sim.setCellLine(cursor, lastCursor, drawRadius, material); }
		}
		else
		{
			SDL_ShowCursor(SDL_ENABLE);
			if(lmbPressed)
			{
				for(int i = 0; i < BUTTON_COUNT; ++i)
				{
					if(SDL_PointInRect(&cursor, &buttons[i]))
					{
						if(i < TOOL_BUTTON_COUNT)
						{
							switch(i)
							{
							case PAUSE_BUTTON:
								paused = !paused;
								break;

							case ERASE_BUTTON:
								material = Simulation::Material::EMPTY;
								break;

							case RESET_BUTTON:
								sim.reset();
								break;
							}
						}
						else
						{
							material = static_cast<Simulation::Material>(i - TOOL_BUTTON_COUNT + 1);
						}
						break;
					}
				}
			}
		}

		if(!paused) { sim.update(); }

		//Draw to the screen
		SDL_UpdateTexture(tex[SIMULATION_TEXTURE], nullptr, sim.getDrawBuffer(), SIMULATION_WIDTH * sizeof(Uint32));
		
		Graphics::setRenderColor(ren, &UI_PANEL_COLOR);
		SDL_RenderClear(ren);

		if(SDL_GetTicks() % PERFORMANCE_POLL_RATE == 0 || drawRadChanged)
		{
			std::string text;
			text += std::to_string(std::min(1000 / std::max<Uint32>(lastRenderTime, 1), 1000 / TICKS_PER_FRAME));
			text += "fps       pen size: ";
			text += std::to_string(drawRadius * 2).c_str();
			SDL_Surface *surf = TTF_RenderText_Solid(font, text.c_str(), TEXT_COLOR);
			SDL_Texture *tempTex = SDL_CreateTextureFromSurface(ren, surf);
			if(tempTex)
			{
				if(tex[INFO_UI_TEXTURE]) { SDL_DestroyTexture(tex[INFO_UI_TEXTURE]); }
				tex[INFO_UI_TEXTURE] = tempTex;
				SDL_QueryTexture(tex[INFO_UI_TEXTURE], nullptr, nullptr, &texRect[INFO_UI_TEXTURE].w, &texRect[INFO_UI_TEXTURE].h);
			}
			SDL_FreeSurface(surf);
		}
		for(int i = 0; i < TEXTURE_COUNT; ++i) { if(tex[i]) { SDL_RenderCopy(ren, tex[i], nullptr, &texRect[i]); } }

		Graphics::setRenderColor(ren, &UI_SELECT_COLOR);
		
		SDL_RenderDrawRects(ren, buttons, BUTTON_COUNT);
		Graphics::drawCircle(ren, &cursor, &texRect[SIMULATION_TEXTURE], drawRadius, &CURSOR_COLOR);

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

		if(SDL_GetError() != "") { std::cout << SDL_GetError() << std::endl; } //DEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUGDEBUG
	}

	SDL_Cleanup(std::string(), win, ren, tex, font);
	return EXIT_SUCCESS;
}