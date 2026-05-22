#include "Helper.h"

Map::Map(GameState& game):game(game){
	const float n1 = perlin.noise2D(20 * 0.01, 1) * cTerrainSteepness + 100;
	game.player = std::make_unique<Player>(game, posWorld{20.f * tileSize, floorf(tileSize * (n1 - 1))});
	game.camera.x = game.player->pos.x;
	game.camera.y = game.player->pos.y;
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
	Chunk* ch = nullptr;
	auto entry = chunks.find(p);
	if(entry != chunks.end()){
		ch = entry->second.get();
	} else{
		game.countChunkGen++;
		ch = chunks.insert({p, std::make_unique<Map::Chunk>(this, p)}).first->second.get();
	}
	assert(ch != nullptr);
	return ch;
}

const Map::Chunk* Map::chunkAt(posChunk p)const{
	return chunks.at(p).get();
}

bool Map::chunkExists(posChunk p)const{
	return chunks.contains(p);
}

Block& Map::world(posTile p){
	const posChunk pc = p;
	Chunk* ch = chunkAt(pc);
	return ch->data[(p.x - pc.x * chSize) + (p.y - pc.y * chSize) * chSize];
}

const Block& Map::world(posTile p)const{
	const posChunk pc = p;
	try{
		Chunk* ch = chunks.at(pc).get();
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
	//TODO: chunk generation = stutter, more caves must wait for chunk generation queue
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
	if(c.t!=Tile::AIR)
		chunkAt(p)->items.push_back(Item(c.t, posWorld(p) + posWorld{tileSize / 2,tileSize / 2}));
	updateLight(p);
	return c;
}

void Map::update(){
	game.countLightUpdates = lightUpdateQueue.size();
	const posChunk ppc = game.player->pos;
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
	game.player->update();
}

void Map::render()const{
	game.camera.x += (game.player->pos.x - game.camera.x + tileSize / 2) / 10;
	game.camera.y += (game.player->pos.y - game.camera.y + tileSize / 2) / 10;

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
			const Chunk* ch = chunkAt(cx,cy);
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
							FC_Draw(game.font, game.renderer, dest.x, dest.y + tileSize / 2, "%.1f", b.fluid);
						}
					}
					if(game.overlayLight){
						FC_Draw(game.font, game.renderer, dest.x, dest.y + tileSize / 2, "%d", b.light);	
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
	for(int cx = chBeginX; cx <= chEndX; cx++){
		for(int cy = chBeginY; cy <= chEndY; cy++){
			const Chunk* ch = chunkAt(cx, cy);
			for(const auto& i : ch->items){
				i.render(game);
			}
		}
	}

	if(game.debugMode){
		posChunk chPos1 = game.player->pos;
		const SDL_FRect chunkRect = {posX(chPos1.x * chSize), posY(chPos1.y * chSize),chSize * tileSize,chSize * tileSize};
		SDL_SetRenderDrawColor(game.renderer, 0, 0, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &chunkRect);

		const SDL_FRect chunkRect2 = {posX(chBeginX * chSize), posY(chBeginY * chSize),chSize * tileSize,chSize * tileSize};
		SDL_SetRenderDrawColor(game.renderer, 0, 0xff, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &chunkRect2);

		const SDL_FRect chunkRect3 = {posX(chEndX * chSize), posY(chEndY * chSize),chSize * tileSize,chSize * tileSize};
		SDL_SetRenderDrawColor(game.renderer, 0xff, 0, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &chunkRect3);
	}
	game.player->render();
}

bool Map::save(const std::string& file)const{
	std::ofstream out = std::ofstream(file, std::ios::binary);
	if(!out.is_open()||out.bad()){
		std::cerr << "Map::save failed to open "<< file << std::endl;
		return false;
	}
	write(out, magic);
	game.player.get()->save(out);
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
	std::string magic1;
	read(in, magic1);
	if(magic1 != magic){
		std::cerr << "Map::load failed: bad magic in file " << file << std::endl;
		in.close();
		return false;
	}
	game.player.get()->load(in);
	game.camera.x = game.player->pos.x;
	game.camera.y = game.player->pos.y;
	uint32_t tmpCount;
	read(in, &tmpCount);
	for(size_t i = 0; i < tmpCount; i++){
		std::unique_ptr<Chunk> ch = std::make_unique<Chunk>(this);
		ch->load(in);
		chunks[ch->pos] = std::move(ch);
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
