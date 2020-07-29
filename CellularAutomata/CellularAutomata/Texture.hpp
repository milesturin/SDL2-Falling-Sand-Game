#pragma once

#include "SDL.h"
#include "SDL_ttf.h"

#include <string>

const SDL_PixelFormatEnum PIXEL_FORMAT = SDL_PIXELFORMAT_ARGB8888;

const Uint8 BUTTONS_IN_ROW = 3;
const float BUTTON_MARGIN = 0.05f;

const SDL_Color TEXT_COLOR = {255, 255, 255, 255};

class Texture
{
public:
	//Normal texture
	Texture(SDL_Renderer *_ren, bool &_success, SDL_Rect _rect);
	//Nonstatic text texture
	Texture(SDL_Renderer *_ren, Sint32 _x, Sint32 _y, TTF_Font *_font);
	//Button texture
	Texture(SDL_Renderer *_ren, bool &_success, SDL_Rect _rect, TTF_Font *_font, std::string _text);
	~Texture();

	bool isButtonClicked(const SDL_Point *_pos, Uint8 &_button) const;

	void renderTexture();
	void changeTexture(Uint32 *_buffer, Uint32 _size);
	void changeText(std::string _text);

private:
	SDL_Renderer *ren;
	SDL_Texture *tex;
	TTF_Font *font;
	SDL_Rect rect;
	SDL_Rect *buttons;
	Uint8 buttonCount;
};