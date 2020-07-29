#pragma once

#include "SDL.h" 

#include <boost/dynamic_bitset.hpp>
#include <random>
#include <string>
#include <fstream>

const SDL_Color EMPTY_COLOR = {0, 0, 0, 255};

const Uint8 MAX_BEHAVIOR_SETS = 4;
const Uint8 MAX_BEHAVIORS_PER_SET = 8;
const Uint16 RAND_BATCH_SIZE = 4000;

const std::string MATERIAL_FILE_PATH = "../../Materials.json";

class Simulation
{
public:
	enum class Material : Uint8
	{
		EMPTY = 0,
		ROCK,
		SAND,
		WATER,
		FIRE,
		LAVA,
		OIL,
		ICE,
		GAS,
		STEAM,
		GRAVEL,
		WOOD,
		PLASMA,
		TOTAL_MATERIALS,
		NO_MATERIAL = 255
	};

	enum class Direction : Uint8
	{
		NORTH_WEST = 0,
		NORTH,
		NORTH_EAST,
		EAST,
		SOUTH_EAST,
		SOUTH,
		SOUTH_WEST,
		WEST,
		TOTAL_DIRECTIONS,
		NO_DIRECTION = 255,
	};

	struct HsvColor { Uint8 h, s, v; };

	struct MaterialSpecs
	{
		std::string name;
		HsvColor minColor, maxColor;
		Uint8 minSpeed, maxSpeed, density, deathChance;
		Sint8 temperature;
		bool solid, flaming, flammable, melting, meltable;
		Uint8 behaviorSetCount, behaviorCounts[MAX_BEHAVIOR_SETS];
		Direction behavior[MAX_BEHAVIOR_SETS][MAX_BEHAVIORS_PER_SET];
	};

	Simulation(int _width, int _height, Uint32 _pixelFormat);
	~Simulation();

	Uint32 *getDrawBuffer() const { return drawBuffer; };
	std::string getMaterialString() const;

	void update();
	void reset(Material _mat = Material::EMPTY, const SDL_Color *_col = &EMPTY_COLOR);
	void setPixelFormat(Uint32 _pixelFormat) { pixelFormat = SDL_AllocFormat(_pixelFormat); };
	void setCellLine(SDL_Point _start, SDL_Point _end, Uint16 _rad, Material _mat);

private:
	Uint16 width, height;
	Uint64 size;
	SDL_PixelFormat *pixelFormat;
	
	std::mt19937 mt;
	std::uniform_real_distribution<double> doubleDist;
	std::uniform_int_distribution<int> xorSeedDist;

	Material *computeBuffer;
	Uint32 *drawBuffer;
	Uint16 *batchNoise;
	Uint32 *iterationNoise;
	boost::dynamic_bitset<Uint64> *updatedCells;

	MaterialSpecs allSpecs[static_cast<int>(Material::TOTAL_MATERIALS)];

	Uint32 getRelative(Uint32 _index, Direction _dir) const;
	SDL_Color HsvToRgb(const HsvColor *_hsv) const;

	void setCell(Uint32 _index, Material _mat);
	void setCellIfValid(Sint32 _x, Sint32 _y, Material _mat);
	void setCellRadius(SDL_Point _pos, Uint16 _rad, Material _mat);
	void swapCell(Uint32 _current, Uint32 _next);
	Uint32 xorshift128();
};