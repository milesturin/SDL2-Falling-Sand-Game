#pragma once

#include "SDL.h"

class Drawing
{
public:
	static void drawCircle(SDL_Renderer *_ren, const SDL_Point *_center, const SDL_Rect *_bounds, Uint16 _rad);
};