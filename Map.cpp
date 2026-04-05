#include "Helper.h"
#include <cassert>

Map::Map(size_t mW, size_t mH) :mWidth(mW), mHeight(mH), player{mW / 2.f * tileSize,tileSize*10}{
	world = new Tile[mW * mH];
	cameraX = player.x;
	cameraY = player.y;
}

Map::~Map(){
	delete[mWidth * mHeight] world;
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

void Map::update(){
	
	for(size_t i = 0; i < mWidth*(mHeight-1); i++){//TODO: this is a bad idea
		if(world[i] == Tile::SAND && world[i + mWidth] == Tile::AIR){
			world[i] = Tile::AIR;
			world[i + mWidth] = Tile::SAND;
		}
	}
	
	const bool* key_states = SDL_GetKeyboardState(NULL);

	float mx, my;
	int button = SDL_GetMouseState(&mx, &my);
	int t = scrTile(mx, my);
	//TODO: handle only on mouseDown events
	if(button & SDL_BUTTON_LMASK){
		world[t] = Tile::STONE;
	} else if(button & SDL_BUTTON_RMASK){
		world[t] = Tile::AIR;
	}

	int mX = player.x / tileSize;
	int mY = player.y / tileSize;
	//(mX >= 0 && mX < mWidth&& mY >= 0 && mY < mHeight)
	// on ground if both blocks under player are solid unless standing exactly on tile, then not on ground
	player.onGround = isSolid(mX, mY + 1) ||(fmodf(player.x, 32) > 0 && isSolid(mX + 1, mY + 1));

	//can move if not going to be blocked and in air not blocked by both tiles

	if(key_states[SDL_SCANCODE_D]){
		if(!isSolid((player.x + PLAYER_SPEED) / tileSize + 1, player.y / tileSize)
			&& !(!player.onGround && isSolid((player.x + PLAYER_SPEED) / tileSize + 1, player.y / tileSize + 1))
			){
			player.x += PLAYER_SPEED;
		} else if(!isSolid(player.x / tileSize + 1, player.y / tileSize) //can snap?
			&& !(!player.onGround && isSolid(player.x / tileSize + 1, player.y / tileSize + 1))
			){
			player.x = player.x - fmodf(player.x, 32)+32;
		}
	}else if(key_states[SDL_SCANCODE_A]){
		if(!isSolid((player.x - PLAYER_SPEED) / tileSize, player.y / tileSize)
			&& !(!player.onGround && isSolid((player.x - PLAYER_SPEED) / tileSize, player.y / tileSize + 1))
			){
			player.x -= PLAYER_SPEED;
		} else{
			player.x = player.x - fmodf(player.x, 32);
		}
	}
	float distanceToTileBoundary = -fmodf(player.y, 32);
	if(!isSolid(player.x / tileSize, (player.y + player.yVel) / tileSize)
		&& !(fmodf(player.x, 32) > 0 && isSolid(player.x / tileSize + 1, (player.y + player.yVel) / tileSize))){
		player.y += player.yVel;
	}else{
		player.yVel = distanceToTileBoundary;
	}

	mX = player.x / tileSize;
	mY = player.y / tileSize;
	//(mX >= 0 && mX < mWidth&& mY >= 0 && mY < mHeight)
	player.onGround = isSolid(mX, mY + 1) || (fmodf(player.x,32) > 0 && isSolid(mX + 1, mY + 1));
	bool mayBeOnGround = isSolid(mX, mY + 2) || (fmodf(player.x, 32) > 0 && isSolid(mX + 1, mY + 2));
	distanceToTileBoundary = 32-fmodf(player.y,32);

	if(!player.onGround){// IN AIR
		player.yVel += GRAVITY;
		if(mayBeOnGround && distanceToTileBoundary < player.yVel)player.yVel = distanceToTileBoundary;
	}else{ // ON GROUND
		player.yVel = 0;
		if(key_states[SDL_SCANCODE_W]){
			player.yVel = -JUMP_IMPULE;
		}
	}
}

void Map::render(){
	cameraX += (player.x - cameraX + tileSize / 2) / 10;
	cameraY += (player.y - cameraY + tileSize / 2) / 10;

	int beginX = fmaxf(player.x / tileSize - RENDER_RADIUS, 0);
	int beginY = fmaxf(player.y / tileSize - RENDER_RADIUS, 0);
	int endX = fminf(player.x / tileSize + RENDER_RADIUS, mWidth);
	int endY = fminf(player.y / tileSize + RENDER_RADIUS, mHeight);
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

	const SDL_FRect dest = {player.x - cameraX + wWidth / 2, player.y - cameraY + wHeight / 2, tileSize, tileSize};
	SDL_RenderTexture(renderer, tiles[Tile::PLAYER], &tileFRect, &dest);

	for(size_t i = 0; i < INVENTORY_SIZE; i++){
		const SDL_FRect itemFrameRect = {wWidth/2-(INVENTORY_SIZE/2.f-i)*tileSize*3.f,wHeight-tileSize*3,tileSize*2,tileSize*2};
		SDL_SetRenderDrawColor(renderer, 0xaa, 0xaa, 0xee, 0xff);
		SDL_RenderRect(renderer, &itemFrameRect);
		if(player.inventory[i].type != Tile::UNKNOWN){
			const SDL_FRect itemRect = {wWidth / 2 - (INVENTORY_SIZE / 2.f - i) * tileSize * 3.f + tileSize / 2,wHeight - tileSize * 3 + tileSize / 2, tileSize, tileSize};
			SDL_RenderTexture(renderer, tiles[player.inventory[i].type], &tileFRect, &itemRect);
		}
	}

}