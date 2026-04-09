#include "Helper.h"
#include <cassert>
#include <iostream>
#include <fstream>

Map::Map(){
	chunks = std::unordered_map<uint32_t, Chunk*>();
	const double n1 = perlin.noise2D(20 * 0.01, 1) * TERRAIN_STEEPNESS + 100;
	player = new Player(20.f * tileSize, (float)tileSize*(((int)n1)-1));
	cameraX = player->x;
	cameraY = player->y;
}

Map::~Map(){
	for(auto& ch : chunks){
		if(ch.second!=nullptr)
		delete ch.second;
	}
	delete player;
}

inline float Map::posX(int x)const{
	return tileSize * x - cameraX + wWidth / 2.f;
}
inline float Map::posY(int y)const{
	return tileSize * y - cameraY + wHeight / 2.f;
}
int Map::tPosX(int x)const{
	return (int) floorf((x + cameraX - wWidth / 2.f) / tileSize);
}
int Map::tPosY(int y)const{
	return (int) floorf((y + cameraY - wHeight / 2.f) / tileSize);
}
#define KEY(high,low)(((uint32_t)high << 16) | ((uint32_t) low)&0xFFFF)

inline Block& Map::world(int x, int y){
	short cx = (short) floorf(x / (float) chSize);
	short cy = (short) floorf(y / (float) chSize);
	uint32_t key = KEY(cx, cy);
	try{
		return chunks.at(key)->data[(x - cx * chSize) + (y - cy * chSize) * chSize];
	} catch(const std::exception& _){
		Chunk* ch = new Chunk(cx, cy);
		ch->generate();
		chunks.insert({key, ch});
		return chunks.at(key)->data[(x - cx * chSize) + (y - cy * chSize) * chSize];
	}
}

inline bool Map::isSolid(int x, int y){
	return ::isSolid(world(x,y));
}

void Map::generateWorld(){
	for(int i = -2; i <= 2; i++){
		for(int j = -2; j <= 2; j++){
			Chunk* ch = new Chunk(i,j);
			ch->generate();
			chunks.insert({KEY(i,j), ch});
		}
	}
}

Map::Chunk::Chunk():x(0),y(0){}

Map::Chunk::Chunk(short x, short y) :x(x), y(y) {
	std::cout << "Created chunk [" << x << "][" << y << "]" << std::endl;
	for(size_t i = 0; i < chSize*chSize; i++){
		data[i] = Tile::AIR;
	}
}

void Map::Chunk::generate(){
	constexpr int dirtHeight = 5;
	int gx = x * chSize;
	int gy = y * chSize;
	int treeHeight = (int) (perlin.noise2D((gx + chSize / 2) * 0.01, 1) * TERRAIN_STEEPNESS) + 100;
	for(int i = 0; i < chSize; i++){
		const int n1 = (int)(perlin.noise2D((gx+i) * 0.01, 1) * TERRAIN_STEEPNESS)+100;
		for(int j = 0; j < chSize; j++){
			if(j + gy < n1){
				Block b = {Tile::AIR,0};
				if(gy+j>120)b.fluid = 1;
				data[i + j * chSize] = b;
			}
			else if(j+gy<n1+dirtHeight)data[i + j * chSize] = Tile::DIRT;
			else data[i + j * chSize] = Tile::STONE;

		}
	}
	if(treeHeight-gy < chSize)generateTree(chSize/2, treeHeight-gy);
}

void Map::Chunk::generateTree(int tx, int ty){
	for(int i = 0; i < 7; i++){
		if(ty - i >= 0){
			data[tx + (ty - i) * chSize] = Tile::WOOD;
		}
	}
	for(int j = 1; j <= 3; j++){
		for(int i = 2+j; i < 9-j; i++){
			if(ty - i >= 0){
				data[-j + tx + (ty - i) * chSize] = Tile::LEAVES;
				data[j + tx + (ty - i) * chSize] = Tile::LEAVES;
			}
		}
	}
	if(ty>=7)
	data[tx + (ty - 7) * chSize] = Tile::LEAVES;
}

void Map::Chunk::update(){
	/*for(size_t i = 0; i < mWidth * (mHeight - 1); i++){//TODO: this is a bad idea
		if(world[i] == Tile::SAND && world[i + mWidth] == Tile::AIR){
			world[i] = Tile::AIR;
			world[i + mWidth] = Tile::SAND;
		}
	}*/
}

void Map::handleKeyDown(char key){
}

void Map::place(int x, int y, Tile t){
	world(x,y) = t;
}

