#include "Texture.hpp"

#include <algorithm>

Texture::Texture(SDL_Renderer *_ren, bool &_success, SDL_Rect _rect)
{
	ren = _ren;
	font = nullptr;
	rect = _rect;
	tex = SDL_CreateTexture(ren, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, rect.w, rect.h);
	_success = _success && tex;
	buttons = nullptr;
	buttonCount = 0;
}

Texture::Texture(SDL_Renderer *_ren, Sint32 _x, Sint32 _y, TTF_Font *_font)
{
	ren = _ren;
	font = _font;
	rect.x = _x;
	rect.y = _y;
	tex = nullptr;
	buttons = nullptr;
	buttonCount = 0;
}

Texture::Texture(SDL_Renderer *_ren, bool &_success, SDL_Rect _rect, TTF_Font *_font, std::string _text)
{
	ren = _ren;
	font = _font;
	rect = _rect;
	
	buttonCount = 0;
	for(int i = 0; i < _text.size(); ++i) { if(_text[i] == ' ') { ++buttonCount; } }
	buttons = new SDL_Rect[buttonCount];

	int textHeight;
	TTF_SizeText(font, "A", nullptr, &textHeight);
	Uint32 buttonHSpace = rect.w;
	Uint32 buttonHMargin = buttonHSpace * BUTTON_MARGIN;
	Uint32 buttonVSpace = textHeight * (buttonCount / BUTTONS_IN_ROW);
	Uint32 buttonVMargin = buttonVSpace * BUTTON_MARGIN;
	Uint32 buttonWidth = (buttonHSpace - buttonHMargin * (BUTTONS_IN_ROW - 1)) / BUTTONS_IN_ROW;
	Uint32 buttonHeight = (buttonVSpace - buttonVMargin * ((buttonCount / BUTTONS_IN_ROW) - 1)) / std::max(buttonCount / BUTTONS_IN_ROW, 1);
	for(int i = 0; i < buttonCount; ++i)
	{
		buttons[i].x = rect.x + (buttonWidth + buttonHMargin) * (i % BUTTONS_IN_ROW);
		buttons[i].y = rect.y + (buttonHeight + buttonVMargin) * (i / BUTTONS_IN_ROW);
		buttons[i].w = buttonWidth;
		buttons[i].h = buttonHeight;
	}
	
	SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, rect.w, buttonVSpace, 32, PIXEL_FORMAT);
	for(int i = 0; i < buttonCount; ++i)
	{
		Uint64 delimiterPos = _text.find_first_of(" ");
		std::string text = _text.substr(0, delimiterPos);
		_text = _text.substr(delimiterPos + 1);
		SDL_Surface *tempSurf = TTF_RenderText_Solid(font, text.c_str(), TEXT_COLOR);
		int textWidth;
		TTF_SizeText(font, text.c_str(), &textWidth, nullptr);
		SDL_Rect tempRect = {buttons[i].x - rect.x + buttonWidth / 2 - textWidth / 2, buttons[i].y - rect.y, 0, 0};
		SDL_BlitSurface(tempSurf, nullptr, surf, &tempRect);
		SDL_FreeSurface(tempSurf);
		
	}
	tex = SDL_CreateTextureFromSurface(ren, surf);
	SDL_FreeSurface(surf);
	_success = _success && tex;
	if(!_success) { return; }
	SDL_QueryTexture(tex, nullptr, nullptr, &rect.w, &rect.h);
}

Texture::~Texture()
{
	if(tex) { SDL_DestroyTexture(tex); }
	if(buttons) { delete[] buttons; }
}

bool Texture::isButtonClicked(const SDL_Point *_pos, Uint8 &_button) const
{
	for(int i = 0; i < buttonCount; ++i)
	{
		if(SDL_PointInRect(_pos, &buttons[i]))
		{
			_button = i;
			return true;
		}
	}
	return false;
}

void Texture::renderTexture()
{
	if(tex) { SDL_RenderCopy(ren, tex, nullptr, &rect); }
}

void Texture::changeTexture(Uint32 *_buffer, Uint32 _size)
{
	SDL_UpdateTexture(tex, nullptr, _buffer, _size * sizeof(Uint32));
}

void Texture::changeText(std::string _text)
{
	SDL_Surface *surf = TTF_RenderText_Solid(font, _text.c_str(), TEXT_COLOR);
	SDL_Texture *tempTex = SDL_CreateTextureFromSurface(ren, surf);
	SDL_FreeSurface(surf);
	if(tempTex)
	{
		if(tex) { SDL_DestroyTexture(tex); }
		tex = tempTex;
		SDL_QueryTexture(tex, nullptr, nullptr, &rect.w, &rect.h);
	}
}