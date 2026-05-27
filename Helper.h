#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include "SDL_FontCache/SDL_FontCache.h"
#include "PerlinNoise.hpp"
#include <unordered_map>
#include <type_traits>
#include <iostream>
#include <cassert>
#include <fstream>
#include <chrono>
#include <memory>
#include <thread>
#include <queue>

/*
* -
*-X+ XY: from left top to right bottom is positive 
* +
*/

/*
* CONSTANTS
*/

constexpr const char* cTilemapPath = "./tilemap.png";
constexpr float cPlayerSpeed = 7;
constexpr float cGravity = 0.5;
constexpr float cSinkRate = 1;
constexpr float cJumpImpulse = 8;
constexpr int cUpdateRadius = 5;
constexpr float itemSize = 12;
constexpr int tileSize = 32;
constexpr int chSize = 50;
constexpr int dirtHeight = 5;
constexpr char lightFalloff = 2;
constexpr int caveCount = 7;
constexpr int caveDistance = 40;
constexpr int maxlightUpdateCount = 500;
constexpr int minUpdateTimeMillis = 25;
constexpr uint8_t cHotbarSize = 5;
constexpr uint8_t cInventorySize = cHotbarSize*4; 
constexpr uint8_t cItemStackSize = 10;
constexpr size_t cPlaceTimeoutMillis = 250;
constexpr size_t tileMapWidth = 10, tileMapHeight = 10;
constexpr SDL_Rect tileRect{0,0,tileSize,tileSize};
constexpr SDL_FRect tileFRect{0,0,tileSize,tileSize};
const std::string magic{"JustAGame"};
constexpr size_t mapSeed = 19254792u;
//constexpr size_t mapSeed = 1546916105u;
const siv::BasicPerlinNoise<float> perlin{mapSeed};

/*
* TYPE DEFINITIONS
*/

template<typename S>
concept scalar = std::is_scalar_v<S>;

template<scalar T, int R>
class position{
public:
	T x, y;
	constexpr position(T x, T y)  :x(x), y(y) {} ;
	template<scalar U, int S>
	operator const position<U, S>()const {
		if constexpr(R<S)
			return position<U, S>((U)(floor((x * (T)R) / (double)S)), (U)(floor((y * (T)R) / (double)S)));
		else
			return position<U, S>((U)((x * (T)R) / S), (U)((y * (T)R) / S));
	}
	position<T, R> operator-(const position<T, R>& p)const {
		return {x - p.x,y - p.y};
	}
	position<T, R> operator+(const position<T, R>& p)const {
		return {x + p.x,y + p.y};
	}
	bool operator==(const position<T, R>& p)const{
		return x == p.x && y == p.y;
	}
	position<T, R> down(T n = 1)const{
		return {x, y + n};
	}
	position<T, R> right(T n = 1)const{
		return {x + n, y};
	}
	position<T, R> left(T n = 1)const{
		return {x - n, y};
	}
	T dist(const position<T, R>& other){
		return std::hypot<T>(x-other.x,y-other.y);
	}
};
class fluid_t{
public:
	enum type{NONE,WATER,LAVA};
	constexpr fluid_t(type kind, float value):kind(kind),value(value){}
	type kind = NONE;
	float value = 0;
	operator bool()const{
		return kind != NONE;
	}
};
constexpr fluid_t nullFluid = fluid_t(fluid_t::NONE, 0.f);

using posWorld = position<float, 1>;
using posTile = position<int, tileSize>;
using posChunk = position<short, tileSize*chSize>;
static_assert(sizeof(posChunk) == sizeof(uint32_t));

//TODO: should check if this is a good idea
template<>
struct std::hash<posChunk>{
	size_t operator()(const posChunk& p) const{
		return (((size_t)p.y) << 16) | (p.x & 0xffff);
	}
};

template<class T>
concept writable = not requires(const T & t, std::ofstream & o){
	t.save(o);
}and std::is_default_constructible_v<T>;
template<class T>
concept readable = not requires(T & t, std::ifstream & i){
	t.load(i);
}and std::is_default_constructible_v<T>;

template<writable T>
void write(std::ofstream& out, const T* t){
	out.write(reinterpret_cast<const char*>(t), sizeof(T));
}
void write(std::ofstream& out, const std::string& s);

template<readable T>
void read(std::ifstream& in, T* t){
	in.read(reinterpret_cast<char*>(t), sizeof(T));
}
void read(std::ifstream& in, std::string& t);

constexpr char lfmax(char a, char b){ return std::max(a, b); };
inline char lfabs(char a){ return (char) (std::abs(a)); };

enum class Tile :uint8_t{
	UNKNOWN = 0, AIR = 1, DIRT = 2, STONE = 3, WOOD = 4, SAND = 5, LEAVES=6, GRASS=7, CRAFTER=8, PLANKS=9, PLAYER=10, GLOW=13, COAL=14
};
enum class Tool :uint8_t{
	HAND,PICKAXE
};
enum class Biome :uint8_t{
	PLAINS,FOREST,DESERT,MOUNTAINS
};

bool isSolid(Tile t);
bool hasBackground(Tile t);
Tile destroyResult(Tile t, Tool l);
int durability(Tile t);
const char* biomeName(Biome b);

/*
* CLASS DEFINITIONS
*/
class Map;
class Player;
class Item;
class Entity;
struct GameState;

