#include "Helper.h"

Map::Map(GameState& game):game(game){
	chunks = std::unordered_map<uint32_t, Chunk*>();
	const double n1 = perlin.noise2D(20 * 0.01, 1) * TERRAIN_STEEPNESS + 100;
	player = std::make_unique<Player>(game, 20.f * tileSize, (float)tileSize*(((int)n1)-1));
	game.cameraX = player->x;
	game.cameraY = player->y;
}

Map::~Map(){
	for(auto& ch : chunks){
		if(ch.second!=nullptr)
		delete ch.second;
	}
}

inline float Map::posX(int x)const{
	return tileSize * x - game.cameraX + game.wWidth / 2.f;
}
inline float Map::posY(int y)const{
	return tileSize * y - game.cameraY + game.wHeight / 2.f;
}
int Map::tPosX(int x)const{
	return (int) floorf((x + game.cameraX - game.wWidth / 2.f) / tileSize);
}
int Map::tPosY(int y)const{
	return (int) floorf((y + game.cameraY - game.wHeight / 2.f) / tileSize);
}
#define KEY(high,low)(((uint32_t)(high) << 16) | ((uint32_t) (low))&0xFFFF)

inline Map::Chunk* Map::chunkAt(int x, int y){
	short cx = (short) floorf(x / (float) chSize);
	short cy = (short) floorf(y / (float) chSize);
	uint32_t key = KEY(cx, cy);
	Chunk* ch;
	try{
		ch = chunks.at(key);
	} catch(const std::out_of_range&){
		ch = new Chunk(this, cx, cy);
		ch->generate();
		chunks.insert({key, ch});
		game.countChunkGen++;
	}
	return ch;
}

