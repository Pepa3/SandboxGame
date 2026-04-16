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
#define KEY(high,low)(((uint32_t)(high) << 16) | ((uint32_t) (low))&0xFFFF)

Map::Chunk* Map::chunkAt(int x, int y){
	short cx = (short) floorf(x / (float) chSize);
	short cy = (short) floorf(y / (float) chSize);
	uint32_t key = KEY(cx, cy);
	Chunk* ch;
	try{
		ch = chunks.at(key);
	} catch(const std::out_of_range& _){
		ch = new Chunk(cx, cy);
		ch->generate();
		chunks.insert({key, ch});
		(void) _;
	}
	return ch;
}

inline Block& Map::world(int x, int y){
	short cx = (short) floorf(x / (float) chSize);
	short cy = (short) floorf(y / (float) chSize);
	Chunk* ch = chunkAt(x, y);
	return ch->data[(x - cx * chSize) + (y - cy * chSize) * chSize];
}

bool Map::isSolid(int x, int y){
	return ::isSolid(world(x,y).t);
}

void Map::generateWorld(){
	for(int i = -2; i <= 2; i++){
		for(int j = -2; j <= 2; j++){
			Chunk* ch = new Chunk(i,j);
			ch->generate();
			chunks.insert({KEY(i,j), ch});
		}
	}
	for(int idx = 0; idx <= caveCount; idx++){
		double wx = 0;
		double wy = (int) (perlin.noise2D(0, 1) * TERRAIN_STEEPNESS) + 100+idx*40;
		double seed = wy;
		int length = 0;
		double dx = 0, dy = 0;
		while(length < 1000){
			world((int) wx, (int) wy).t = Tile::AIR;
			world((int) wx, (int) wy - 1).t = Tile::AIR;
			world((int) wx, (int) wy + 1).t = Tile::AIR;
			world((int) wx + 1, (int) wy - 1).t = Tile::AIR;
			world((int) wx + 1, (int) wy + 1).t = Tile::AIR;
			world((int) wx + 1, (int) wy).t = Tile::AIR;
			world((int) wx - 1, (int) wy - 1).t = Tile::AIR;
			world((int) wx - 1, (int) wy + 1).t = Tile::AIR;
			world((int) wx - 1, (int) wy).t = Tile::AIR;
			world((int) wx - 2, (int) wy).t = Tile::AIR;
			world((int) wx + 2, (int) wy).t = Tile::AIR;
			world((int) wx, (int) wy + 2).t = Tile::AIR;
			world((int) wx, (int) wy - 2).t = Tile::AIR;
			world((int) wx - 2, (int) wy+1).t = Tile::AIR;
			world((int) wx + 2, (int) wy+1).t = Tile::AIR;
			world((int) wx+1, (int) wy + 2).t = Tile::AIR;
			world((int) wx+1, (int) wy - 2).t = Tile::AIR;
			world((int) wx - 2, (int) wy-1).t = Tile::AIR;
			world((int) wx + 2, (int) wy-1).t = Tile::AIR;
			world((int) wx-1, (int) wy + 2).t = Tile::AIR;
			world((int) wx-1, (int) wy - 2).t = Tile::AIR;
			dx += (idx%2)==0?1:-1;
			dy += perlin.noise2D(seed, 0 + length * 0.1)*1.5;
			wx += dx;
			dx -= dx > 0 ? 1 : -1;
			wy += dy;
			dy -= dy > 0 ? 1 : -1;
			length += 1;
		}
	}
	for(int i = -3; i <= 3; i++){
		for(int j = -3; j <= 3; j++){
			Chunk* ch = chunkAt(i*chSize, j*chSize);
			for(int x = 0; x < chSize; x++){
				for(int y = 0; y < chSize; y++){
					ch->updateLight(x, y, true);
				}
			}
		}
	}
}

Map::Chunk::Chunk(short x, short y) :x(x), y(y) {
	std::cout << "Created chunk [" << x << "][" << y << "]:\""<< KEY(x,y) << "\"" << std::endl;
	for(size_t i = 0; i < chSize*chSize; i++){
		data[i] = Block();//No init, leave Tile::UNKNOWN
	}
}

