#include "Simulation.hpp"

#include "Graphics.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
//asdfasdfasdfasdfasdf
#include <iostream>

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
	drawBuffer = new Uint32[size];
	reset();
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

	//Loads in material properties from a json file.
	//Behavior sets can be biased by using duplicate behaviors. However, 
	//if less biased behaviors are not equally distributed from a left to right perspective, unwanted bias can occur.
	boost::property_tree::ptree root;
	boost::property_tree::read_json(MATERIAL_FILE_PATH, root);
	for(auto it = root.begin(); it != root.end(); ++it)
	{
		int dist = std::distance(root.begin(), it);
		if(dist >= static_cast<int>(Material::TOTAL_MATERIALS) - 1) { break; }
		MaterialSpecs &mat = allSpecs[dist + 1];
		mat.name = it->first;
		mat.textPadding = it->second.get<int>("textPadding");
		auto color = it->second.get_child("minColor");
		mat.minColor = {color.get<Uint8>("h"), color.get<Uint8>("s"), color.get<Uint8>("v")};
		color = it->second.get_child("maxColor");
		mat.maxColor = {color.get<Uint8>("h"), color.get<Uint8>("s"), color.get<Uint8>("v")};
		mat.minSpeed = it->second.get<int>("minSpeed");
		mat.maxSpeed = it->second.get<int>("maxSpeed");
		mat.density = it->second.get<int>("density");
		mat.deathChance = it->second.get<int>("deathChance");
		mat.solid = it->second.get<bool>("solid");
		mat.flaming = it->second.get<bool>("flaming");
		mat.flammable = it->second.get<bool>("flammable");
		mat.melting = it->second.get<bool>("melting");
		mat.meltable = it->second.get<bool>("meltable");
		int i = 0;
		for(auto arr : it->second.get_child("behavior"))
		{
			int j = 0;
			for(auto dir : arr.second)
			{
				mat.behavior[i][j] = static_cast<Direction>(dir.second.get_value<int>());
				++j;
			}
			mat.behaviorCounts[i] = j;
			++i;
		}
		mat.behaviorSetCount = i;
	}
}

Simulation::~Simulation()
{
	delete[] computeBuffer;
	delete[] drawBuffer;
	delete[] batchNoise;
	delete[] iterationNoise;
	delete updatedCells;
}

std::string Simulation::getFormattedNames() const
{
	std::string result;
	for(int i = 1; i < static_cast<int>(Material::TOTAL_MATERIALS); ++i)
	{
		for(int j = 0; j < allSpecs[i].textPadding; ++j) { result += ' '; }
		result += allSpecs[i].name;
		if(i % 3 == 0) { result += '\n'; }
	}
	return result;
}

void Simulation::update()
{
	//Because RNG is the main computational bottleneck, we create only a fraction of the needed numbers,
	//then pick them using a pre-generated noise array. Each random number is used only with the modulo operator,
	//so we can reuse each number several times by dividing by ten after each use.
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
						//Chemical and physical reactions. General properties use booleans, while specific interactions are hard coded
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
							if(collisionSpecs->flaming || collisionSpecs->melting)
							{
								setCell(index, Material::STEAM);
								destroyed = true;

								if(computeBuffer[newIndex] == Material::LAVA && preGenRandRange(0, 1) == 0) 
								{ 
									setCell(newIndex, Material::GRAVEL); 
								}
								else
								{
									setCell(newIndex, Material::EMPTY);
								}

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

void Simulation::reset(Material _mat, const SDL_Color *_col)
{
	memset(computeBuffer, static_cast<int>(_mat), size * sizeof(Uint8));
	memset(drawBuffer, Graphics::getColorInt(_col), size * sizeof(Uint32));
}

//Draws a thick line between two points. This is used so that when the cursor is moved quickly it makes a contiguous line instead of dots
void Simulation::setCellLine(SDL_Point _start, SDL_Point _end, Uint16 _rad, Material _mat)
{
	setCellRadius(_start, _rad, _mat);
	if(!(_start.x == _end.x && _start.y == _end.y))
	{
		setCellRadius(_end, _rad, _mat);
		float angle = -atan2(_end.x - _start.x, _end.y - _start.y);
		Sint32 tanDiffX = round(cos(angle) * _rad);
		Sint32 tanDiffY = round(sin(angle) * _rad);
		Sint32 *tanLineX, *tanLineY, *mainLineX, *mainLineY;
		Uint16 tanLen = Graphics::bresenhams(_start.x + tanDiffX, _start.y + tanDiffY, _start.x - tanDiffX, _start.y - tanDiffY, tanLineX, tanLineY, true);
		Uint16 mainLen = Graphics::bresenhams(_start.x + tanDiffX, _start.y + tanDiffY, _end.x + tanDiffX, _end.y + tanDiffY, mainLineX, mainLineY, false);
		for(int i = 0; i < tanLen; ++i)
		{
			for(int j = 0; j < mainLen; ++j)
			{
				Sint32 diffX = (_start.x + tanDiffX) - tanLineX[i];
				Sint32 diffY = (_start.y + tanDiffY) - tanLineY[i];
				setCellIfValid(mainLineX[j] - diffX, mainLineY[j] - diffY, _mat);
			}
		}
		delete[] tanLineX;
		delete[] tanLineY;
		delete[] mainLineX;
		delete[] mainLineY;
	}
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

//Sets a cell to a material. Interpolates between colors to add visual variation
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
		drawBuffer[_index] = SDL_MapRGBA(pixelFormat, EMPTY_COLOR.r, EMPTY_COLOR.g, EMPTY_COLOR.b, EMPTY_COLOR.a);
	}

	updatedCells->set(_index);
}

void Simulation::setCellIfValid(Sint32 _x, Sint32 _y, Material _mat)
{
	Uint32 index = _x + _y * width;
	if(_y >= 0 && _y < height && _x >= 0 && _x < width && (_mat == Material::EMPTY || computeBuffer[index] == Material::EMPTY))
	{
		setCell(index, _mat);
	}
}


//Sets a circle of cells by radius for user input
void Simulation::setCellRadius(SDL_Point _pos, Uint16 _rad, Material _mat)
{
	for(int i = 0; i < _rad * 2; ++i)
	{
		Uint16 slice = sqrt(pow(_rad, 2) - pow(abs(_rad - i), 2));
		for(int j = -slice; j < slice; ++j)
		{
			Sint32 x = _pos.x + j;
			Sint32 y = _pos.y - _rad + i;
			setCellIfValid(x, y, _mat);
		}
	}
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