#include "Simulation.hpp"
//#include "iostream"//sdfgsioudfnghoisdfnhoisuhn
Simulation::Simulation(int _width, int _height, SDL_Texture *_tex)
{
	width = _width;
	height = _height;
	size = static_cast<Uint64>(_width) * static_cast<Uint64>(_height);

	Uint32 intFormat;
	SDL_QueryTexture(_tex, &intFormat, nullptr, nullptr, nullptr);
	pixelFormat = SDL_AllocFormat(intFormat);

	mt = std::mt19937(std::time(0));
	std::uniform_real_distribution<double> doubleDist(0.0, 1.0);
	std::uniform_int_distribution<int> xorSeedDist(100000000);

	computeBuffer = new Material[size];
	fillComputeBuffer();
	drawBuffer = new Uint32[size];
	fillDrawBuffer();
	batchNoise = new Uint16[size];
	iterationNoise = new Uint32[size];
	for(int i = 0; i < size; ++i)
	{
		batchNoise[i] = i % (size / RAND_BATCH_SIZE) == 0 ? i / (size / RAND_BATCH_SIZE) : batchNoise[i - 1];
		iterationNoise[i] = i;
	}
	std::shuffle(batchNoise, batchNoise + size, mt);
	std::shuffle(iterationNoise, iterationNoise + size, mt);

	updatedCells = new boost::dynamic_bitset<Uint64>(size);

	//Behavior sets can be biased by using duplicate behaviors. However, 
	//if less biased behaviors are not equally distributed from a left to right perspective, unwanted bias can occur.
	allSpecs[static_cast<int>(Material::ROCK)] = {
		{0, 0, 32}, {0, 0, 77}, 
		0, 0, 255, 0, 
		true, false, false, false, false,
		0, {},
		{}
	};
	allSpecs[static_cast<int>(Material::SAND)] = {
		{30, 214, 156}, {38, 240, 243}, 
		1, 2, 120, 0, 
		true, false, false, false, true,
		1, {1},
		{
			{Direction::SOUTH}
		}
	};
	allSpecs[static_cast<int>(Material::WATER)] = {
		{140, 186, 255}, {164, 255, 255}, 
		2, 3, 50, 0, 
		false, false, false, false, false,
		2, {6, 2},
		{
			{Direction::SOUTH, Direction::SOUTH, Direction::SOUTH_EAST, Direction::SOUTH, Direction::SOUTH, Direction::SOUTH_WEST},
			{Direction::EAST, Direction::WEST}
		}
	};
	allSpecs[static_cast<int>(Material::FIRE)] = {
		{0, 219, 255}, {19, 255, 255}, 
		3, 4, 0, 10, 
		false, true, false, false, false,
		2, {4, 2},
		{
			{Direction::NORTH, Direction::NORTH, Direction::NORTH_EAST, Direction::NORTH_WEST},
			{Direction::EAST, Direction::WEST}
		}
	};
	allSpecs[static_cast<int>(Material::LAVA)] = {
		{13, 255, 180}, {30, 255, 255},
		1, 3, 100, 0,
		false, true, false, true, false,
		2, {1, 2},
		{
			{Direction::SOUTH},
			{Direction::EAST, Direction::WEST}
		}
	};
	allSpecs[static_cast<int>(Material::GAS)] = {
		{194, 60, 224}, {209, 120, 255}, 
		5, 7, 10, 0, 
		false, false, true, false, false,
		2, {3, 2},
		{
			{Direction::NORTH, Direction::NORTH_EAST, Direction::NORTH_WEST},
			{Direction::EAST, Direction::WEST}
		}
	};
	allSpecs[static_cast<int>(Material::STEAM)] = {
		{0, 0, 200}, {0, 0, 255},
		4, 6, 5, 50,
		false, false, false, false, false,
		2, {3, 2},
		{
			{Direction::NORTH, Direction::NORTH_EAST, Direction::NORTH_WEST},
			{Direction::EAST, Direction::WEST}
		}
	};
	allSpecs[static_cast<int>(Material::GRAVEL)] = {
		{37, 0, 40}, {37, 66, 190},
		2, 4, 150, 0,
		true, false, false, false, false,
		2, {1, 2},
		{
			{Direction::SOUTH},
			{Direction::SOUTH_EAST, Direction::SOUTH_WEST}
		}
	};
	allSpecs[static_cast<int>(Material::PLASMA)] = {
		{180, 140, 255}, {213, 255, 255},
		2, 5, 0, 7,
		false, true, false, true, false,
		1, {8},
		{
			{Direction::NORTH_WEST, Direction::NORTH, Direction::NORTH_EAST, Direction::EAST, Direction::SOUTH_EAST, Direction::SOUTH, Direction::SOUTH_WEST, Direction::WEST}
		}
	};
}

