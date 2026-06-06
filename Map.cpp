#include "Helper.h"

Map::Map(GameState& game):game(game){
	const int n1 = terrainHeight(20);
	game.player = std::make_unique<Player>(game, posWorld{20.f * tileSize, (float)tileSize * (n1 - 1)});
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

void Map::generateWorld(){
	std::cout << "Generating chunks" << std::endl;
	for(int i = cUpdateRadius + 1; i >= -cUpdateRadius - 1; i--){
		for(int j = cUpdateRadius + 1; j >= -cUpdateRadius - 1; j--){
			chunkAt(i, j);
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

void Map::spawnEntity(posTile pos){
	entities.push_back(Entity(pos));
}

bool Map::place(int x, int y, Tile t){
	Block& b = world(x, y);
	if(::isSolid(b.t)){
		return false;
	}
	if(b.fluid){//TODO:fluid can be destroyed here
		Block& up = world(x, y - 1);
		if(!::isSolid(up.t)){
			up.fluid.value += fminf(b.fluid.value,0xff-up.fluid.value);
		}
	}
	b.t = t;
	b.skyView = false;
	b.fluid = nullFluid;
	return true;
}

Block Map::destroy(posTile p, Tool tool){
	Block& b = world(p);
	if(!::isDestroyable(b.t))return nullBlock;
	Block c = b;
	c.t = destroyResult(c.t, tool);
	b.t = Tile::AIR;
	b.lightSource = false;
	if(c.t!=Tile::AIR)
		chunkAt(p)->items.push_back(Item(c.t, posWorld(p) + posWorld{tileSize / 2,tileSize / 2}));
	updateLight(p);
	return c;
}

void Map::update(){
	game.countLightUpdates = lightUpdateQueue.size();
	int cChG = game.countChunkGen;
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
	if(game.countChunkGen == cChG){//Pre-generates one chunk per update
		for(int i = -cUpdateRadius - 1; i <= cUpdateRadius + 1 && game.countChunkGen == cChG; i++){
			for(int j = -cUpdateRadius - 1; j <= cUpdateRadius + 1 && game.countChunkGen == cChG; j++){
				if(i != -cUpdateRadius - 1 && i != cUpdateRadius + 1 && j != -cUpdateRadius - 1 && j != cUpdateRadius + 1)continue;
				chunkAt(ppc.x + i, ppc.y + j);
			}
		}
		for(int i = -cUpdateRadius - 2; i <= cUpdateRadius + 2 && game.countChunkGen == cChG; i++){
			for(int j = -cUpdateRadius - 2; j <= cUpdateRadius + 2 && game.countChunkGen == cChG; j++){
				if(i != -cUpdateRadius - 2 && i != cUpdateRadius + 2 && j != -cUpdateRadius - 2 && j != cUpdateRadius + 2)continue;
				chunkAt(ppc.x + i, ppc.y + j);
			}
		}
	}
	game.player->update();
	for(auto& e : entities){
		e.update(game);
	}
	std::erase_if(entities, [](const Entity& e)->bool{return e.dead; });
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
					if(b.fluid){
						const SDL_FRect fluidRect = {posX(x),posY(y) + tileSize * (1 - fminf(b.fluid.value,1.f)),
							(float) tileSize,tileSize * fminf(b.fluid.value,1.f)};
						switch(b.fluid.kind){
						case fluid_t::type::LAVA:
							SDL_SetRenderDrawColor(game.renderer, 0xcc, 0x22, 0x22, 0xff);
							break;
						case fluid_t::type::WATER:
							SDL_SetRenderDrawColor(game.renderer, 0x44, 0x44, 0xaa, 0xff);
							break;
						case fluid_t::type::NONE:
						default:
							SDL_SetRenderDrawColor(game.renderer, 0xaa, 0xaa, 0xaa, 0xff);
							break;
						}
						SDL_RenderFillRect(game.renderer, &fluidRect);
						if(game.overlayFluid){
							FC_Draw(game.font, game.renderer, dest.x, dest.y + tileSize / 2 - 9, "%.1f", b.fluid.value);
						}
					}
					//if(b.light != 127){
					const SDL_FRect shadowRect = {posX(x), posY(y), (float) tileSize, (float) tileSize};
					SDL_SetRenderDrawColor(game.renderer, 0, 0, 0, (127 - b.light) * 2);
					SDL_RenderFillRect(game.renderer, &shadowRect);
					//}
					if(game.overlayLight){
						FC_Draw(game.font, game.renderer, dest.x, dest.y + tileSize / 2, "%d", b.light);
					}
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
	for(const auto& e : entities){
		if(!e.dead)	e.render(game);
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
	const uint32_t countCh = chunks.size();
	write(out, &countCh);
	for(const auto& ch : chunks){
		ch.second->save(out);
	}
	const uint32_t countE = entities.size();
	write(out, &countE);
	for(const auto& e : entities){
		e.save(out);
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
	read(in, &tmpCount);
	for(size_t i = 0; i < tmpCount; i++){
		Entity e = Entity();
		e.load(in);
		entities.push_back(e);
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

int Map::terrainHeight(int x)const{
	float f = 1;
	size_t a = 0;
	switch(getBiome(posTile(x, 0))){
	case Biome::PLAINS:
		a = 30;
		f = 0.01f;
		break;
	case Biome::FOREST:
		a = 40;
		f = 0.02f;
		break;
	case Biome::DESERT:
		a = 10;
		f = 0.005f;
		break;
	case Biome::MOUNTAINS:
		a = 80;
		f = 0.02f;
		break;
	}
	posTile p = posTile(x, 0);
	posChunk pc = p;
	int xd = p.x - pc.x * chSize;
	if(xd > chSize / 2){//xd=chSize/2 -- chSize
		if(getBiome(pc) != getBiome(pc.right())){
			int tc = terrainHeight(pc.x * chSize + chSize / 2);
			int tn = terrainHeight(pc.right().x * chSize + chSize / 2);
			tn = (tc + tn) / 2;
			int t = (int)(tc + (xd - chSize / 2) * 2.f / chSize * (tn - tc));
			return t;
		}
	} else if(xd < chSize / 2){
		if(getBiome(pc) != getBiome(pc.left())){
			int tc = terrainHeight(pc.x * chSize + chSize / 2);
			int tn = terrainHeight(pc.left().x * chSize + chSize / 2);
			tn = (tc + tn) / 2;
			int t = (int)(tn + xd * 2.f / chSize * (tc - tn));
			return t;
		}
	}
	return (int) (perlin.noise2D(x * f, 1) * a);
}

Biome Map::getBiome(posChunk pos)const{
	const float n = perlin.noise2D((float)pos.x*0.1f, 10);

	if(n < -0.5f)
		return Biome::DESERT;

	if(n < -0.1f)
		return Biome::PLAINS;

	if(n < 0.2f)
		return Biome::FOREST;

	return Biome::MOUNTAINS;
}
