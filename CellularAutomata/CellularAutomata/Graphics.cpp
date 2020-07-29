#include "Graphics.hpp"

#include <algorithm>

void Graphics::setRenderColor(SDL_Renderer *_ren, const SDL_Color *_col)
{
	SDL_SetRenderDrawColor(_ren, _col->r, _col->g, _col->b, _col->a);
}

//Uses a modified version of the Midpoint Circle Algorithm
void Graphics::drawCircle(SDL_Renderer *_ren, const SDL_Point *_center, const SDL_Rect *_bounds, Uint16 _rad)
{
	if(!SDL_PointInRect(_center, _bounds)) { return; }

	Sint32 x = _rad;
	Sint32 y = 0;
	Sint32 tx = 1;
	Sint32 ty = 1;
	Sint32 error = tx - _rad * 2;

	while(x >= y)
	{
		Sint32 lookup[16] = {x, -y, x, y, -x, -y, -x, y, y, -x, y, x, -y, -x, -y, x};
		for(int i = 0; i < 8; ++i)
		{
			Sint32 drawX = std::clamp(_center->x + lookup[i * 2], _bounds->x, _bounds->x + _bounds->w);
			Sint32 drawY = std::clamp(_center->y + lookup[i * 2 + 1], _bounds->y, _bounds->y + _bounds->h - 1);
			SDL_RenderDrawPoint(_ren, drawX, drawY);
		}

		if(error <= 0)
		{
			++y;
			ty += 2;
			error += ty;
		}
		if(error > 0)
		{
			--x;
			tx += 2;
			error += tx - _rad * 2;
		}
	}
}

//A modified version of bresenham's line algorithm
Uint16 Graphics::bresenhams(Sint32 _x1, Sint32 _y1, Sint32 _x2, Sint32 _y2, Sint32 *&_xOut, Sint32 *&_yOut, bool _fill)
{
	Sint32 dx = _x2 - _x1;
	Sint32 dy = _y2 - _y1;
	bool hor = abs(dx) >= abs(dy);
	if(hor && dx < 0 || !hor && dy < 0)
	{
		Sint32 temp = _x1;
		_x1 = _x2;
		_x2 = temp;
		temp = _y1;
		_y1 = _y2;
		_y2 = temp;
		dx = -dx;
		dy = -dy;
	}

	bool di = true;
	Sint32 d1, d2, i0, i1, j0, j1;
	if(hor)
	{
		d1 = dx;
		d2 = dy;
		i0 = _x1;
		i1 = _x2;
		j0 = _y1;
		j1 = _y2;
	}
	else
	{
		d1 = dy;
		d2 = dx;
		i0 = _y1;
		i1 = _y2;
		j0 = _x1;
		j1 = _x2;
	}
	if(d2 < 0)
	{
		di = false;
		d2 = -d2;
	}
	Sint32 D = 2 * d2 - d1;
	Sint32 j = j0;
	Uint16 len = i1 - i0;
	if(_fill) { len += abs(j1 - j0) - 1; }
	_xOut = new Sint32[len];
	_yOut = new Sint32[len];
	Uint16 index = 0;
	for(Sint32 i = i0; i < i1; ++i)
	{
		_xOut[index] = hor ? i : j;
		_yOut[index] = hor ? j : i;

		if(D > 0)
		{
			if(_fill && i != i1 - 1)
			{
				++index;
				_xOut[index] = hor ? i + 1 : j;
				_yOut[index] = hor ? j : i + 1;
			}
			j += di ? 1 : -1;
			D -= 2 * d1;
		}
		D += 2 * d2;
		++index;
	}
	return len;
}
