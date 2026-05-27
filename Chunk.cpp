#include "Helper.h"

//Chunk generation takes approximately 14 ms
Map::Chunk::Chunk(Map* map, posChunk p) :pos(p), map(map){
	std::cout << "Created chunk [" << p.x << "][" << p.y << "]" << std::endl;
	generate();
}

void Map::Chunk::generate(){
	const int gx = pos.x * chSize;
	const int gy = pos.y * chSize;
	const Biome biome = map->getBiome(pos);
	std::mt19937 rng((size_t) ((uint64_t) mapSeed * mapSeed) + pos.x + pos.y * mapSeed);
	
	for(int i = 0; i < chSize; i++){
		const int n1 = map->terrainHeight(gx + i);
		for(int j = 0; j < chSize; j++){
			if(j + gy < n1){
				Block b = Block(Tile::AIR, Tile::AIR, nullFluid, 127);
				b.skyView = true;
				if(biome != Biome::DESERT && gy + j > 0)b.fluid = fluid_t(fluid_t::type::WATER,2.f);
				data[i + j * chSize] = b;
			} else if(j + gy == n1){
				if(biome == Biome::DESERT){
					data[i + j * chSize] = Block(Tile::SAND, Tile::SAND);
				} else{
					data[i + j * chSize] = Block(Tile::GRASS, Tile::DIRT);
				}
			} else if(j + gy < n1 + dirtHeight){
				if(biome == Biome::DESERT){
					data[i + j * chSize] = Block(Tile::SAND, Tile::SAND);
				} else{
					data[i + j * chSize] = Block(Tile::DIRT, Tile::DIRT);
				}
			} else{
				std::uniform_int_distribution<> dist(0, 10000);
				const bool oreG = dist(rng) > 9990;
				const bool oreC = dist(rng) > 9990;
				if(oreG){
					data[i + j * chSize] = Block(Tile::GLOW, Tile::STONE);
					data[i + j * chSize].lightSource = true;
				} else if(oreC){
					data[i + j * chSize] = Block(Tile::COAL, Tile::STONE);
				} else{
					data[i + j * chSize] = Block(Tile::STONE, Tile::STONE);
				}
			}
			const float caveNoise = perlin.octave2D_01((gx + i) * 0.04f, (gy + j) * 0.04f, 4);
			if(caveNoise > 0.7f){
				data[i + j * chSize].t = Tile::AIR;
				data[i + j * chSize].lightSource = false;
			}
		}
	}
	if(biome == Biome::FOREST){
		std::uniform_int_distribution<> dist(0, 10000);
		for(int i = 4; i < chSize - 4; i++){
			const bool hasTree = dist(rng) > 8000;
			if(hasTree){
				const int treeHeight = map->terrainHeight(gx + i);
				const short treeChY = (short) floor(treeHeight / (float) chSize);
				if(treeChY == pos.y && treeHeight <= 0)generateTree(i, treeHeight - gy);
			}
		}
	}

	for(int x = 0; x < chSize; x++){
		for(int y = 0; y < chSize; y++){
			updateLight(x, y, true);
		}
	}
	for(int x = chSize - 1; x >= 0; x--){
		for(int y = chSize - 1; y >= 0; y--){
			updateLight(x, y, true);
		}
	}
}