inline bool Map::chunkExists(int x, int y)const{
	uint32_t key = KEY(x, y);
	return chunks.contains(key);
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
	std::cout << "Generating chunks" << std::endl;
	for(int i = -2; i <= 2; i++){
		for(int j = -2; j <= 2; j++){
			Chunk* ch = new Chunk(this, i, j);
			ch->generate();
			chunks.insert({KEY(i,j), ch});
		}
	}
	std::cout << "Digging caves" << std::endl;
	for(int idx = 0; idx <= caveCount; idx++){
		double wx = 0;
		double wy = (int) (perlin.noise2D(0, 1) * TERRAIN_STEEPNESS) + 100 + idx * caveDistance;
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
	/*std::cout << "Shining light" << std::endl;
	for(int i = -2; i <= 2; i++){
		for(int j = -2; j <= 2; j++){
			Chunk* ch = chunkAt(i*chSize, j*chSize);
			for(int x = chSize - 1; x >= 0; x--){
				for(int y = chSize - 1; y >= 0; y--){
					ch->updateLight(x, y, false);
				}
			}
			for(int x = 0; x < chSize; x++){
				for(int y = 0; y < chSize; y++){
					ch->updateLight(x, y, false);
				}
			}
		}
	}*/
	std::cout << "Processing light updates" << std::endl;

	int count = 0;
	while(!lightUpdateQueue.empty()){
		count++;
		const auto [x, y, g] = lightUpdateQueue.front();
		lightUpdateQueue.pop_front();
		Chunk* ch = chunkAt(x, y);
		short cx = (short) floorf((float) x / chSize);
		short cy = (short) floorf((float) y / chSize);
		ch->updateLight(x - cx * chSize, y - cy * chSize, g);
	}
	std::cout << "World generation updated light " << count << " times.\n" << std::endl;
}

Map::Chunk::Chunk(Map* map, short x, short y) :x(x), y(y), map(map){
	std::cout << "Created chunk [" << x << "][" << y << "]:\""<< KEY(x,y) << "\"" << std::endl;
	for(size_t i = 0; i < chSize*chSize; i++){
		data[i] = Block();//No init, leave Tile::UNKNOWN
	}
}

void Map::Chunk::generate(){
	int gx = x * chSize;
	int gy = y * chSize;
	constexpr int height = 100;
	std::mt19937 rng(seed * seed + x + y * seed);
	int treeHeight = (int) (perlin.noise2D((gx + chSize / 2) * 0.01, 1) * TERRAIN_STEEPNESS) + height;
	for(int i = 0; i < chSize; i++){
		const int n1 = (int)(perlin.noise2D((gx+i) * 0.01, 1) * TERRAIN_STEEPNESS) + height;
		for(int j = 0; j < chSize; j++){
			if(j + gy < n1){
				Block b = Block(Tile::AIR,Tile::AIR, 0, 127);
				b.skyView = true;
				if(gy+j> height)b.fluid = 2;
				data[i + j * chSize] = b;
			}
			else if(j + gy == n1){
				if(gy + j < height)
					data[i + j * chSize] = Block(Tile::GRASS,Tile::DIRT, 0);
				else
					data[i + j * chSize] = Block(Tile::SAND,Tile::AIR, 0);
			}
			else if(j+gy<n1+dirtHeight)data[i + j * chSize] = Block(Tile::DIRT,Tile::DIRT, 0);
			else{
				std::uniform_int_distribution<> dist(0, 10000);
				bool ore = dist(rng) > 9990;
				if(ore){
					data[i + j * chSize] = Block(Tile::GLOW, Tile::STONE, 0);
					data[i + j * chSize].lightSource = true;
				} else{
					data[i + j * chSize] = Block(Tile::STONE, Tile::STONE, 0);
				}
			}
		}
	}
	short treeY = (short) floorf(treeHeight / (float) chSize);
	if(treeY == y && treeHeight <= height)generateTree(chSize / 2, treeHeight - gy);
	for(int x = 0; x < chSize; x++){
		for(int y = 0; y < chSize; y++){
			updateLight(x, y, true);
		}
	}
}

void Map::Chunk::generateTree(int tx, int ty){
	int mx = x * chSize;
	int my = y * chSize;
	for(int i = 0; i < 6; i++){
		if(ty - i >= 0){
			data[tx + (ty - i) * chSize] = Block(Tile::WOOD, Tile::AIR, 0);
		} else{
			map->world(mx + tx, my + ty - i) = Block(Tile::WOOD, Tile::AIR, 0);
		}
	}
	for(int j = 1; j <= 3; j++){
		for(int i = 2+j; i < 9-j; i++){
			if(ty - i >= 0 && tx - j >= 0 && j < chSize){
				data[-j + tx + (ty - i) * chSize] = Block(Tile::LEAVES, Tile::AIR, 0);
				data[j + tx + (ty - i) * chSize] = Block(Tile::LEAVES, Tile::AIR, 0);
			} else{
				map->world(mx - j + tx, my + ty - i) = Block(Tile::LEAVES, Tile::AIR, 0);
				map->world(mx + j + tx, my + ty - i) = Block(Tile::LEAVES, Tile::AIR, 0);
			}
		}
	}
	if(ty >= 7){
		data[tx + (ty - 7) * chSize] = Block(Tile::LEAVES, Tile::AIR, 0);
	} else{
		map->world(mx + tx, my + ty - 7) = Block(Tile::LEAVES, Tile::AIR, 0);
	}
	if(ty >= 6){
		data[tx + (ty - 6) * chSize] = Block(Tile::LEAVES, Tile::AIR, 0);
	} else{
		map->world(mx + tx, my + ty - 6) = Block(Tile::LEAVES, Tile::AIR, 0);
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
					b = Block(under.t, b.bg, under.fluid, 0);
					under = Block(tmp.t, under.bg, tmp.fluid, tmp.light);
					map->updateLight(x * chSize + i, y * chSize + j, false);
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
					if(fl <= 1){//Fill tile below
						flu = fl;
						fl1 = 0;
					} else if(fl > 1 && fl <= 2.25f){//Second tile pressure propagation
						float k = (fl1 + (flu - 1) * 4) / 2;
						flu = 1 + k * 0.25f;
						fl1 = fl - 1 - k * 0.25f;
					} else if(fl > 2.25){//Deep tile pressure propagation
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

void Map::updateLight(int x, int y, bool genStep){
	std::tuple<int,int,bool> p = {x,y, genStep};
	Block& b = world(x, y);
	if(!b.hasScheduledLightUpdate){
		lightUpdateQueue.push_back(p);
		game.countLightUpdates = lightUpdateQueue.size();
		b.hasScheduledLightUpdate = true;
	}
}

void Map::Chunk::updateLight(int i, int j, bool genStep){
	if(i < 0 || j < 0 || i >= chSize || j >= chSize){
		std::cerr << "Bad position in Map::Chunk::updateLight()!!!" << std::endl;
		return;
	}
	bool lightChanged = false;
	Block& c = data[i + j * chSize];
	c.hasScheduledLightUpdate = false;
	const Block l = (i > 0)
		? data[i - 1 + j * chSize]
		: (genStep && !map->chunkExists(x - 1, y) ? Block() : map->world(x * chSize + i - 1, y * chSize + j));
	const Block r = (i < chSize - 1)
		? data[i + 1 + j * chSize]
		: (genStep && !map->chunkExists(x + 1, y) ? Block() : map->world(x * chSize + i + 1, y * chSize + j));
	const Block u = (j > 0)
		? data[i + (j - 1) * chSize]
		: (genStep && !map->chunkExists(x, y - 1) ? Block() : map->world(x * chSize + i, y * chSize + j - 1));
	const Block d = (j < chSize - 1)
		? data[i + (j + 1) * chSize]
		: (genStep && !map->chunkExists(x, y + 1) ? Block() : map->world(x * chSize + i, y * chSize + j + 1));
	if(!(genStep && j==0) && c.t==Tile::AIR){
		c.skyView = u.skyView;
	}
	char lgt = 0;
	if(c.skyView || c.lightSource){
		lgt = 127;
	}else if(!::isSolid(c.t)){//Transparent
		char lgt1 = (char) fmax(d.light - lightFalloff, u.light - lightFalloff);
		char lgt2 = (char) fmax(l.light - lightFalloff, r.light - lightFalloff);
		lgt = (char) fmax(0, fmax(lgt1, lgt2));
	} else{//Solid
		char lgt1 = (char) fmax(d.light - lightFalloff*4, u.light - lightFalloff*4);
		char lgt2 = (char) fmax(l.light - lightFalloff*4, r.light - lightFalloff*4);
		lgt = (char) fmax(0, fmax(lgt1, lgt2));
	}
	if(lgt!=c.light)lightChanged = true;
	c.light = lgt;

	if(!genStep && lightChanged){
		map->updateLight(x * chSize + i, y * chSize + j - 1);
		map->updateLight(x * chSize + i, y * chSize + j + 1);
		map->updateLight(x * chSize + i - 1, y * chSize + j);
		map->updateLight(x * chSize + i + 1, y * chSize + j);
		return;//skip neighbor check
	} else if(genStep && lightChanged){	
		//schedule light updates in generated neighboring chunk borders, to process light from this chunk
		if(i == 0 && map->chunkExists(x - 1, y)) map->updateLight(x * chSize + i - 1, y * chSize + j, false);
		if(i == chSize - 1 && map->chunkExists(x + 1, y)) map->updateLight(x * chSize + i + 1, y * chSize + j, false);
		if(j == 0 && map->chunkExists(x, y - 1)) map->updateLight(x * chSize + i, y * chSize + j - 1, false);
		if(j == chSize - 1 && map->chunkExists(x, y + 1)) map->updateLight(x * chSize + i, y * chSize + j + 1, false);
	}

	if(!genStep && !lightChanged){
		char ldu = (char) fabs(c.light - u.light);
		if(::isSolid(u.t)){
			if(ldu > lightFalloff * 4)map->updateLight(x * chSize + i, y * chSize + j - 1);
		} else{
			if(ldu > lightFalloff)map->updateLight(x * chSize + i, y * chSize + j - 1);
		}
		char ldd = (char) fabs(c.light - d.light);
		if(::isSolid(d.t)){
			if(ldd > lightFalloff * 4)map->updateLight(x * chSize + i, y * chSize + j + 1);
		} else{
			if(ldd > lightFalloff)map->updateLight(x * chSize + i, y * chSize + j + 1);
		}
		char ldl = (char) fabs(c.light - l.light);
		if(::isSolid(l.t)){
			if(ldl > lightFalloff * 4)map->updateLight(x * chSize + i - 1, y * chSize + j);
		} else{
			if(ldl > lightFalloff)map->updateLight(x * chSize + i - 1, y * chSize + j);
		}
		char ldr = (char) fabs(c.light - r.light);
		if(::isSolid(r.t)){
			if(ldr > lightFalloff * 4)map->updateLight(x * chSize + i + 1, y * chSize + j);
		} else{
			if(ldr > lightFalloff)map->updateLight(x * chSize + i + 1, y * chSize + j);
		}
	}
}

/*void Map::handleKeyDown(char key){
}*/

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
	if(b.fluid > 0){//TODO:fluid can be destroyed here
		Block& up = world(x, y - 1);
		if(!::isSolid(up.t)){
			up.fluid = fminf(b.fluid+up.fluid,1);
		}
	}
	b.t = t;
	b.skyView = false;
	b.fluid = 0;
	return true;
}

Block Map::destroy(int x, int y){
	Block& b = world(x, y);
	if(!::isSolid(b.t))return nullBlock;
	Block c = b;
	c.t = destroyResult(c.t);
	b.t = Tile::AIR;
	updateLight(x, y);
	return c;
}


void Map::update(){
	game.countLightUpdates = lightUpdateQueue.size();
	short px = (short) floorf(player->x / tileSize);
	short py = (short) floorf(player->y / tileSize);
	for(int i = -UPDATE_RADIUS; i <= UPDATE_RADIUS; i++){
		for(int j = -UPDATE_RADIUS; j <= UPDATE_RADIUS; j++){
			Chunk* ch = chunkAt(px+i*chSize,py+j*chSize);
			ch->update();
		}
	}
	int count = 0;
	while(!lightUpdateQueue.empty() && count++ < maxlightUpdateCount){
		const auto [x,y,g] = lightUpdateQueue.front();
		lightUpdateQueue.pop_front();
		Chunk* ch = chunkAt(x,y);
		short cx = (short) floorf((float) x / chSize);
		short cy = (short) floorf((float) y / chSize);
		ch->updateLight(x-cx*chSize,y-cy*chSize,g);
	}
	player->update();
}

void Map::render(){
	game.cameraX += (player->x - game.cameraX + tileSize / 2) / 10;
	game.cameraY += (player->y - game.cameraY + tileSize / 2) / 10;

	int beginX = (int) (game.cameraX - game.wWidth / 2) / tileSize-1;
	int beginY = (int) (game.cameraY - game.wHeight / 2) / tileSize-1;
	int endX = (int) (game.cameraX + game.wWidth / 2) / tileSize+1;
	int endY = (int) (game.cameraY + game.wHeight / 2) / tileSize+1;
	for(int x = beginX; x < endX; x++){
		for(int y = beginY; y < endY; y++){
			//TODO: looks up chunks every iteration
			// currently rendering abour one chunk of tiles(2500), spread into max 6 chunks
			// can't just render all visible chunks, visibility must be handled at tile resolution

			Block& b = world(x, y);
			const SDL_FRect dest = {posX(x),posY(y),tileSize,tileSize};
			if(::hasBackground(b.t) && b.bg!=Tile::AIR){
				SDL_RenderTexture(game.renderer, game.tiles[b.bg], &tileFRect, &dest);
				const SDL_FRect shadowRect = {posX(x),posY(y),(float) tileSize,(float) tileSize};
				SDL_SetRenderDrawColor(game.renderer, 0, 0, 0, 0x55);
				SDL_RenderFillRect(game.renderer, &shadowRect);
			} else{
				SDL_RenderTexture(game.renderer, game.tiles[b.t], &tileFRect, &dest);
			}
			if(b.fluid > 0){
				const SDL_FRect fluidRect = {posX(x),posY(y) + tileSize * (1 - fminf(b.fluid,1)),(float) tileSize,tileSize * fminf(b.fluid,1)};
				SDL_SetRenderDrawColor(game.renderer,0x44,0x44,0xaa,0xff);
				SDL_RenderFillRect(game.renderer, &fluidRect);
				if(game.overlayFluid){
					char string[4];
					SDL_snprintf(string, sizeof(string), "%.1f", b.fluid);
					TTF_SetTextString(game.text, string, sizeof(string));
					TTF_DrawRendererText(game.text, dest.x, dest.y + tileSize / 2);
				}
			}
			if(game.overlayLight){
				char string[4];
				SDL_snprintf(string, sizeof(string), "%d", b.light);
				TTF_SetTextString(game.text, string, sizeof(string));
				TTF_DrawRendererText(game.text, dest.x, dest.y + tileSize / 2);
			}
			//if(b.light != 127){
				const SDL_FRect shadowRect = {posX(x), posY(y), (float) tileSize, (float) tileSize};
				SDL_SetRenderDrawColor(game.renderer, 0, 0, 0, (127 - b.light) * 2);
				SDL_RenderFillRect(game.renderer, &shadowRect);
			//}
		}
	}
	float mx, my;
	SDL_GetMouseState(&mx, &my);
	int tx = tPosX((int)mx);
	int ty = tPosY((int)my);

	const SDL_FRect cursorRect = {posX(tx), posY(ty),tileSize,tileSize};
	SDL_SetRenderDrawColor(game.renderer,0,0,0,0xff);
	SDL_RenderRect(game.renderer, &cursorRect);
	if(game.debugMode){
		Chunk* ch = chunkAt((int)player->x / tileSize, (int)player->y / tileSize);
		const SDL_FRect chunkRect = {posX(ch->x*chSize), posY(ch->y*chSize),chSize*tileSize,chSize*tileSize};
		SDL_SetRenderDrawColor(game.renderer, 0, 0, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &chunkRect);
	}
	player->render();
}

bool Map::save(const std::string& file)const{
	std::ofstream out = std::ofstream(file, std::ios::binary);
	if(!out.is_open()||out.bad()){
		std::cerr << "Map::save failed to open "<< file << std::endl;
		return false;
	}
	player.get()->save(out);
	uint32_t count = chunks.size();
	write(out, &chSize);
	write(out, &count);
	for(const auto& ch : chunks){
		ch.second->save(out);
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
	player.get()->load(in);
	game.cameraX = player->x;
	game.cameraY = player->y;
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
		Chunk* ch = new Chunk(this);
		ch->load(in);
		chunks[KEY(ch->x, ch->y)] = ch;
	}
	in.close();
	std::cout << "Loaded from " << file << std::endl;
	return true;
}

void Map::Chunk::save(std::ofstream& out) const{
	write(out, &x);
	write(out, &y);
	write(out, &data);
}
void Map::Chunk::load(std::ifstream& in){
	read(in, &x);
	read(in, &y);
	read(in, &data);
}