Simulation::~Simulation()
{
	delete[] computeBuffer;
	delete[] drawBuffer;
	delete[] batchNoise;
	delete[] iterationNoise;
	delete updatedCells;
}

void Simulation::update()
{
	//Because RNG is the main computational bottleneck, we create only a fraction of the needed numbers,
	//then pick them using a pre-generated noise array. Each random number is used only with the modulo operator,
	//so we can reuse each number several times by dividing by ten (*= 0.1 is faster) after each use.
	Uint32 *randBatch = new Uint32[RAND_BATCH_SIZE];
	for(int i = 0; i < RAND_BATCH_SIZE; ++i) { randBatch[i] = xorshift128(); }

	updatedCells->reset();
	for(int i = 0; i < size; ++i)
	{
		//In order to not prefer a certain direction of movement, we have to iterate through the array in a random way
		Uint32 index = iterationNoise[i];
		if(computeBuffer[index] == Material::EMPTY || updatedCells->test(index)) { continue; }

		const MaterialSpecs *matSpecs = &allSpecs[static_cast<int>(computeBuffer[index])];
		if(matSpecs->behaviorSetCount == 0) { continue; }

		Uint32 randi = randBatch[batchNoise[index]];
		auto preGenRandRange = [&](Uint8 _min, Uint8 _max)
		{
			if(_min == _max) { return _min; }
			Uint8 result = _min + randi % (_max + 1 - _min);
			randi *= 0.1;
			return result;
		};

		bool moved = false;
		Uint8 speed = preGenRandRange(matSpecs->minSpeed, matSpecs->maxSpeed);

		//Attempts to move the cell by all defined behavior sets in order of preference
		for(int j = 0; j < matSpecs->behaviorSetCount; ++j)
		{
			if(moved) { break; }

			if(matSpecs->deathChance > 0)
			{
				if(preGenRandRange(1, matSpecs->deathChance) == 1)
				{
					setCell(index, Material::EMPTY);
					break;
				}
			}

			//Tries each direction in each behavior set, starting from a random one.
			Uint8 directionIndex = preGenRandRange(0, matSpecs->behaviorCounts[j] - 1);
			for(int k = 0; k < matSpecs->behaviorCounts[j]; ++k)
			{
				Direction direction = matSpecs->behavior[j][directionIndex];
				Uint32 lastIndex = index;
				bool destroyed = false;
				for(int l = 0; l < speed; ++l)
				{
					Uint32 newIndex = getRelative(lastIndex, direction);
					if(newIndex == -1) { break; }
					if(computeBuffer[newIndex] != Material::EMPTY)
					{
						//Chemical and physical reactions. General properties use boolean flags, while specific interactions are hard coded
						const MaterialSpecs *collisionSpecs = &allSpecs[static_cast<int>(computeBuffer[newIndex])];
						if(matSpecs->flaming && collisionSpecs->flammable)
						{
							setCell(newIndex, Material::FIRE);
							break;
						}
						if(matSpecs->melting && collisionSpecs->meltable)
						{
							setCell(newIndex, Material::LAVA);
							break;
						}
						if(computeBuffer[index] == Material::WATER)
						{
							if(computeBuffer[newIndex] == Material::FIRE)
							{
								setCell(newIndex, Material::EMPTY);
								setCell(index, Material::STEAM);
								destroyed = true;
								break;
							}
							if(computeBuffer[newIndex] == Material::LAVA)
							{
								setCell(newIndex, Material::GRAVEL);
								setCell(index, Material::STEAM);
								destroyed = true;
								break;
							}
						}
						if(!collisionSpecs->solid && 
							(collisionSpecs->density < matSpecs->density ||
							collisionSpecs->density > matSpecs->density && direction < Direction::EAST))
						{
							lastIndex = newIndex;
							break;
						}
						break;
					}
					lastIndex = newIndex;
				}
				if(lastIndex != index && !destroyed)
				{
					swapCell(index, lastIndex);
					moved = true;
					break;
				}
				if(++directionIndex >= matSpecs->behaviorCounts[j]) { directionIndex = 0; }
			}
		}
		//Creates a nice visual effect by mixing non-solids if they cannot move normally
		if(!moved && !matSpecs->solid)
		{
			Uint8 direction = preGenRandRange(0, static_cast<int>(Direction::TOTAL_DIRECTIONS) - 1);
			for(int j = 0; j < static_cast<int>(Direction::TOTAL_DIRECTIONS); ++j)
			{
				Uint32 location = getRelative(index, static_cast<Direction>(direction));
				if(location == -1) { continue; }
				Material buffMat = computeBuffer[location];
				if(!allSpecs[static_cast<int>(buffMat)].solid && allSpecs[static_cast<int>(buffMat)].density == matSpecs->density)
				{
					swapCell(index, location);
					break;
				}
				if(direction >= static_cast<int>(Direction::TOTAL_DIRECTIONS)) { direction = 0; }
			}
		}
	}
	delete[] randBatch;
}

