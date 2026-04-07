#include "Helper.h"
#include <cassert>

Map::Map(size_t mW, size_t mH) :mWidth(mW), mHeight(mH){
	world = new Tile[mW * mH];
	player = new Player(this, mW / 2.f * tileSize, tileSize * 10);
	cameraX = player->x;
	cameraY = player->y;
}

Map::~Map(){
	delete[mWidth * mHeight] world;
	delete player;
}

inline float Map::tPosX(int p)const{
	return tileSize * (p % mWidth) - cameraX + wWidth / 2.f;
}
inline float Map::tPosY(int p)const{
	return tileSize * (p / mWidth) - cameraY + wHeight / 2.f;
}
inline float Map::posX(int x)const{
	return tileSize * x - cameraX + wWidth / 2.f;
}
inline float Map::posY(int y)const{
	return tileSize * y - cameraY + wHeight / 2.f;
}

inline int Map::scrTile(float x, float y)const{
	int shiftedX = x + cameraX-wWidth/2.f;
	int shiftedY = y + cameraY-wHeight/2.f;
	if(shiftedX < 0 || shiftedY < 0)return 0;
	return shiftedX / tileSize + shiftedY / tileSize * mWidth;
}

bool Map::isSolid(int x, int y)const{
	if(x < 0 || x >= mWidth || y < 0 || y >= mHeight)return true;
	return ::isSolid(world[x + y * mWidth]);
}

void Map::generateWorld(){
	for(size_t x = 0; x < mWidth; x++){
		const int n1 = perlin.noise2D_01(x * 0.01, 1) * mHeight;
		const int n2 = perlin.noise2D_01(x * 0.01, 2) * n1;
		for(size_t y = 0; y < mHeight; y++){
			if(y >= n1)world[x + y * mWidth] = Tile::STONE;
			else if(y>=n2) world[x + y * mWidth] = Tile::DIRT;
			else world[x + y * mWidth] = Tile::AIR;
		}
	}
	for(size_t x = 0; x < mWidth; x++){
		if(rand() % 100 > 10)continue;
		const int n1 = perlin.noise2D_01(x * 0.01, 1) * mHeight;
		const int n2 = perlin.noise2D_01(x * 0.01, 2) * n1;
		for(size_t y = 0; y < rand()%12; y++){
			world[x + n2 * mWidth-y*mWidth] = Tile::WOOD;
		}
	}
}

void Map::handleKeyDown(char key){
}

void Map::place(int i, Tile t){
	world[i] = t;
}

void Map::update(){
	
	for(size_t i = 0; i < mWidth*(mHeight-1); i++){//TODO: this is a bad idea
		if(world[i] == Tile::SAND && world[i + mWidth] == Tile::AIR){
			world[i] = Tile::AIR;
			world[i + mWidth] = Tile::SAND;
		}
	}
	player->update();
}

void Map::render(){
	cameraX += (player->x - cameraX + tileSize / 2) / 10;
	cameraY += (player->y - cameraY + tileSize / 2) / 10;

	int beginX = fmaxf((cameraX - wWidth / 2) / tileSize, 0);
	int beginY = fmaxf((cameraY - wHeight / 2) / tileSize, 0);
	int endX = fminf((cameraX + wWidth / 2) / tileSize+1, mWidth);
	int endY = fminf((cameraY + wHeight / 2) / tileSize+1, mHeight);
	for(size_t x = beginX; x < endX; x++){
		for(size_t y = beginY; y < endY; y++){
			const SDL_FRect dest = {posX(x),posY(y),tileSize,tileSize};
			SDL_RenderTexture(renderer, tiles[world[x + y * mWidth]], &tileFRect, &dest);
		}
	}
	float mx, my;
	SDL_GetMouseState(&mx, &my);
	int t = scrTile(mx, my);

	const SDL_FRect cursorRect = {tPosX(t),tPosY(t),tileSize,tileSize};
	SDL_SetRenderDrawColor(renderer,0,0,0,0xff);
	SDL_RenderRect(renderer, &cursorRect);
	player->render();
}