#include "Helper.h"

Map::Map(size_t mW, size_t mH) :mWidth(mW), mHeight(mH), player{mW / 2.f * tileSize,10.f}{
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
	return shiftedX / tileSize + shiftedY / tileSize * mWidth;
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

	if(key_states[SDL_SCANCODE_D]){
		player.x += PLAYER_SPEED;
	}else if(key_states[SDL_SCANCODE_A]){
		player.x -= PLAYER_SPEED;
	}

	player.y += player.yVel;
	//player.yVel = player.yVel + GRAVITY;

	int mX = player.x / tileSize;
	int mY = player.y / tileSize;
	player.onGround = (mX >= 0 && mX < mWidth&& mY >= 0 && mY < mHeight) && isSolid(world[mX + (mY + 1) * mWidth]);
	bool mayBeOnGround = (mX >= 0 && mX < mWidth&& mY >= 0 && mY < mHeight) && isSolid(world[mX + (mY + 2) * mWidth]);
	float distanceToTileBoundary = 32-fmodf(player.y,32);

	if(!player.onGround){// IN AIR
		player.yVel += GRAVITY;
		if(mayBeOnGround && distanceToTileBoundary < player.yVel)player.yVel = distanceToTileBoundary;
	}else{ // ON GROUND
		player.yVel = 0;
		if(key_states[SDL_SCANCODE_W]){
			player.yVel = -JUMP_IMPULE;
		}
	}

	cameraX += (player.x - cameraX+tileSize/2)/10;
	cameraY += (player.y - cameraY+tileSize/2)/10;
}

void Map::render(){
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