void Map::update(){
	int beginX = (int) (cameraX - wWidth / 2) / tileSize - chSize;
	int beginY = (int) (cameraY - wHeight / 2) / tileSize - chSize;
	int endX = (int) (cameraX + wWidth / 2) / tileSize + chSize;
	int endY = (int) (cameraY + wHeight / 2) / tileSize + chSize;
	for(int x = beginX; x < endX; x++){
		for(int y = beginY; y < endY; y++){
			Block& b = world(x, y);
			if(b == Tile::SAND && world(x,y+1) == Tile::AIR){
				Block c = world(x, y + 1);
				Block tmp = b;
				b = c;
				world(x, y + 1) = tmp;
			}
			double& fl1 = b.fluid;
			if(fl1 > 0){
				double& fl2 = world(x, y + 1).fluid;
				if(!isSolid(x, y + 1)){
					double fl = fl1 + fl2;
					if(fl > 1){
						fl2 = 1;
						fl1 = fl - 1;
					} else{
						fl2 = fl;
						fl1 = 0;
					}
				}
				double fldl = fl1 - world(x - 1, y).fluid;
				double fldr = fl1 - world(x + 1, y).fluid;
				if(!isSolid(x - 1, y) && fldl > 0){
					fl1 -= fldl / 3;
					world(x - 1, y).fluid += fldl / 3;
				}
				if(!isSolid(x + 1, y) && fldr > 0){
					fl1 -= fldr / 3;
					world(x + 1, y).fluid += fldr / 3;
				}
			}
		}
	}
	
	player->update();
}

void Map::render(){
	cameraX += (player->x - cameraX + tileSize / 2) / 10;
	cameraY += (player->y - cameraY + tileSize / 2) / 10;

	int beginX = (int) (cameraX - wWidth / 2) / tileSize-1;
	int beginY = (int) (cameraY - wHeight / 2) / tileSize-1;
	int endX = (int) (cameraX + wWidth / 2) / tileSize+1;
	int endY = (int) (cameraY + wHeight / 2) / tileSize+1;
	for(int x = beginX; x < endX; x++){
		for(int y = beginY; y < endY; y++){
			//TODO: SLOW -> looks up chunks every iteration

			Block& b = world(x, y);
			const SDL_FRect dest = {posX(x),posY(y),tileSize,tileSize};
			SDL_RenderTexture(renderer, tiles[b], &tileFRect, &dest);
			if(b.fluid > 0){
				const SDL_FRect fluidRect = {posX(x),posY(y)+tileSize*(1- (float) b.fluid),(float)tileSize,tileSize* (float) b.fluid};
				SDL_SetRenderDrawColor(renderer,0x44,0x44,0xaa,0xff);
				SDL_RenderFillRect(renderer, &fluidRect);
			}
		}
	}
	float mx, my;
	SDL_GetMouseState(&mx, &my);
	int tx = tPosX((int)mx);
	int ty = tPosY((int)my);

	const SDL_FRect cursorRect = {posX(tx), posY(ty),tileSize,tileSize};
	SDL_SetRenderDrawColor(renderer,0,0,0,0xff);
	SDL_RenderRect(renderer, &cursorRect);
	player->render();
}

template<typename T>
void write(std::ofstream& out, T* t){
	out.write((char*) t, sizeof(T));
}
template<typename T>
void read(std::ifstream& in, T* t){
	in.read((char*) t, sizeof(T));
}

bool Map::save(const std::string& file)const{
	std::ofstream out = std::ofstream(file, std::ios::binary);
	if(!out.is_open()||out.bad()){
		std::cerr << "Map::save failed to open "<< file << std::endl;
		return false;
	}
	write(out, player);
	uint32_t count = chunks.size();
	write(out, &chSize);
	write(out, &count);
	for(const auto& ch : chunks){
		write(out, ch.second);
	}
	out.close();
	std::cout << "Saved to " << file << std::endl;
	return true;
}

bool Map::load(const std::string& file){
	std::ifstream in = std::ifstream(file, std::ios::binary);
	if(!in.is_open() || in.bad()){
		std::cerr << "Map::load failed to open " << file << std::endl;
		return false;
	}
	read(in, player);
	cameraX = player->x;
	cameraY = player->y;
	int tmpChSize;
	uint32_t tmpCount;
	read(in, &tmpChSize);
	if(tmpChSize != chSize){
		std::cerr << "Map::load failed: bad chunk size in file "<< file << std::endl;
		in.close();
		return false;
	}
	read(in, &tmpCount);
	for(size_t i = 0; i < tmpCount; i++){
		Chunk* ch = new Chunk();
		read(in, ch);
		chunks[KEY(ch->x, ch->y)] = ch;
	}
	in.close();
	std::cout << "Loaded from " << file << std::endl;
	return true;
}