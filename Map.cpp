#include "Helper.h"

Map::Map(GameState& game):game(game){
	chunks = std::unordered_map<uint32_t, std::unique_ptr<Chunk>>();
	const float n1 = perlin.noise2D(20 * 0.01, 1) * TERRAIN_STEEPNESS + 100;
	player = std::make_unique<Player>(game, posWorld{20.f * tileSize, floorf(tileSize * (n1 - 1))});
	game.camera.x = player->pos.x;
	game.camera.y = player->pos.y;
}

float Map::posX(int x)const noexcept{
	return tileSize * x - game.camera.x + game.wWidth / 2.f;
}
float Map::posY(int y)const noexcept{
	return tileSize * y - game.camera.y + game.wHeight / 2.f;
}
int Map::tPosX(float x)const noexcept{
	return (int) floorf((x + game.camera.x - game.wWidth / 2.f) / tileSize);
}
int Map::tPosY(float y)const noexcept{
	return (int) floorf((y + game.camera.y - game.wHeight / 2.f) / tileSize);
}
inline constexpr uint32_t KEY(uint32_t high, uint32_t low){ return ((high << 16) | (low & 0xFFFF)); }

Map::Chunk* Map::chunkAt(posChunk p){
	uint32_t key = KEY(p.x, p.y);
	Chunk* ch = nullptr;
	try{
		ch = chunks.at(key).get();
	} catch(const std::out_of_range&){
		game.countChunkGen++;
		ch = chunks.insert({key, std::make_unique<Map::Chunk>(this, p)}).first->second.get();
	}
	assert(ch != nullptr);
	return ch;
}

bool Map::chunkExists(posChunk p)const{
	return chunks.contains(KEY(p.x, p.y));
}

Block& Map::world(posTile p){
	const posChunk pc = p;
	Chunk* ch = chunkAt(pc);
	return ch->data[(p.x - pc.x * chSize) + (p.y - pc.y * chSize) * chSize];
}

bool Map::isSolid(posTile p){
	return ::isSolid(world(p).t);
}

void Map::generateWorld(){
	std::cout << "Generating chunks" << std::endl;
	for(int i = 2; i >= -2; i--){
		for(int j = 2; j >= -2; j--){
			chunkAt(i, j);
		}
	}
	std::cout << "Digging caves" << std::endl;//Generates chunks
	for(int idx = 0; idx <= caveCount; idx++){
		float wx = 0;
		float wy = perlin.noise2D(0, 1) * TERRAIN_STEEPNESS + 100 + idx * caveDistance;
		const float seed = wy;//TODO: must change for distant caves
		int length = 0;
		float dx = 0, dy = 0;
		while(length < 1000){
			const auto carve = [this](float x, float y){
				Block& b = world((int)x, (int)y);
				b.t = Tile::AIR;
				b.lightSource = false;
			};
			for(int i = -2; i <= 2; i++){
				for(int j = -2; j <= 2; j++){
					if(!(abs(i) == 2 && abs(j) == 2)){
						carve(wx + i, wy + j);
					}
				}
			}
			dx += (idx%2)==0?1:-1;
			dy += perlin.noise2D(seed, 0 + length * 0.1f)*1.5f;
			wx += dx;
			dx -= dx > 0 ? 1 : -1;
			wy += dy;
			dy -= dy > 0 ? 1 : -1;
			length += 1;
		}
	}
	std::cout << "Processing light updates" << std::endl;

	int count = 0;
	while(!lightUpdateQueue.empty()){
		count++;
		const std::pair<posTile, bool> t = lightUpdateQueue.front();
		lightUpdateQueue.pop_front();
		const posChunk pc = t.first;
		Chunk* ch = chunkAt(pc);
		ch->updateLight(t.first-pc, t.second);
	}
	std::cout << "World generation updated light " << count << " times.\n" << std::endl;
}

Map::Chunk::Chunk(Map* map, posChunk p) :pos(p), map(map){
	std::cout << "Created chunk [" << p.x << "][" << p.y << "]:\""<< KEY(p.x,p.y) << "\"" << std::endl;
	generate();
}