void Map::Chunk::generate(){
	int gx = x * chSize;
	int gy = y * chSize;
	constexpr int height = 100;
	int treeHeight = (int) (perlin.noise2D((gx + chSize / 2) * 0.01, 1) * TERRAIN_STEEPNESS) + height;
	for(int i = 0; i < chSize; i++){
		const int n1 = (int)(perlin.noise2D((gx+i) * 0.01, 1) * TERRAIN_STEEPNESS) + height;
		for(int j = 0; j < chSize; j++){
			if(j + gy < n1){
				Block b = Block(Tile::AIR,Tile::AIR, 0, 127);
				b.skyView = true;
				if(gy+j> height)b.fluid = 1;
				data[i + j * chSize] = b;
			}
			else if(j + gy == n1){
				if(gy + j < height)
					data[i + j * chSize] = Block(Tile::GRASS,Tile::DIRT, 0);
				else
					data[i + j * chSize] = Block(Tile::SAND,Tile::AIR, 0);
			}
			else if(j+gy<n1+dirtHeight)data[i + j * chSize] = Block(Tile::DIRT,Tile::DIRT, 0);
			else data[i + j * chSize] = Block(Tile::STONE, Tile::STONE, 0);
		}
	}
	short treeY = (short) floorf(treeHeight / (float) chSize);
	if(treeY==y&&treeHeight<= height)generateTree(chSize/2, treeHeight-gy);
}

void Map::Chunk::generateTree(int tx, int ty){
	int mx = x * chSize;
	int my = y * chSize;
	for(int i = 0; i < 6; i++){
		if(ty - i >= 0){
			data[tx + (ty - i) * chSize].t = Tile::WOOD;
		} else{
			map->world(mx + tx, my + ty - i).t = Tile::WOOD;
		}
	}
	for(int j = 1; j <= 3; j++){
		for(int i = 2+j; i < 9-j; i++){
			if(ty - i >= 0){
				data[-j + tx + (ty - i) * chSize] = Block(Tile::LEAVES, Tile::AIR);
				data[j + tx + (ty - i) * chSize] = Block(Tile::LEAVES, Tile::AIR);
			} else{
				map->world(mx - j + tx, my + ty - i) = Block(Tile::LEAVES, Tile::AIR);
				map->world(mx + j + tx, my + ty - i) = Block(Tile::LEAVES, Tile::AIR);
			}
		}
	}
	if(ty >= 7){
		data[tx + (ty - 7) * chSize] = Block(Tile::LEAVES, Tile::AIR);
	} else{
		map->world(mx + tx, my + ty - 7) = Block(Tile::LEAVES, Tile::AIR);
	}
	if(ty >= 6){
		data[tx + (ty - 6) * chSize] = Block(Tile::LEAVES, Tile::AIR);
	} else{
		map->world(mx + tx, my + ty - 6) = Block(Tile::LEAVES, Tile::AIR);
	}
}

