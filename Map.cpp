#include "Helper.h"

Map::Map(GameState& game):game(game){
	chunks = std::unordered_map<uint32_t, std::unique_ptr<Chunk>>();
	const float n1 = perlin.noise2D(20 * 0.01, 1) * cTerrainSteepness + 100;
	player = std::make_unique<Player>(game, posWorld{20.f * tileSize, floorf(tileSize * (n1 - 1))});
	game.camera.x = player->pos.x;
	game.camera.y = player->pos.y;
}

float Map::posX(int x)const{
	return tileSize * x - game.camera.x + game.wWidth / 2.f;
}
float Map::posY(int y)const{
	return tileSize * y - game.camera.y + game.wHeight / 2.f;
}
int Map::tPosX(float x)const{
	return (int) floorf((x + game.camera.x - game.wWidth / 2.f) / tileSize);
}
int Map::tPosY(float y)const{
	return (int) floorf((y + game.camera.y - game.wHeight / 2.f) / tileSize);
}

Map::Chunk* Map::chunkAt(posChunk p){
	uint32_t key = chunkHashFromPos(p.x, p.y);
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
	return chunks.contains(chunkHashFromPos(p.x, p.y));
}

Block& Map::world(posTile p){
	const posChunk pc = p;
	Chunk* ch = chunkAt(pc);
	return ch->data[(p.x - pc.x * chSize) + (p.y - pc.y * chSize) * chSize];
}

const Block& Map::world(posTile p)const{
	const posChunk pc = p;
	try{
		Chunk* ch = chunks.at(chunkHashFromPos(pc.x,pc.y)).get();
		return ch->data[(p.x - pc.x * chSize) + (p.y - pc.y * chSize) * chSize];
	} catch(const std::out_of_range&){
		return nullBlock;
	}
}

bool Map::isSolid(posTile p)const{
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
		float wy = perlin.noise2D(0, 1) * cTerrainSteepness + 100 + idx * caveDistance;
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
		lightUpdateQueue.pop();
		const posChunk pc = t.first;
		Chunk* ch = chunkAt(pc);
		ch->updateLight(t.first-pc, t.second);
	}
	std::cout << "World generation updated light " << count << " times.\n" << std::endl;
}

void Map::updateLight(posTile pos, bool genStep){
	Block& b = world(pos);
	if(!b.hasScheduledLightUpdate){
		lightUpdateQueue.push({pos, genStep});
		game.countLightUpdates = lightUpdateQueue.size();
		b.hasScheduledLightUpdate = true;
	}
}

void Map::handleMouseWheel(SDL_MouseWheelEvent event){
	const bool dir = event.integer_y < 0;
	if(dir){
		player->selectedSlot++;
		if(player->selectedSlot >= cInventorySize) player->selectedSlot = 0;
	} else{
		player->selectedSlot--;
		if(player->selectedSlot < 0)player->selectedSlot = cInventorySize - 1;
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
	for(int i = -cUpdateRadius; i <= cUpdateRadius; i++){
		for(int j = -cUpdateRadius; j <= cUpdateRadius; j++){
			Chunk* ch = chunkAt(ppc.x + i, ppc.y + j);
			ch->update();
		}
	}
	int count = 0;
	while(!lightUpdateQueue.empty() && count++ < maxlightUpdateCount){
		const std::pair<posTile, bool> f = lightUpdateQueue.front();
		lightUpdateQueue.pop();
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
	write(out, &count);
	for(const auto& ch : chunks){
		ch.second->save(out);
	}
	write(out, &chSize);
	out.close();
	std::cout << "Saved to " << file << std::endl;
	return true;
}

bool Map::load(const std::string& file){//TODO: wont work with texture caching
	std::ifstream in = std::ifstream(file, std::ios::binary);
	if(!in.is_open() || in.bad()){
		std::cerr << "Map::load failed to open " << file << std::endl;
		return false;
	}
	player.get()->load(in);
	game.camera.x = player->pos.x;
	game.camera.y = player->pos.y;
	uint32_t tmpCount;
	read(in, &tmpCount);
	for(size_t i = 0; i < tmpCount; i++){
		std::unique_ptr<Chunk> ch = std::make_unique<Chunk>(this);
		ch->load(in);
		chunks[chunkHashFromPos(ch->pos.x, ch->pos.y)] = std::move(ch);
	}
	int tmpChSize;
	read(in, &tmpChSize);
	if(tmpChSize != chSize){
		std::cerr << "Map::load failed: bad chunk size in file " << file << std::endl;
		in.close();
		return false;
	}
	in.close();
	std::cout << "Loaded from " << file << std::endl;
	return true;
}