void Map::Chunk::generate(){
	const int gx = pos.x * chSize;
	const int gy = pos.y * chSize;
	constexpr int height = 100;
	std::mt19937 rng((size_t)((uint64_t)map_seed * map_seed) + pos.x + pos.y * map_seed);
	const int treeHeight = (int)(perlin.noise2D((gx + chSize / 2) * 0.01f, 1) * TERRAIN_STEEPNESS) + height;
	for(int i = 0; i < chSize; i++){
		const int n1 = (int)(perlin.noise2D((gx+i) * 0.01f, 1) * TERRAIN_STEEPNESS) + height;
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
				const std::uniform_int_distribution<> dist(0, 10000);
				const bool oreG = dist(rng) > 9990;
				const bool oreC = dist(rng) > 9990;
				if(oreG){
					data[i + j * chSize] = Block(Tile::GLOW, Tile::STONE, 0);
					data[i + j * chSize].lightSource = true;
				}else if(oreC){
					data[i + j * chSize] = Block(Tile::COAL, Tile::STONE, 0);
				}else{
					data[i + j * chSize] = Block(Tile::STONE, Tile::STONE, 0);
				}
			}
		}
	}
	const short treeY = (short)floor(treeHeight / chSize);
	if(treeY == pos.y && treeHeight <= height)generateTree(chSize / 2, treeHeight - gy);
	for(int x = 0; x < chSize; x++){
		for(int y = 0; y < chSize; y++){
			updateLight(x, y, true);
		}
	}
	for(int x = chSize-1; x >= 0; x--){
		for(int y = chSize-1; y >= 0; y--){
			updateLight(x, y, true);
		}
	}
}

