#pragma once
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include "PerlinNoise.hpp"
#include <unordered_map>

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
constexpr size_t INVENTORY_SIZE = 5;
constexpr int tileSize = 32;
constexpr size_t tileMapWidth = 10, tileMapHeight = 10;
constexpr int chSize = 50;
constexpr size_t TERRAIN_STEEPNESS = 100;
constexpr SDL_Rect tileRect = {0,0,tileSize,tileSize};
constexpr SDL_FRect tileFRect = {0,0,tileSize,tileSize};
const siv::PerlinNoise::seed_type seed = 19254792u;
const siv::PerlinNoise perlin{seed};

/*
* TYPE DEFINITIONS
*/

enum Tile :uint8_t{
	UNKNOWN = 0, AIR = 1, DIRT = 2, STONE = 3, WOOD = 4, SAND = 5, LEAVES=6, PLAYER=10
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
class Map;
class Player;

class Map{
public:
	Map();
	~Map();
	void generateWorld();
	void update();
	void render();
	bool save(const std::string &file)const;
	bool load(const std::string& file);
	void handleKeyDown(char key);
	void place(int x, int y, Tile t);
	bool isSolid(int x, int y);
	int tPosX(int x)const;
	int tPosY(int y)const;
	inline float posX(int x)const;
	inline float posY(int y)const;
	inline Tile& world(int x, int y);
	class Chunk{
	public:
		Chunk(short x, short y);
		Chunk();
		void generate();
		void generateTree(int x, int y);
		void update();
		short x, y;
		Tile data[chSize * chSize];
	};
	
private:
	std::unordered_map<uint32_t, Chunk*> chunks;
	Player* player;
};

class Player{
	friend Map;
public:
	Player(float x, float y);
	void render();
	void update();
	//void save(std::ofstream& out)const;
	//void load(std::ifstream& in);
private:
	float x, y;
	float yVel=0.f;
	bool onGround=false;
	struct Item{
		Tile type;
		size_t count;
	} inventory[INVENTORY_SIZE] = {Tile::UNKNOWN};
};

/*
* GLOBALS
*/

#define GLOBALS \
GLOBAL(size_t wWidth)\
GLOBAL(size_t wHeight)\
GLOBAL(SDL_Window* window)\
GLOBAL(SDL_Renderer* renderer)\
GLOBAL(TTF_Font *font)\
GLOBAL(TTF_TextEngine* engine)\
GLOBAL(TTF_Text* text)\
GLOBAL(SDL_Texture* tiles[0xff])\
GLOBAL(float cameraX)\
GLOBAL(float cameraY)\
GLOBAL(uint64_t lastUpdateTicks)\
GLOBAL(Map* map)\

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