#include "Helper.h"

Player::Player(Map* m, float x1, float y1) :map(m),x(x1),y(y1){

}

void Player::update(){//TODO: ugly and not really working ideally
	const bool* key_states = SDL_GetKeyboardState(NULL);

	float mx, my;
	int button = SDL_GetMouseState(&mx, &my);
	int t = map->scrTile(mx, my);
	//TODO: handle only on mouseDown events --- maybe not
	if(button & SDL_BUTTON_LMASK){
		map->place(t,Tile::STONE);
	} else if(button & SDL_BUTTON_RMASK){
		map->place(t, Tile::AIR);
	}

	int mX = x / tileSize;
	int mY = y / tileSize;
	//(mX >= 0 && mX < mWidth&& mY >= 0 && mY < mHeight)
	// on ground if both blocks under player are solid unless standing exactly on tile, then not on ground
	onGround = map->isSolid(mX, mY + 1) || (fmodf(x, 32) > 0 && map->isSolid(mX + 1, mY + 1));

	//can move if not going to be blocked and in air not blocked by both tiles

	if(key_states[SDL_SCANCODE_D]){
		if(!map->isSolid((x + PLAYER_SPEED) / tileSize + 1, y / tileSize)
			&& !(!onGround && map->isSolid((x + PLAYER_SPEED) / tileSize + 1, y / tileSize + 1))
			){
			x += PLAYER_SPEED;
		} else if(!map->isSolid(x / tileSize + 1, y / tileSize) //can snap?
			&& !(!onGround && map->isSolid(x / tileSize + 1, y / tileSize + 1))
			){
			x = x - fmodf(x, 32) + 32;
		}
	} else if(key_states[SDL_SCANCODE_A]){
		if(!map->isSolid((x - PLAYER_SPEED) / tileSize, y / tileSize)
			&& !(!onGround && map->isSolid((x - PLAYER_SPEED) / tileSize, y / tileSize + 1))
			){
			x -= PLAYER_SPEED;
		} else{
			x = x - fmodf(x, 32);
		}
	}
	float distanceToTileBoundary = -fmodf(y, 32);
	if(!map->isSolid(x / tileSize, (y + yVel) / tileSize)
		&& !(fmodf(x, 32) > 0 && map->isSolid(x / tileSize + 1, (y + yVel) / tileSize))){
		y += yVel;
	} else{
		yVel = distanceToTileBoundary;
	}

	mX = x / tileSize;
	mY = y / tileSize;
	//(mX >= 0 && mX < mWidth&& mY >= 0 && mY < mHeight)
	onGround = map->isSolid(mX, mY + 1) || (fmodf(x, 32) > 0 && map->isSolid(mX + 1, mY + 1));
	bool mayBeOnGround = map->isSolid(mX, mY + 2) || (fmodf(x, 32) > 0 && map->isSolid(mX + 1, mY + 2));
	distanceToTileBoundary = 32 - fmodf(y, 32);

	if(!onGround){// IN AIR
		yVel += GRAVITY;
		if(mayBeOnGround && distanceToTileBoundary < yVel)yVel = distanceToTileBoundary;
	} else{ // ON GROUND
		yVel = 0;
		if(key_states[SDL_SCANCODE_W]){
			yVel = -JUMP_IMPULE;
		}
	}
}

void Player::render(){
	const SDL_FRect dest = {x - cameraX + wWidth / 2, y - cameraY + wHeight / 2, tileSize, tileSize};
	SDL_RenderTexture(renderer, tiles[Tile::PLAYER], &tileFRect, &dest);

	for(size_t i = 0; i < INVENTORY_SIZE; i++){
		const SDL_FRect itemFrameRect = {wWidth / 2 - (INVENTORY_SIZE / 2.f - i) * tileSize * 3.f,wHeight - tileSize * 3,tileSize * 2,tileSize * 2};
		SDL_SetRenderDrawColor(renderer, 0xaa, 0xaa, 0xee, 0xff);
		SDL_RenderRect(renderer, &itemFrameRect);
		if(inventory[i].type != Tile::UNKNOWN){
			const SDL_FRect itemRect = {wWidth / 2 - (INVENTORY_SIZE / 2.f - i) * tileSize * 3.f + tileSize / 2,wHeight - tileSize * 3 + tileSize / 2, tileSize, tileSize};
			SDL_RenderTexture(renderer, tiles[inventory[i].type], &tileFRect, &itemRect);
		}
	}
}