void Map::Chunk::generateTree(int tx, int ty){
	const int mx = pos.x * chSize;
	const int my = pos.y * chSize;
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
					: map->world(pos.x * chSize + i, pos.y * chSize + j + 1);
				if(under.t == Tile::AIR){
					const Block tmp = b;
					b = Block(under.t, b.bg, under.fluid, 0);
					under = Block(tmp.t, under.bg, tmp.fluid, tmp.light);
					map->updateLight(pos.x * chSize + i, pos.y * chSize + j, false);
				}
			}
			float& fl1 = b.fluid;
			if(fl1 > 0){
				Block& under = (j < chSize - 1)
					? data[i + (j + 1) * chSize]
					: map->world(pos.x * chSize + i, pos.y * chSize + j + 1);
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
						: map->world(pos.x * chSize + i - 1, pos.y * chSize + j);
					Block& right = (i < chSize - 1)
						? data[i + 1 + j * chSize]
						: map->world(pos.x * chSize + i + 1, pos.y * chSize + j);
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
						: map->world(pos.x * chSize + i, pos.y * chSize + j - 1);
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

void Map::updateLight(posTile pos, bool genStep){
	Block& b = world(pos);
	if(!b.hasScheduledLightUpdate){
		lightUpdateQueue.push_back({pos, genStep});
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
		: (genStep && !map->chunkExists(pos.x - 1, pos.y) ? Block() : map->world(pos.x * chSize + i - 1, pos.y * chSize + j));
	const Block r = (i < chSize - 1)
		? data[i + 1 + j * chSize]
		: (genStep && !map->chunkExists(pos.x + 1, pos.y) ? Block() : map->world(pos.x * chSize + i + 1, pos.y * chSize + j));
	const Block u = (j > 0)
		? data[i + (j - 1) * chSize]
		: (genStep && !map->chunkExists(pos.x, pos.y - 1) ? Block() : map->world(pos.x * chSize + i, pos.y * chSize + j - 1));
	const Block d = (j < chSize - 1)
		? data[i + (j + 1) * chSize]
		: (genStep && !map->chunkExists(pos.x, pos.y + 1) ? Block() : map->world(pos.x * chSize + i, pos.y * chSize + j + 1));
	if(!(genStep && j==0) && c.t==Tile::AIR){
		c.skyView = u.skyView;
	}
	char lgt = 0;
	if(c.skyView || c.lightSource){
		lgt = 127;
	}else if(!::isSolid(c.t)){//Transparent
		const char lgt1 = lfmax(d.light - lightFalloff, u.light - lightFalloff);
		const char lgt2 = lfmax(l.light - lightFalloff, r.light - lightFalloff);
		lgt = lfmax(0, lfmax(lgt1, lgt2));
	} else{//Solid
		const char lgt1 = lfmax(d.light - lightFalloff*4, u.light - lightFalloff*4);
		const char lgt2 = lfmax(l.light - lightFalloff*4, r.light - lightFalloff*4);
		lgt = lfmax(0, lfmax(lgt1, lgt2));
	}
	if(lgt!=c.light)lightChanged = true;
	c.light = lgt;

	if(!genStep && lightChanged){
		map->updateLight(pos.x * chSize + i, pos.y * chSize + j - 1);
		map->updateLight(pos.x * chSize + i, pos.y * chSize + j + 1);
		map->updateLight(pos.x * chSize + i - 1, pos.y * chSize + j);
		map->updateLight(pos.x * chSize + i + 1, pos.y * chSize + j);
		return;//skip neighbor check
	} else if(genStep && lightChanged){	
		//schedule light updates in generated neighboring chunk borders, to process light from this chunk
		if(i == 0 && map->chunkExists(pos.x - 1, pos.y)) map->updateLight(pos.x * chSize + i - 1, pos.y * chSize + j, false);
		if(i == chSize - 1 && map->chunkExists(pos.x + 1, pos.y)) map->updateLight(pos.x * chSize + i + 1, pos.y * chSize + j, false);
		if(j == 0 && map->chunkExists(pos.x, pos.y - 1)) map->updateLight(pos.x * chSize + i, pos.y * chSize + j - 1, false);
		if(j == chSize - 1 && map->chunkExists(pos.x, pos.y + 1)) map->updateLight(pos.x * chSize + i, pos.y * chSize + j + 1, false);
	}

	if(!genStep && !lightChanged){
		const char ldu = lfabs(c.light - u.light);
		if(::isSolid(u.t)){
			if(ldu > lightFalloff * 4)map->updateLight(pos.x * chSize + i, pos.y * chSize + j - 1);
		} else{
			if(ldu > lightFalloff)map->updateLight(pos.x * chSize + i, pos.y * chSize + j - 1);
		}
		const char ldd = lfabs(c.light - d.light);
		if(::isSolid(d.t)){
			if(ldd > lightFalloff * 4)map->updateLight(pos.x * chSize + i, pos.y * chSize + j + 1);
		} else{
			if(ldd > lightFalloff)map->updateLight(pos.x * chSize + i, pos.y * chSize + j + 1);
		}
		const char ldl = lfabs(c.light - l.light);
		if(::isSolid(l.t)){
			if(ldl > lightFalloff * 4)map->updateLight(pos.x * chSize + i - 1, pos.y * chSize + j);
		} else{
			if(ldl > lightFalloff)map->updateLight(pos.x * chSize + i - 1, pos.y * chSize + j);
		}
		const char ldr = lfabs(c.light - r.light);
		if(::isSolid(r.t)){
			if(ldr > lightFalloff * 4)map->updateLight(pos.x * chSize + i + 1, pos.y * chSize + j);
		} else{
			if(ldr > lightFalloff)map->updateLight(pos.x * chSize + i + 1, pos.y * chSize + j);
		}
	}
}

/*void Map::handleKeyDown(char key){
}*/

void Map::handleMouseWheel(SDL_MouseWheelEvent event) noexcept{
	const bool dir = event.integer_y < 0;
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

Block Map::destroy(posTile p, Tool tool){
	Block& b = world(p);
	if(!::isSolid(b.t))return nullBlock;
	Block c = b;
	c.t = destroyResult(c.t, tool);
	b.t = Tile::AIR;
	updateLight(p);
	return c;
}


void Map::update(){
	game.countLightUpdates = lightUpdateQueue.size();
	const posChunk ppc = player->pos;
	for(int i = -UPDATE_RADIUS; i <= UPDATE_RADIUS; i++){
		for(int j = -UPDATE_RADIUS; j <= UPDATE_RADIUS; j++){
			Chunk* ch = chunkAt(ppc.x + i, ppc.y + j);
			ch->update();
		}
	}
	int count = 0;
	while(!lightUpdateQueue.empty() && count++ < maxlightUpdateCount){
		const std::pair<posTile, bool> f = lightUpdateQueue.front();
		lightUpdateQueue.pop_front();
		const posChunk pc = f.first;
		Chunk* ch = chunkAt(pc);
		ch->updateLight(f.first-pc,f.second);
	}
	player->update();
}

void Map::render(){
	game.camera.x += (player->pos.x - game.camera.x + tileSize / 2) / 10;
	game.camera.y += (player->pos.y - game.camera.y + tileSize / 2) / 10;

	const int beginX = (int) (game.camera.x - game.wWidth / 2.f) / tileSize-1;
	const int beginY = (int) (game.camera.y - game.wHeight / 2.f) / tileSize-1;
	const int endX = (int) (game.camera.x + game.wWidth / 2.f) / tileSize+1;
	const int endY = (int) (game.camera.y + game.wHeight / 2.f) / tileSize+1;
	const int chBeginX = (int) floorf((float) beginX / chSize);
	const int chBeginY = (int) floorf((float) beginY / chSize);
	const int chEndX = (int) floorf((float) endX / chSize);
	const int chEndY = (int) floorf((float) endY / chSize);
	for(int cx = chBeginX; cx <= chEndX; cx++){
		const int bx = cx == chBeginX ? beginX - chBeginX * chSize : 0;
		const int ex = cx == chEndX ? endX - chEndX * chSize : chSize - 1;
		for(int cy = chBeginY; cy <= chEndY; cy++){
			const int by = cy == chBeginY ? beginY - chBeginY * chSize : 0;
			const int ey = cy == chEndY ? endY - chEndY * chSize : chSize - 1;
			Chunk* ch = chunkAt(cx,cy);
			for(int lx = bx; lx <= ex; lx++){
				for(int ly = by; ly <= ey; ly++){
					const int x = lx + cx * chSize;
					const int y = ly + cy * chSize;
					const Block& b = ch->data[lx + ly * chSize];
					const SDL_FRect dest = {posX(x),posY(y),tileSize,tileSize};
					if(::hasBackground(b.t) && b.bg != Tile::AIR){
						SDL_RenderTexture(game.renderer, game.tiles[(int) b.bg], &tileFRect, &dest);
						const SDL_FRect shadowRect = {posX(x),posY(y),(float) tileSize,(float) tileSize};
						SDL_SetRenderDrawColor(game.renderer, 0, 0, 0, 0x55);
						SDL_RenderFillRect(game.renderer, &shadowRect);
					} else{
						SDL_RenderTexture(game.renderer, game.tiles[(int) b.t], &tileFRect, &dest);
					}
					if(b.fluid > 0){
						const SDL_FRect fluidRect = {posX(x),posY(y) + tileSize * (1 - fminf(b.fluid,1)),(float) tileSize,tileSize * fminf(b.fluid,1)};
						SDL_SetRenderDrawColor(game.renderer, 0x44, 0x44, 0xaa, 0xff);
						SDL_RenderFillRect(game.renderer, &fluidRect);
						if(game.overlayFluid){
							char string[4];//TODO: drawing text is slow and can be cached
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
		}
	}

	float mx, my;
	SDL_GetMouseState(&mx, &my);
	const int tx = tPosX(mx);
	const int ty = tPosY(my);

	const SDL_FRect cursorRect = {posX(tx), posY(ty),tileSize,tileSize};
	SDL_SetRenderDrawColor(game.renderer,0,0,0,0xff);
	SDL_RenderRect(game.renderer, &cursorRect);
	if(game.debugMode){
		const Chunk* ch = chunkAt(player->pos);
		const SDL_FRect chunkRect = {posX(ch->pos.x*chSize), posY(ch->pos.y*chSize),chSize*tileSize,chSize*tileSize};
		SDL_SetRenderDrawColor(game.renderer, 0, 0, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &chunkRect);
		const Chunk* ch2 = chunkAt(chBeginX, chBeginY);
		const SDL_FRect chunkRect2 = {posX(ch2->pos.x * chSize), posY(ch2->pos.y * chSize),chSize * tileSize,chSize * tileSize};
		SDL_SetRenderDrawColor(game.renderer, 0, 0xff, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &chunkRect2);
		const Chunk* ch3 = chunkAt(chEndX, chEndY);
		const SDL_FRect chunkRect3 = {posX(ch3->pos.x * chSize), posY(ch3->pos.y * chSize),chSize * tileSize,chSize * tileSize};
		SDL_SetRenderDrawColor(game.renderer, 0xff, 0, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &chunkRect3);
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
	const uint32_t count = chunks.size();
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
	game.camera.x = player->pos.x;
	game.camera.y = player->pos.y;
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
		std::unique_ptr<Chunk> ch = std::make_unique<Chunk>(this);
		ch->load(in);
		chunks[KEY(ch->pos.x, ch->pos.y)] = std::move(ch);
	}
	in.close();
	std::cout << "Loaded from " << file << std::endl;
	return true;
}

void Map::Chunk::save(std::ofstream& out) const{
	write(out, &pos.x);
	write(out, &pos.y);
	write(out, &data);
}
void Map::Chunk::load(std::ifstream& in){
	read(in, &pos.x);
	read(in, &pos.y);
	read(in, &data);
}