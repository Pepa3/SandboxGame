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

inline int Map::scrTile(float x, float y)const{
	int shiftedX = x + cameraX-wWidth/2.f;
	int shiftedY = y + cameraY-wHeight/2.f;
	if(shiftedX < 0 || shiftedY < 0)return 0;
	return shiftedX / tileSize + shiftedY / tileSize * mWidth;
}

bool Map::isSolid(int x, int y)const{
	assert(x >= 0 && x < mWidth && "Bad position in Map::isSolid()!");
	assert(y >= 0 && y < mHeight && "Bad position in Map::isSolid()!");
	return ::isSolid(world[x + y * mWidth]);
}

void Map::generateWorld(){
	for(size_t i = 0; i < mWidth*mHeight; i++){
		if(i / mWidth > mHeight / 2)
			world[i] = Tile::STONE;
		else if(i / mWidth > mHeight / 3)
			world[i] = Tile::DIRT;
		else
			world[i] = Tile::AIR;
	}
}

void Map::handleKeyDown(char key){
}

void Map::update(){
	const bool* key_states = SDL_GetKeyboardState(NULL);

	float mx, my;
	int button = SDL_GetMouseState(&mx, &my);
	int t = scrTile(mx, my);
	if(button & SDL_BUTTON_LMASK){
		world[t] = Tile::DIRT;
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
	for(size_t i = 0; i < mWidth*mHeight; i++){
		const SDL_FRect dest = {tPosX(i),tPosY(i),tileSize,tileSize};
		SDL_RenderTexture(renderer, tiles[world[i]], &tileFRect, &dest);
	}
	float mx, my;
	SDL_GetMouseState(&mx, &my);
	int t = scrTile(mx, my);

	const SDL_FRect cursorRect = {tPosX(t),tPosY(t),tileSize,tileSize};
	SDL_SetRenderDrawColor(renderer,0,0,0,0xff);
	SDL_RenderRect(renderer, &cursorRect);

	const SDL_FRect dest = {player.x - cameraX + wWidth / 2,player.y - cameraY + wHeight / 2,tileSize,tileSize};
	SDL_RenderTexture(renderer, tiles[Tile::UNKNOWN], &tileFRect, &dest);
}