void Map::Chunk::generateTree(int tx, int ty){
	const int mx = pos.x * chSize;
	const int my = pos.y * chSize;
	for(int i = 0; i < 6; i++){
		if(ty - i >= 0){
			data[tx + (ty - i) * chSize] = Block(Tile::WOOD, Tile::AIR);
		} else{
			map->world(mx + tx, my + ty - i) = Block(Tile::WOOD, Tile::AIR);
		}
	}
	for(int j = 1; j <= 3; j++){
		for(int i = 2 + j; i < 9 - j; i++){
			if(ty - i >= 0 && tx - j >= 0 && j < chSize){
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
					: map->world(pos.x * chSize + i, pos.y * chSize + j + 1);
				if(under.t == Tile::AIR){
					const Block tmp = b;
					b = Block(under.t, b.bg, under.fluid);
					under = Block(tmp.t, under.bg, tmp.fluid, tmp.light);
					map->updateLight(pos.x * chSize + i, pos.y * chSize + j, false);
				}
			}
			fluid_t& fl1 = b.fluid;
			if(fl1){
				Block& under = (j < chSize - 1)
					? data[i + (j + 1) * chSize]
					: map->world(pos.x * chSize + i, pos.y * chSize + j + 1);
				fluid_t& flu = under.fluid;
				if(!::isSolid(under.t) && (flu.kind == fluid_t::NONE || flu.kind == fl1.kind)){
					float fl = flu.value + fl1.value;
					if(fl <= 1){//Fill tile below
						flu.value = fl;
						fl1.value = 0;
						flu.kind = fl1.kind;
						fl1.kind = fluid_t::NONE;
					} else if(fl > 1 && fl <= 2.25f){//Second tile pressure propagation
						float k = (fl1.value + (flu.value - 1) * 4) / 2;
						flu.value = 1 + k * 0.25f;
						fl1.value = fl - 1 - k * 0.25f;
						flu.kind = fl1.kind;
					} else if(fl > 2.25f){//Deep tile pressure propagation
						fl1.value = (fl - 0.25f) / 2;
						flu.value = (fl + 0.25f) / 2;
						flu.kind = fl1.kind;
					}
				}else if(!::isSolid(under.t) && (flu.kind != fluid_t::NONE && flu.kind != fl1.kind)){
					if(flu.kind == fluid_t::WATER && fl1.kind == fluid_t::LAVA){
						std::swap(flu, fl1);
					}
				}
				if(fl1){
					//Spill horizontally
					Block& left = (i > 0)
						? data[i - 1 + j * chSize]
						: map->world(pos.x * chSize + i - 1, pos.y * chSize + j);
					Block& right = (i < chSize - 1)
						? data[i + 1 + j * chSize]
						: map->world(pos.x * chSize + i + 1, pos.y * chSize + j);
					float fldl = fl1.value - left.fluid.value;
					if(!::isSolid(left.t) && fldl > 0 && (left.fluid.kind == fluid_t::NONE || left.fluid.kind == fl1.kind)){
						fl1.value -= fldl / 2;
						left.fluid.value += fldl / 2;
						left.fluid.kind = fl1.kind;
					} else if(!::isSolid(left.t) && (left.fluid.kind != fluid_t::NONE && left.fluid.kind != fl1.kind)){
						if(left.fluid.kind == fluid_t::WATER && fl1.kind == fluid_t::LAVA){
							std::swap(left.fluid, fl1);
						}
					}//TODO: left-biased
					float fldr = fl1.value - right.fluid.value;
					if(!::isSolid(right.t) && fldr > 0 && (right.fluid.kind == fluid_t::NONE || right.fluid.kind == fl1.kind)){
						fl1.value -= fldr / 2;
						right.fluid.value += fldr / 2;
						right.fluid.kind = fl1.kind;
					}/* else if(!::isSolid(right.t) && (right.fluid.kind != fluid_t::NONE && right.fluid.kind != fl1.kind)){
						if(right.fluid.kind == fluid_t::WATER && fl1.kind == fluid_t::LAVA){
							std::swap(right.fluid, fl1);
						}
					}*/
				}
				if(fl1.value > 1.f){
					//Propagate up if high pressure
					Block& up = (j > 0)
						? data[i + (j - 1) * chSize]
						: map->world(pos.x * chSize + i, pos.y * chSize + j - 1);
					if(!::isSolid(up.t) && !up.fluid){
						float fld = fl1.value - 1;
						fl1.value -= fld / 2;
						up.fluid.value += fld / 2;
						up.fluid.kind = fl1.kind;
					}
				}
				if(fl1.value <= 0.01f)fl1 = nullFluid;//TODO: a better min value?
			}
		}
	}
	for(auto& i : items){
		i.update(map->game);
	}
	std::erase_if(items, [](const Item& i)->bool{return i.pickedUp; });
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
	if(!(genStep && j == 0) && c.t == Tile::AIR){
		c.skyView = u.skyView;
	}
	char lgt = 0;
	if(c.skyView || c.lightSource){
		lgt = 127;
	} else if(!::isSolid(c.t)){//Transparent
		const char lgt1 = lfmax(d.light - lightFalloff, u.light - lightFalloff);
		const char lgt2 = lfmax(l.light - lightFalloff, r.light - lightFalloff);
		lgt = lfmax(0, lfmax(lgt1, lgt2));
	} else{//Solid
		const char lgt1 = lfmax(d.light - lightFalloff * 4, u.light - lightFalloff * 4);
		const char lgt2 = lfmax(l.light - lightFalloff * 4, r.light - lightFalloff * 4);
		lgt = lfmax(0, lfmax(lgt1, lgt2));
	}
	if(lgt != c.light)lightChanged = true;
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

void Map::Chunk::save(std::ofstream& out) const{
	write(out, &pos.x);
	write(out, &pos.y);
	write(out, &data);
	const size_t count = items.size();
	write(out, &count);
	for(size_t i = 0; i < count; i++){
		write(out, &items[i]);
	}
}
void Map::Chunk::load(std::ifstream& in){
	read(in, &pos.x);
	read(in, &pos.y);
	read(in, &data);
	size_t count;
	read(in, &count);
	for(size_t i = 0; i < count; i++){
		Item it;
		read(in, &it);
		items.push_back(it);
	}
}
