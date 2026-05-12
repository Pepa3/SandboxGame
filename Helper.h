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
constexpr int cUpdateRadius = 3;
constexpr int tileSize = 32;
constexpr int chSize = 50;
constexpr int dirtHeight = 5;
constexpr char lightFalloff = 2;
constexpr int caveCount = 7;
constexpr int caveDistance = 40;
constexpr int maxlightUpdateCount = 500;
constexpr int minUpdateTimeMillis = 25;
constexpr uint8_t cInventorySize = 5;
constexpr size_t cPlaceTimeoutMillis = 250;
constexpr size_t tileMapWidth = 10, tileMapHeight = 10;
constexpr size_t cTerrainSteepness = 80;
constexpr SDL_Rect tileRect = {0,0,tileSize,tileSize};
constexpr SDL_FRect tileFRect = {0,0,tileSize,tileSize};
constexpr size_t mapSeed = 19254792u;
//constexpr uint32_t mapSeed = 1546916105u;
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
	position(T x, T y)  :x(x), y(y) {} ;
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
};

using posWorld = position<float, 1>;
using posTile = position<int, tileSize>;
using posChunk = position<short, tileSize*chSize>;

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
template<readable T>
void read(std::ifstream& in, T* t){
	in.read(reinterpret_cast<char*>(t), sizeof(T));
}

inline char lfmax(char a, char b){ return (char) (fmax(a, b)); };
inline char lfabs(char a){ return (char) (fabs(a)); };

enum class Tile :uint8_t{
	UNKNOWN = 0, AIR = 1, DIRT = 2, STONE = 3, WOOD = 4, SAND = 5, LEAVES=6, GRASS=7, PLAYER=10, GLOW=13, COAL=14
};
enum class Tool :uint8_t{
	HAND,PICKAXE
};
bool isSolid(Tile t);
bool hasBackground(Tile t);
Tile destroyResult(Tile t, Tool l);
int durability(Tile t);

/*
* CLASS DEFINITIONS
*/
class Map;
class Player;
struct GameState;

class Block{
public:
	constexpr Block(Tile t1, Tile bgnd, float fl = 0, char lght = 0):t(t1),fluid(fl),bg(bgnd),light(lght){}
	Block() = default;
	float fluid = 0;//0 - 1 - 1.25 - 1.25+0.25*depth
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
		Map* map;
	};
	Map(GameState& game);
	void generateWorld();
	void update();
	void updateLight(posTile p, bool genStep = false);
	void updateLight(int x, int y, bool genStep = false){ updateLight({x, y}, genStep); }
	void render();
	bool save(const std::string& file)const;
	bool load(const std::string& file);
	//void handleKeyDown(char key);
	void handleMouseWheel(SDL_MouseWheelEvent event) ;
	bool place(int x, int y, Tile t);
	bool place(posTile p, Tile t){ return place(p.x, p.y, t); }
	Block destroy(posTile p, Tool tool);
	Block destroy(int x, int y, Tool tool){ return destroy({x,y}, tool); }
	bool isSolid(posTile p)const;
	bool isSolid(int x, int y)const{ return isSolid({x,y}); }
	int tPosX(float x)const;
	int tPosY(float y)const;
	float posX(int x)const;
	float posY(int y)const;
	Block& world(posTile p);
	const Block& world(posTile p)const;
	Block& world(int x, int y){ return world({x,y}); }
	const Block& world(int x, int y)const { return world({x,y}); }
	Chunk* chunkAt(posChunk p);
	Chunk* chunkAt(short x, short y){ return chunkAt({x, y});}
	bool chunkExists(posChunk p)const;
	bool chunkExists(short x, short y)const{ return chunkExists({x,y}); }

private:
	GameState& game;
	std::unique_ptr<Player> player;
	std::unordered_map<uint32_t, std::unique_ptr<Chunk>> chunks;
	std::queue<std::pair<posTile, bool>> lightUpdateQueue;
};

class Player{
	friend Map;
public:
	Player(GameState& game, posWorld p);
	void render()const;
	void update();
	void save(std::ofstream& out) const;
	void load(std::ifstream& in);
	bool addInventory(Block b);
private:
	posWorld pos;
	float yVel = 0.f;
	bool onGround = false;
	struct Item{
		Tile type = Tile::UNKNOWN;
		uint8_t count = 0;
	} inventory[cInventorySize];
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
	TTF_Font* font = nullptr;
	TTF_TextEngine* engine = nullptr;
	TTF_Text* text = nullptr;
	std::array<SDL_Texture*, 0xff> tiles = std::array<SDL_Texture*, 0xff>();
	posWorld camera{0,0};
	std::unique_ptr<Map> map;
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
