#include "SDL.h"
#include "SDL_ttf.h"
#include "Simulation.hpp"
#include "Graphics.hpp"
#include "Texture.hpp"

#include <iostream>
#include <string>
#include <algorithm>

//#define DEBUG

/*
TODO:
rename solution and repo
build boost bcp, look into property config, fix boost config
make fire work down
remove iostream from everywhere
rename window
remove debug
switch from bool flags to temperature
freezing
improve button system and move from spaces to composite textures
window icon
save/load with .bin
liquid pressure
non polar chemical reactions
*/

const Uint32 SCREEN_WIDTH = 1500;
const Uint32 SCREEN_HEIGHT = 800;
const Uint32 SIMULATION_WIDTH = 1150;
const Uint32 SIMULATION_HEIGHT = 800;
const SDL_Rect SIMULATION_RECT = {0, 0, SIMULATION_WIDTH, SIMULATION_HEIGHT};

const Uint32 TICKS_PER_FRAME = 30;
const Uint32 PERFORMANCE_POLL_RATE = 15;

const std::string FONT_FILE_PATH = "../../Fipps-Regular.ttf";

const Uint16 MIN_DRAW_RADIUS = 3;
const Uint16 MAX_DRAW_RADIUS = 75;

const Sint32 UI_HORIZONTAL_MARGIN = 20;
const Sint32 UI_VERTICAL_MARGIN[] = {0, 12, 80, 160};

const SDL_Color CURSOR_COLOR = {255, 255, 255, 255};
const SDL_Color UI_PANEL_COLOR = {80, 80, 80, 255};

enum class TextureID : Uint8 {
	SIMULATION_TEXTURE = 0,
	INFO_UI_TEXTURE,
	TOOLS_UI_TEXTURE,
	MATERIALS_UI_TEXTURE,
	TOTAL_TEXTURES
};

enum class ToolButton : Uint8 {
	PAUSE = 0,
	ERASE,
	RESET,
	TOTAL_BUTTONS
};

//Free all sdl resources
void SDL_Cleanup(std::string _error, SDL_Window *_win = nullptr, SDL_Renderer *_ren = nullptr, 
	TTF_Font *_font = nullptr, Texture *_tex[] = nullptr)
{
	if(!_error.empty())
	{
		std::string error = "SDL error at: " + _error + "\nerror: " + SDL_GetError();
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL ERROR", error.c_str(), _win);
	}

	if(_win) { SDL_DestroyWindow(_win); }
	if(_ren) { SDL_DestroyRenderer(_ren); }
	if(_font) { TTF_CloseFont(_font); }
	if(_tex) { for(int i = 0; i < static_cast<int>(TextureID::TOTAL_TEXTURES); ++i) { if(_tex[i]) { delete _tex[i]; } } }
	
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

	TTF_Init();
	TTF_Font *font = TTF_OpenFont(FONT_FILE_PATH.c_str(), 16);
	if(!font)
	{
		SDL_Cleanup("font creation", win, ren);
		return EXIT_FAILURE;
	}

	Simulation sim(SIMULATION_WIDTH, SIMULATION_HEIGHT, PIXEL_FORMAT);

	Texture *tex[static_cast<int>(TextureID::TOTAL_TEXTURES)];
	bool success = true;
	tex[static_cast<int>(TextureID::SIMULATION_TEXTURE)] = new Texture(ren, success, SIMULATION_RECT);
	tex[static_cast<int>(TextureID::INFO_UI_TEXTURE)] = new Texture(ren, SIMULATION_WIDTH + UI_HORIZONTAL_MARGIN, UI_VERTICAL_MARGIN[static_cast<int>(TextureID::INFO_UI_TEXTURE)], font);
	SDL_Rect rect = {SIMULATION_WIDTH + UI_HORIZONTAL_MARGIN, UI_VERTICAL_MARGIN[static_cast<int>(TextureID::TOOLS_UI_TEXTURE)], SCREEN_WIDTH - SIMULATION_WIDTH - UI_HORIZONTAL_MARGIN * 2, 0};
	tex[static_cast<int>(TextureID::TOOLS_UI_TEXTURE)] = new Texture(ren, success, rect, font, "Pause Erase Reset ");
	rect.y = UI_VERTICAL_MARGIN[static_cast<int>(TextureID::MATERIALS_UI_TEXTURE)];
	tex[static_cast<int>(TextureID::MATERIALS_UI_TEXTURE)] = new Texture(ren, success, rect, font, sim.getMaterialString());
	if(!success)
	{
		SDL_Cleanup("texture creation", win, ren, font, tex);
		return EXIT_FAILURE;
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

		if(SDL_PointInRect(&cursor, &SIMULATION_RECT))
		{
			SDL_ShowCursor(SDL_DISABLE);
			if(lmbHeld) { sim.setCellLine(cursor, lastCursor, drawRadius, material); }
		}
		else
		{
			SDL_ShowCursor(SDL_ENABLE);
			if(lmbPressed)
			{
				Uint8 clicked;
				if(tex[static_cast<int>(TextureID::TOOLS_UI_TEXTURE)]->isButtonClicked(&cursor, clicked))
				{
					switch(static_cast<ToolButton>(clicked))
					{
					case ToolButton::PAUSE:
						paused = !paused;
						break;

					case ToolButton::ERASE:
						material = Simulation::Material::EMPTY;
						break;

					case ToolButton::RESET:
						sim.reset();
						break;
					}
				}
				else if(tex[static_cast<int>(TextureID::MATERIALS_UI_TEXTURE)]->isButtonClicked(&cursor, clicked))
				{
					material = static_cast<Simulation::Material>(clicked + 1);
				}
			}
		}

		if(!paused) { sim.update(); }

		//Draw to the screen
		Graphics::setRenderColor(ren, &UI_PANEL_COLOR);
		SDL_RenderClear(ren);

		if(SDL_GetTicks() % PERFORMANCE_POLL_RATE == 0 || drawRadChanged)
		{
			std::string text = std::to_string(std::min(1000 / std::max<Uint32>(lastRenderTime, 1), 1000 / TICKS_PER_FRAME)) + "fps       pen size: " + std::to_string(drawRadius * 2);
			tex[static_cast<int>(TextureID::INFO_UI_TEXTURE)]->changeText(text);
		}
		tex[static_cast<int>(TextureID::SIMULATION_TEXTURE)]->changeTexture(sim.getDrawBuffer(), SIMULATION_WIDTH);
		for(int i = 0; i < static_cast<int>(TextureID::TOTAL_TEXTURES); ++i) { tex[i]->renderTexture(); }

		Graphics::setRenderColor(ren, &CURSOR_COLOR);
		Graphics::drawCircle(ren, &cursor, &SIMULATION_RECT, drawRadius);

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

		if(strlen(SDL_GetError()) > 0)
		{ 
			SDL_Cleanup("runtime", win, ren, font, tex);
			return EXIT_FAILURE;
		}
	}

	SDL_Cleanup(std::string(), win, ren, font, tex);
	return EXIT_SUCCESS;
}