//Sets a cell to a material. Interpolates between colors to add visual variation. Used only when a cell is initially placed.
void Simulation::setCell(Uint32 _index, Material _mat)
{
	computeBuffer[_index] = _mat;
	
	if(_mat != Material::EMPTY)
	{
		auto randLerp = [&](Uint8 min, Uint8 max) { return static_cast<Uint8>(min + round((max - min) * doubleDist(mt))); };
		const MaterialSpecs *specs = &allSpecs[static_cast<int>(_mat)];
		HsvColor interpolatedHsv = {
			randLerp(specs->minColor.h, specs->maxColor.h),
			randLerp(specs->minColor.s, specs->maxColor.s),
			randLerp(specs->minColor.v, specs->maxColor.v)
		};
		SDL_Color rgba = HsvToRgb(&interpolatedHsv);
		drawBuffer[_index] = SDL_MapRGBA(pixelFormat, rgba.r, rgba.g, rgba.b, rgba.a);
	}
	else
	{
		drawBuffer[_index] = SDL_MapRGBA(pixelFormat, 0, 0, 0, 255);
	}

	updatedCells->set(_index);
}

//Sets a circle of cells by radius for user input
void Simulation::setCellRadius(SDL_Point _pos, Uint16 _rad, Material _mat)
{
	for(int i = 0; i < _rad * 2; ++i)
	{
		Uint16 slice = round(sqrt(pow(_rad, 2) - pow(abs(_rad - i), 2)));
		for(int j = -slice; j < slice; ++j)
		{
			Uint32 x = _pos.x + j;
			Uint32 y = _pos.y - _rad + i;
			if(y >= 0 && y < height && x >= 0 && x < width)
			{
				Uint32 index = x + y * width;
				if(computeBuffer[index] == Material::EMPTY)
				{
					setCell(index, _mat);
				}
			}
		}
	}
}

//Draws a thick line between two points. This is used so that when the cursor is moved quickly it makes a contiguous line
void Simulation::setCellLine(SDL_Point _start, SDL_Point _end, Uint16 _rad, Material _mat)
{
	/*setCellRadius(_start, _rad, _mat);
	setCellRadius(_end, _rad, _mat);
	float slope = (_end.y - _start.y) / (_end.x - _start.x);
	if(abs(slope) < 1.0f)
	{
		for(int i = 0; i < _end.x - _start.x; ++i)
		{
			for(int j = 0; j < _rad * 2; ++j)
			{

			}
		}
	}
	else
	{

	}*/
}