void Map::Chunk::update(){
	for(size_t i = 0; i < chSize; i++){
		for(size_t j = 0; j < chSize; j++){
			Block& b = data[i + j * chSize];
			if(b.t == Tile::SAND){
				Block& under = (j < chSize - 1)
					? data[i + (j + 1) * chSize]
					: map->world(x * chSize + i, y * chSize + j + 1);
				if(under.t == Tile::AIR){
					Block tmp = b;
					b = Block(under.t, b.bg, under.fluid, b.light);
					under = Block(tmp.t, under.bg, tmp.fluid, under.light);
				}
			}
			float& fl1 = b.fluid;
			if(fl1 > 0){
				Block& under = (j < chSize - 1)
					? data[i + (j + 1) * chSize]
					: map->world(x * chSize + i, y * chSize + j + 1);
				float& flu = under.fluid;
				if(!::isSolid(under.t)){
					float fl = fl1 + flu;
					if(fl <= 1){
						flu = fl;
						fl1 = 0;
					} else if(fl > 1 && fl <= 2.25f){
						float k = (fl1 + (flu - 1) * 4) / 2;
						flu = 1 + k * 0.25f;
						fl1 = fl - 1 - k * 0.25f;
					} else if(fl > 2.25){
						fl1 = (fl - 0.25f) / 2;
						flu = (fl + 0.25f) / 2;
					}
				}
				if(fl1 > 0){
					//Spill horizontally
					Block& left = (i > 0)
						? data[i - 1 + j * chSize]
						: map->world(x * chSize + i - 1, y * chSize + j);
					Block& right = (i < chSize - 1)
						? data[i + 1 + j * chSize]
						: map->world(x * chSize + i + 1, y * chSize + j);
					float fldl = fl1 - left.fluid;
					if(!::isSolid(left.t) && fldl > 0){
						fl1 -= fldl / 2;
						left.fluid += fldl / 2;
					}
					float fldr = fl1 - right.fluid;
					if(!::isSolid(right.t) && fldr > 0){
						fl1 -= fldr / 2;
						right.fluid += fldr / 2;
					}
				}
				if(fl1 > 1){
					//Propagate up if high pressure
					Block& up = (j > 0)
						? data[i + (j - 1) * chSize]
						: map->world(x * chSize + i, y * chSize + j - 1);
					if(!::isSolid(up.t) && up.fluid == 0){
						float fld = fl1 - 1;
						fl1 -= fld / 2;
						up.fluid += fld / 2;
					}
				}
			}
		}
	}
}

void Map::updateLight(int x, int y){
	short cx = (short) floorf(x / (float) chSize);
	short cy = (short) floorf(y / (float) chSize);
	Chunk* ch = chunkAt(x, y);
	ch->updateLight(x - cx * chSize, y - cy * chSize, false);
}

void Map::Chunk::updateLight(int i, int j, bool genStep){
	Block& c = data[i + j * chSize];
	if(c.lightSource){
		c.light = 127;
		return;
	}
	const char ll = (i > 0)
		? data[i - 1 + j * chSize].light
		: (genStep ? 0 : map->world(x * chSize + i - 1, y * chSize + j).light);
	const char rl = (i < chSize - 1)
		? data[i + 1 + j * chSize].light
		: (genStep ? 0 : map->world(x * chSize + i + 1, y * chSize + j).light);
	const Block u = (j > 0)
		? data[i + (j - 1) * chSize]
		: (genStep ? Block() : map->world(x * chSize + i, y * chSize + j - 1));
	const char dl = (j < chSize - 1)
		? data[i + (j + 1) * chSize].light
		: (genStep ? 0 : map->world(x * chSize + i, y * chSize + j + 1).light);
	if(!(genStep && j==0) && c.t==Tile::AIR){
		c.skyView = u.skyView;
	}
	if(c.skyView){
		c.light = 127;
		return;
	}
	if(!::isSolid(c.t)){//Transparent
		char lgt1 = (char) fmax(dl - lightFalloff, u.light - lightFalloff);
		char lgt2 = (char) fmax(ll - lightFalloff, rl - lightFalloff);
		char lgt = (char) fmax(lgt1, lgt2);
		c.light = (char) fmax(0, lgt);
	} else{//Solid
		char lgt1 = (char) fmax(dl - lightFalloff*4, u.light - lightFalloff*4);
		char lgt2 = (char) fmax(ll - lightFalloff*4, rl - lightFalloff*4);
		char lgt = (char) fmax(lgt1, lgt2);
		c.light = (char) fmax(0, lgt);
	}
	char ldu = (char)fabs(c.light - u.light);
	//TODO: cascading updates
}

void Map::handleKeyDown(char key){
}

