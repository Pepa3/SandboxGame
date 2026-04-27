#pragma once
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include "PerlinNoise.hpp"
#include <unordered_map>
#include <type_traits>
#include <iostream>
#include <cassert>
#include <fstream>
#include <memory>
#include <deque>

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
constexpr int caveCount = 7;
constexpr int caveDistance = 40;
constexpr int maxlightUpdateCount = 500;
constexpr int minUpdateTimeMillis = 25;
constexpr size_t INVENTORY_SIZE = 5;
constexpr size_t PLACE_MILLIS = 250;
constexpr size_t tileMapWidth = 10, tileMapHeight = 10;
constexpr size_t TERRAIN_STEEPNESS = 80;
constexpr SDL_Rect tileRect = {0,0,tileSize,tileSize};
constexpr SDL_FRect tileFRect = {0,0,tileSize,tileSize};
constexpr const siv::PerlinNoise::seed_type seed = 19254792u;
const siv::PerlinNoise perlin{seed};

/*
* TYPE DEFINITIONS
*/

enum Tile :uint8_t{
	UNKNOWN = 0, AIR = 1, DIRT = 2, STONE = 3, WOOD = 4, SAND = 5, LEAVES=6, GRASS=7, PLAYER=10, GLOW=13
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
constexpr Tile destroyResult(Tile t){
	switch(t){
	case GRASS:
		return DIRT;
	case GLOW:
		return STONE;
	default:
		return t;
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
	case GLOW:
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
class Block;
struct GameState;

class Block{
public:
	constexpr Block(Tile t1, Tile bgnd, float fl = 0, char lght = 0):t(t1),fluid(fl),bg(bgnd),light(lght){}
	Block() = default;
	float fluid = 0;
	Tile t = Tile::UNKNOWN;//Foreground tile
	Tile bg = Tile::UNKNOWN;//Background tile
	char light = 0;//0-127
	bool lightSource:1 = false;
	bool skyView:1 = false;
	bool hasScheduledLightUpdate:1 = false;
};

class Map{
public:
	class Chunk{
		friend Map;
	public:
		Chunk(Map* map, short x, short y);
		Chunk(Map* map) :x(0), y(0), map(map){}
		void generate();
		void generateTree(int x, int y);
		void update();
		void updateLight(int x, int y, bool genStep);
		void save(std::ofstream& out) const;
		void load(std::ifstream& in);
	private:
		short x, y;
		Block data[chSize * chSize];
		Map* map;
	};
	Map(GameState& game);
	~Map();
	void generateWorld();
	void update();
	void updateLight(int x, int y, bool genStep = false);
	void render();
	bool save(const std::string& file)const;
	bool load(const std::string& file);
	//void handleKeyDown(char key);
	void handleMouseWheel(SDL_MouseWheelEvent event);
	bool place(int x, int y, Tile t);
	Block destroy(int x, int y);
	bool isSolid(int x, int y);
	int tPosX(int x)const;
	int tPosY(int y)const;
	inline float posX(int x)const;
	inline float posY(int y)const;
	inline Block& world(int x, int y);
	inline Chunk* chunkAt(int x, int y);
	inline bool chunkExists(int x, int y)const;

private:
	GameState& game;
	std::unique_ptr<Player> player;
	std::unordered_map<uint32_t, Chunk*> chunks;
	std::deque<std::tuple<int, int, bool>> lightUpdateQueue;
};

class Player{
	friend Map;
public:
	Player(GameState& game, float x, float y);
	void render();
	void update();
	void save(std::ofstream& out) const;
	void load(std::ifstream& in);
	bool addInventory(Block b);
private:
	float x, y;
	float yVel = 0.f;
	bool onGround = false;
	struct Item{
		Tile type = Tile::UNKNOWN;
		size_t count = 0;
	} inventory[INVENTORY_SIZE];
	char selectedSlot = 0;
	uint64_t lastPlaceTicks = 0;
	int breakDurability = 0;
	int breakMaxDurability = 0;
	int breakX = 0, breakY = 0;
	GameState& game;
};

/*
* GLOBALS
*/
constexpr Block nullBlock = Block(Tile::UNKNOWN, Tile::UNKNOWN);
struct GameState{
	size_t wWidth, wHeight;
	SDL_Window* window;
	SDL_Renderer* renderer;
	TTF_Font* font;
	TTF_TextEngine* engine;
	TTF_Text* text;
	SDL_Texture* tiles[0xff];
	float cameraX, cameraY;
	std::unique_ptr<Map> map;
	bool overlayFluid = false;
	bool overlayLight = false;
#ifdef _DEBUG
	bool debugMode = true;
#else
	bool debugMode = false;
#endif
	int countLightUpdates;
	int countChunkGen;
};

int SDL_RenderCircle(SDL_Renderer* renderer, float x, float y, int radius);
int SDL_RenderFillCircle(SDL_Renderer* renderer, float x, float y, int radius);

template<class T>
concept writable = not requires(T& t, std::ofstream & o){
	t.save(o);
} and std::is_default_constructible_v<T>;
template<class T>
concept readable = not requires(T& t, std::ifstream & i){
	t.load(i);
} and std::is_default_constructible_v<T>;
template<writable T>
void write(std::ofstream& out, T* t){
	out.write((char*) t, sizeof(T));
}
template<readable T>
void read(std::ifstream& in, T* t){
	in.read((char*) t, sizeof(T));
}