//Sees if a cell relative to a given index is a valid spot to move
Uint32 Simulation::getRelative(Uint32 _index, Direction _dir) const
{
	Sint8 horizontal = 0;
	horizontal += _dir == Direction::NORTH_EAST || _dir == Direction::EAST || _dir == Direction::SOUTH_EAST;
	horizontal -= _dir == Direction::SOUTH_WEST || _dir == Direction::WEST || _dir == Direction::NORTH_WEST;
	Sint8 vertical = 0;
	vertical += _dir == Direction::SOUTH_EAST || _dir == Direction::SOUTH || _dir == Direction::SOUTH_WEST;
	vertical -= _dir == Direction::NORTH_WEST || _dir == Direction::NORTH || _dir == Direction::NORTH_EAST;
	
	if(horizontal != 0)
	{
		Uint32 original = _index;
		_index += horizontal;
		if(_index / width != original / width) { return -1; }
	}
	if(vertical != 0)
	{
		_index += vertical * width;
		if(_index >= size || _index < 0) { return -1; }
	}
	return _index;
}

//Converts a hsv value to rgb. We smoothly interpolate colors using hsv, then convert to rgb so SDL can use them
SDL_Color Simulation::HsvToRgb(const HsvColor *_hsv) const
{
	SDL_Color rgba = {0, 0, 0, 255};
	
	if(_hsv->s == 0)
	{
		rgba.r = _hsv->v;
		rgba.g = _hsv->v;
		rgba.b = _hsv->v;
		return rgba;
	}

	Uint8 region, remainder, p, q, t;
	region = _hsv->h / 43;
	remainder = (_hsv->h - (region * 43)) * 6;
	p = (_hsv->v * (255 - _hsv->s)) >> 8;
	q = (_hsv->v * (255 - ((_hsv->s * remainder) >> 8))) >> 8;
	t = (_hsv->v * (255 - ((_hsv->s * (255 - remainder)) >> 8))) >> 8;

	switch(region)
	{
	case 0:
		rgba.r = _hsv->v; rgba.g = t; rgba.b = p;
		break;
	case 1:
		rgba.r = q; rgba.g = _hsv->v; rgba.b = p;
		break;
	case 2:
		rgba.r = p; rgba.g = _hsv->v; rgba.b = t;
		break;
	case 3:
		rgba.r = p; rgba.g = q; rgba.b = _hsv->v;
		break;
	case 4:
		rgba.r = t; rgba.g = p; rgba.b = _hsv->v;
		break;
	default:
		rgba.r = _hsv->v; rgba.g = p; rgba.b = q;
		break;
	}

	return rgba;
}

void Simulation::swapCell(Uint32 _current, Uint32 _next)
{
	Material tempMat = computeBuffer[_next];
	computeBuffer[_next] = computeBuffer[_current];
	computeBuffer[_current] = tempMat;
	Uint32 tempCol = drawBuffer[_next];
	drawBuffer[_next] = drawBuffer[_current];
	drawBuffer[_current] = tempCol;
	updatedCells->set(_next);
}

void Simulation::fillComputeBuffer(Material _mat)
{
	memset(computeBuffer, static_cast<int>(_mat), size * sizeof(Uint8));
}

void Simulation::fillDrawBuffer(Uint8 _color)
{
	memset(drawBuffer, _color, size * sizeof(Uint32));
}

//A highly efficient but imperfect random number generation algorithm
Uint32 Simulation::xorshift128()
{
	static Uint32 a = xorSeedDist(mt), b = xorSeedDist(mt), c = xorSeedDist(mt), d = xorSeedDist(mt);
	Uint32 t = d;
	Uint32 s = a;

	d = c;
	c = b;
	b = s;

	t ^= t << 11;
	t ^= t >> 8;
	a = t ^ s ^ (s >> 19);

	return a;
}