void Map::handleMouseWheel(SDL_MouseWheelEvent event){
	bool dir = event.integer_y < 0;
	if(dir){
		player->selectedSlot++;
		if(player->selectedSlot >= INVENTORY_SIZE) player->selectedSlot = 0;
	} else{
		player->selectedSlot--;
		if(player->selectedSlot < 0)player->selectedSlot = INVENTORY_SIZE - 1;
	}
}

bool Map::place(int x, int y, Tile t){
	Block& b = world(x, y);
	if(::isSolid(b.t)){
		return false;
	}
	if(b.fluid > 0){//TODO:fluid is destroyed here
		Block& up = world(x, y - 1);
		if(!::isSolid(up.t)){
			up.fluid = fminf(b.fluid+up.fluid,1);
		}
	}
	b.t = t;
	b.fluid = 0;
	return true;
}

Block& Map::destroy(int x, int y){//TODO: replace with a better system
	Block& b = world(x, y);
	if(!::isSolid(b.t))return nullBlock;
	if(b.t == Tile::GRASS)b.t = Tile::DIRT;
	return b;
}


void Map::update(){
	short cx = (short) floorf(player->x / chSize / tileSize);
	short cy = (short) floorf(player->y / chSize / tileSize);
	for(int i = -UPDATE_RADIUS; i <= UPDATE_RADIUS; i++){
		for(int j = -UPDATE_RADIUS; j <= UPDATE_RADIUS; j++){
			uint32_t key = KEY(cx+i, cy+j);
			Chunk* ch;
			try{
				ch = chunks.at(key);
			} catch(const std::out_of_range& _){
				ch = new Chunk(cx+i, cy+j);
				ch->generate();
				chunks.insert({key, ch});
				(void) _;
			}
			ch->update();
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
			if(::hasBackground(b.t) && b.bg!=Tile::AIR){
				SDL_RenderTexture(renderer, tiles[b.bg], &tileFRect, &dest);
				const SDL_FRect shadowRect = {posX(x),posY(y),(float) tileSize,(float) tileSize};
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0x55);
				SDL_RenderFillRect(renderer, &shadowRect);
			} else{
				SDL_RenderTexture(renderer, tiles[b.t], &tileFRect, &dest);
			}
			if(b.fluid > 0){
				const SDL_FRect fluidRect = {posX(x),posY(y) + tileSize * (1 - fminf(b.fluid,1)),(float) tileSize,tileSize * fminf(b.fluid,1)};
				SDL_SetRenderDrawColor(renderer,0x44,0x44,0xaa,0xff);
				SDL_RenderFillRect(renderer, &fluidRect);
				if(overlayFluid){
					char string[4];
					SDL_snprintf(string, sizeof(string), "%.1f", b.fluid);
					TTF_SetTextString(text, string, sizeof(string));
					TTF_DrawRendererText(text, dest.x, dest.y + tileSize / 2);
				}
			}
			if(overlayLight){
				char string[4];
				SDL_snprintf(string, sizeof(string), "%d", b.light);
				TTF_SetTextString(text, string, sizeof(string));
				TTF_DrawRendererText(text, dest.x, dest.y + tileSize / 2);
			}
			const SDL_FRect shadowRect = {posX(x),posY(y),(float) tileSize,(float) tileSize};
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, (127-b.light)*2);
			SDL_RenderFillRect(renderer, &shadowRect);
		}
	}
	float mx, my;
	SDL_GetMouseState(&mx, &my);
	int tx = tPosX((int)mx);
	int ty = tPosY((int)my);

	const SDL_FRect cursorRect = {posX(tx), posY(ty),tileSize,tileSize};
	SDL_SetRenderDrawColor(renderer,0,0,0,0xff);
	SDL_RenderRect(renderer, &cursorRect);
	if(debugMode){
		Chunk* ch = chunkAt(player->x / tileSize, player->y / tileSize);
		const SDL_FRect chunkRect = {posX(ch->x*chSize), posY(ch->y*chSize),chSize*tileSize,chSize*tileSize};
		SDL_SetRenderDrawColor(renderer, 0, 0, 0xff, 0xff);
		SDL_RenderRect(renderer, &chunkRect);
	}
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