class Block{
public:
	constexpr Block(Tile t1, Tile bgnd, fluid_t fl=nullFluid, char lght = 0) :fluid(fl), t(t1), bg(bgnd), light(lght){}
	Block() = default;
	fluid_t fluid{nullFluid};//0 - 1 - 1.25 - 1.25+0.25*depth
	Tile t = Tile::UNKNOWN;//Foreground tile
	Tile bg = Tile::UNKNOWN;//Background tile
	char light = 0;//0-127
	bool lightSource:1 = false;
	bool skyView:1 = false;
	bool hasScheduledLightUpdate:1 = false;
};

class Entity{
	friend Map;
public:
	Entity(posWorld pos) :pos(pos){};
	Entity(float x, float y) :Entity(posWorld{x,y}){};
	void update(const GameState& game);
	void render(const GameState& game)const;
	void save(std::ofstream& out) const;
	void load(std::ifstream& in);
private:
	posWorld pos{0,0};
	float yVel = 0.f;
	bool dead:1 = false;
};

class Map{
public:
	class Chunk{
		friend Map;
	public:
		Chunk(Map* map, posChunk p);
		Chunk(Map* map, short x, short y) :Chunk(map, {x,y}){}
		Chunk(Map* map) :pos(0,0), map(map){}
		void generate();
		void generateTree(int tx, int ty);
		void generateTree(posTile p){ generateTree(p.x,p.y); }
		void update();
		void updateLight(int x, int y, bool genStep);
		void updateLight(posTile p, bool genStep){ updateLight(p.x, p.y, genStep); }
		void save(std::ofstream& out) const;
		void load(std::ifstream& in);
	private:
		posChunk pos;
		std::array<Block, chSize* chSize> data{};
		std::vector<Item> items{};
		Map* map;
	};
	Map(GameState& game);
	void generateWorld();
	void update();
	void updateLight(posTile p, bool genStep = false);
	void updateLight(int x, int y, bool genStep = false){ updateLight({x, y}, genStep); }
	void render()const;
	bool save(const std::string& file)const;
	bool load(const std::string& file);
	bool place(int x, int y, Tile t);
	bool place(posTile p, Tile t){ return place(p.x, p.y, t); }
	Block destroy(posTile p, Tool tool);
	Block destroy(int x, int y, Tool tool){ return destroy({x,y}, tool); }
	int tPosX(float x)const;
	int tPosY(float y)const;
	float posX(int x)const;
	float posY(int y)const;
	Block& world(posTile p);
	const Block& world(posTile p)const;
	Block& world(int x, int y){ return world({x,y}); }
	const Block& world(int x, int y)const { return world({x,y}); }
	Chunk* chunkAt(posChunk p);
	const Chunk* chunkAt(posChunk p)const;
	Chunk* chunkAt(short x, short y){ return chunkAt({x, y});}
	const Chunk* chunkAt(short x, short y)const { return chunkAt({x, y}); }
	bool chunkExists(posChunk p)const;
	bool chunkExists(short x, short y)const{ return chunkExists({x,y}); }
	int terrainHeight(int x)const;
	Biome getBiome(posChunk pos)const;
	void spawnEntity(posTile pos);

private:
	GameState& game;
	std::unordered_map<posChunk, std::unique_ptr<Chunk>> chunks{};
	std::queue<std::pair<posTile, bool>> lightUpdateQueue;
	std::vector<Entity> entities{};
};

class Item{
	friend Map::Chunk;
public:
	Item(Tile t, posWorld pos) :t(t), pos(pos){}
	Item() = default;
	void update(const GameState& game);
	void render(const GameState& game)const;
private:
	Tile t = Tile::UNKNOWN;
	float yVel = 0;
	posWorld pos{0,0};
	bool pickedUp = false;
};

class Player{
	friend Map;
	friend Item;
	friend Entity;
public:
	Player(GameState& game, posWorld p);
	void renderInventory()const;
	void render()const;
	void update();
	void handleMouseWheel(SDL_MouseWheelEvent e);
	void handleKeyDown(char key);
	void handleMouseDown(SDL_MouseButtonEvent e);
	void handleMouseUp(SDL_MouseButtonEvent e);
	void handleMouseMotion(SDL_MouseMotionEvent e);
	void save(std::ofstream& out) const;
	void load(std::ifstream& in);
	bool addInventory(Tile t);
private:
	struct ItemSlot{
		Tile type = Tile::UNKNOWN;
		uint8_t count = 0;
	};
	posWorld pos;
	float yVel = 0.f;
	ItemSlot hotbar[cHotbarSize];
	ItemSlot inventory[cInventorySize];
	ItemSlot holding{};
	bool openInventory = false;
	char selectedSlot = 0;
	uint64_t lastPlaceTicks = 0;
	int breakDurability = 0;
	int breakMaxDurability = 0;
	posTile brokenBlock{0,0};
	GameState& game;
};

/*
* GLOBALS
*/
constexpr Block nullBlock = Block(Tile::UNKNOWN, Tile::UNKNOWN);
struct GameState{
	size_t wWidth = 0, wHeight = 0;
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	FC_Font* font = nullptr;
	std::array<SDL_Texture*, 0xff> tiles{};
	posWorld camera{0,0};
	std::unique_ptr<Map> map;
	std::unique_ptr<Player> player;
	std::jthread updateThread;
	bool overlayFluid = false;
	bool overlayLight = false;
#ifdef _DEBUG
	bool debugMode = true;
#else
	bool debugMode = false;
#endif
	int countLightUpdates = 0;
	int countChunkGen = 0;
};

int SDL_RenderCircle(SDL_Renderer* renderer, float x, float y, int radius);
int SDL_RenderFillCircle(SDL_Renderer* renderer, float x, float y, int radius);
