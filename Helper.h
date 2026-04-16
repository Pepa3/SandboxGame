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
constexpr float SINK_RATE = 1;
constexpr float JUMP_IMPULE = 8;
constexpr int UPDATE_RADIUS = 3;
constexpr int tileSize = 32;
constexpr int chSize = 50;
constexpr int dirtHeight = 5;
constexpr int lightFalloff = 2;
constexpr int caveCount = 3;
constexpr size_t INVENTORY_SIZE = 5;
constexpr size_t PLACE_MILLIS = 250;
constexpr size_t tileMapWidth = 10, tileMapHeight = 10;
constexpr size_t TERRAIN_STEEPNESS = 80;
constexpr SDL_Rect tileRect = {0,0,tileSize,tileSize};
constexpr SDL_FRect tileFRect = {0,0,tileSize,tileSize};
const siv::PerlinNoise::seed_type seed = 19254792u;
const siv::PerlinNoise perlin{seed};

/*
* TYPE DEFINITIONS
*/

enum Tile :uint8_t{
	UNKNOWN = 0, AIR = 1, DIRT = 2, STONE = 3, WOOD = 4, SAND = 5, LEAVES=6, GRASS=7, PLAYER=10
};
constexpr bool isSolid(Tile t){
	switch(t){
	case AIR:
	case LEAVES:
		return false;
	default:
		return true;
	}
}
constexpr bool hasBackground(Tile t){
	switch(t){
	case AIR:
		return true;
	default:
		return false;
	}
}
constexpr int durability(Tile t){
	switch(t){
	case SAND:
		return 10;
	case GRASS:
	case DIRT:
	case WOOD:
		return 20;
	case STONE:
		return 100;
	default:
		return 1;
	}
}

/*
* CLASS DEFINITIONS
*/
class Map;
class Player;

class Block{
public:
	Block(Tile t1, Tile bgnd, float fl = 0, uint8_t lght = 0):t(t1),fluid(fl),bg(bgnd),light(lght){}
	Block(){}
	Tile t = Tile::UNKNOWN;//Foreground tile
	Tile bg = Tile::UNKNOWN;//Background tile
	float fluid = 0;
	char light = 0;//0-127 128-255
	bool lightSource = false;
	bool skyView = false;
};

class Map{
public:
	class Chunk{
	public:
		Chunk(short x, short y);
		Chunk() :x(0), y(0){}
		void generate();
		void generateTree(int x, int y);
		void update();
		void updateLight(int x, int y, bool genStep);
		short x, y;
		Block data[chSize * chSize];
	};
	Map();
	~Map();
	void generateWorld();
	void update();
	void updateLight(int x, int y);
	void render();
	bool save(const std::string &file)const;
	bool load(const std::string& file);
	void handleKeyDown(char key);
	void handleMouseWheel(SDL_MouseWheelEvent event);
	bool place(int x, int y, Tile t);
	Block& destroy(int x, int y);
	bool isSolid(int x, int y);
	int tPosX(int x)const;
	int tPosY(int y)const;
	inline float posX(int x)const;
	inline float posY(int y)const;
	inline Block& world(int x, int y);
	Chunk* chunkAt(int x, int y);
	
private:
	Player* player;
	std::unordered_map<uint32_t, Chunk*> chunks;
};

class Player{
	friend Map;
public:
	Player(float x, float y);
	void render();
	void update();
	bool addInventory(Block b);
private:
	float x, y;
	float yVel=0.f;
	bool onGround=false;
	struct Item{
		Tile type = Tile::UNKNOWN;
		size_t count = 0;
	} inventory[INVENTORY_SIZE];
	char selectedSlot = 0;
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
GLOBAL(uint64_t lastPlaceTicks)\
GLOBAL(int breakDurability)\
GLOBAL(int breakMaxDurability)\
GLOBALI(int breakX,0)\
GLOBALI(int breakY,0)\
GLOBAL(Map* map)\
GLOBALI(Block nullBlock, Block(Tile::UNKNOWN,Tile::UNKNOWN))\
GLOBALI(bool overlayFluid, false)\
GLOBALI(bool overlayLight, false)\
GLOBALI(bool debugMode, true)\

#ifdef HELPER_INIT
# define GLOBAL(what) what;
# define GLOBALI(what, init) what = init;
  GLOBALS
# undef GLOBAL
# undef GLOBALI
#else
# define GLOBAL(what) extern what;
# define GLOBALI(what,init) extern what;
  GLOBALS
# undef GLOBAL
# undef GLOBALI
#endif

int SDL_RenderCircle(SDL_Renderer* renderer, float x, float y, int radius);
int SDL_RenderFillCircle(SDL_Renderer* renderer, float x, float y, int radius);