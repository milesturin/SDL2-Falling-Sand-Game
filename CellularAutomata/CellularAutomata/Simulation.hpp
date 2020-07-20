#pragma once

#include "SDL.h" 

#include <string>
#include <random>
#include <ctime>
#include <boost/dynamic_bitset.hpp>

const Uint32 DEFAULT_COLOR = 0;
const Uint8 MAX_BEHAVIOR_SETS = 4;
const Uint8 MAX_BEHAVIORS_PER_SET = 8;
const Uint16 RAND_BATCH_SIZE = 4000;

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
		HsvColor minColor, maxColor;
		Uint8 minSpeed, maxSpeed;
		Uint8 density;
		Uint8 deathChance;
		bool solid;
		bool flaming;
		bool flammable;
		bool melting;
		bool meltable;
		Uint8 behaviorSetCount;
		Uint8 behaviorCounts[MAX_BEHAVIOR_SETS];
		Direction behavior[MAX_BEHAVIOR_SETS][MAX_BEHAVIORS_PER_SET];
	};

	Simulation(int _width, int _height, SDL_Texture *_tex);
	~Simulation();

	Uint32 *getDrawBuffer() const { return drawBuffer; };

	void update();
	void setCell(Uint32 _index, Material _mat);
	void setCellRadius(SDL_Point _pos, Uint16 _rad, Material _mat);
	void setCellFillLine(SDL_Point _start, SDL_Point _end, Uint16 _rad, bool _dir, bool _hor, Material _mat);
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
	bool isInBounds(Uint32 _x, Uint32 _y) const;
	SDL_Color HsvToRgb(const HsvColor *_hsv) const;

	void swapCell(Uint32 _current, Uint32 _next);
	void fillComputeBuffer(Material _mat = Material::EMPTY);
	void fillDrawBuffer(Uint8 _color = DEFAULT_COLOR);
	Uint32 xorshift128();
};