#pragma once
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include "PerlinNoise.hpp"
/*
* -
*-X+ XY: from left top to right bottom is positive 
* +
*/

/*
* CONSTANTS
*/

constexpr const char* TILEMAP_PATH = "./tilemap.png";
constexpr float PLAYER_SPEED = 7;
constexpr float GRAVITY = 0.5;
constexpr float JUMP_IMPULE = 8;
constexpr size_t tileSize = 32;
constexpr size_t tileMapWidth = 10, tileMapHeight = 10;
constexpr SDL_Rect tileRect = {0,0,tileSize,tileSize};
constexpr SDL_FRect tileFRect = {0,0,tileSize,tileSize};
const siv::PerlinNoise::seed_type seed = 19254792u;
const siv::PerlinNoise perlin{seed};

/*
* TYPE DEFINITIONS
*/

enum Tile :uint8_t{
	UNKNOWN = 0, AIR = 1, DIRT = 2, STONE = 3, COUNT
};
constexpr bool isSolid(Tile t){
	switch(t){
	case AIR:
		return false;
	default:
		return true;
	}
}

/*
* CLASS DEFINITIONS
*/

class Map{
public:
	const size_t mWidth, mHeight;
	Map(size_t mW, size_t mH);
	~Map();
	void generateWorld();
	void update();
	void render();
	void handleKeyDown(char key);
	bool isSolid(int x, int y)const;
	inline float tPosX(int p)const;
	inline float tPosY(int p)const;
	inline int scrTile(float x, float y)const;

private:
	Tile* world;
	struct player_t{
		float x, y;
		float yVel;
		bool onGround;
	} player;
};

/*
* GLOBALS
*/

#define GLOBALS \
GLOBAL(size_t wWidth)\
GLOBAL(size_t wHeight)\
GLOBAL(SDL_Window* window)\
GLOBAL(SDL_Renderer* renderer)\
GLOBAL(SDL_Texture* tiles[0xff])\
GLOBAL(float cameraX)\
GLOBAL(float cameraY)\
GLOBAL(uint64_t lastUpdateTicks)\

#ifdef HELPER_INIT
# define GLOBAL(what) what;
  GLOBALS
# undef GLOBAL
#else
# define GLOBAL(what) extern what;
  GLOBALS
# undef GLOBAL
#endif

int SDL_RenderCircle(SDL_Renderer* renderer, int x, int y, int radius);
int SDL_RenderFillCircle(SDL_Renderer* renderer, int x, int y, int radius);