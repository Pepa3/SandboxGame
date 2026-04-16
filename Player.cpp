#include "Helper.h"
#include <iostream>

Player::Player(float x1, float y1) :x(x1),y(y1){

}

bool Player::addInventory(Block b){
	for(size_t i = 0; i < INVENTORY_SIZE; i++){
		if(inventory[i].type == b.t){
			inventory[i].count++;
			return true;
		}
	}
	for(size_t i = 0; i < INVENTORY_SIZE; i++){
		if(inventory[i].type == Tile::UNKNOWN){
			inventory[i] = {b.t,1};
			return true;
		}
	}

	return false;
}

void Player::update(){//TODO: ugly, it still does not work ideally, but it works with negative positions
	const bool* key_states = SDL_GetKeyboardState(NULL);

	float mox, moy;
	int button = SDL_GetMouseState(&mox, &moy);
	int tx = map->tPosX((int) mox);
	int ty = map->tPosY((int) moy);
	if(breakX != tx || breakY != ty){
		breakDurability = 0;
		breakMaxDurability = durability(map->world(tx,ty).t);
		breakX = tx;
		breakY = ty;
	}
	if(button & SDL_BUTTON_LMASK){
		if(lastPlaceTicks <= SDL_GetTicks() - PLACE_MILLIS){
			if(inventory[selectedSlot].count != 0){
				if(map->place(tx, ty, inventory[selectedSlot].type)){
					lastPlaceTicks = SDL_GetTicks();
					inventory[selectedSlot].count--;
					map->updateLight(tx, ty);
				}
			} else if(debugMode){
				map->world(tx, ty).lightSource = !map->world(tx, ty).lightSource;
			}
		}
	} else if(button & SDL_BUTTON_RMASK){
		Block& b = map->destroy(tx, ty);
		if(b.t != Tile::UNKNOWN){
			breakDurability++;
			if(debugMode || breakDurability >= breakMaxDurability){
				breakDurability = 0;
				if(addInventory(b)){
					b.t = Tile::AIR;
					map->updateLight(tx, ty);
				}
			}
		}
	}

	int mX = (int)floorf(x / tileSize);
	int mY = (int) floorf(y / tileSize);
	// on ground if both blocks under player are solid unless standing exactly on tile, then not on ground
	onGround = map->isSolid(mX, mY + 1) || (mX-(x/tileSize) != 0.f && map->isSolid(mX + 1, mY + 1));

	//can move if not going to be blocked and in air not blocked by both tiles

	if(key_states[SDL_SCANCODE_D]){
		if(!map->isSolid((int) floorf((x + PLAYER_SPEED) / tileSize) + 1, mY)
			&& !(!onGround && map->isSolid((int) floorf((x + PLAYER_SPEED) / tileSize) + 1, mY + 1))
			){
			x += PLAYER_SPEED;
		} else if(!map->isSolid(mX + 1, mY) //can snap?
			&& !(!onGround && map->isSolid(mX + 1, mY + 1))
			){
			x = x + (mX - (x / tileSize)) * tileSize +tileSize;
		}
	} else if(key_states[SDL_SCANCODE_A]){
		if(!map->isSolid((int) floorf((x - PLAYER_SPEED) / tileSize), mY)
			&& !(!onGround && map->isSolid((int) floorf((x - PLAYER_SPEED) / tileSize), mY + 1))
			){
			x -= PLAYER_SPEED;
		} else{
			x = x + (mX - (x / tileSize))*tileSize;
		}
	}
	mX = (int) floorf(x / tileSize);

	float distanceToTileBoundary = (mY - y / tileSize)*tileSize;
	if(!map->isSolid(mX, (int) floorf((y + yVel) / tileSize))
		&& !(mX - (x / tileSize) != 0.f && map->isSolid(mX + 1, (int) floorf((y + yVel) / tileSize)))){
		y += yVel;
	} else{
		yVel = distanceToTileBoundary;
	}

	mY = (int) floorf(y / tileSize);
	onGround = map->isSolid(mX, mY + 1) || (mX - (x / tileSize) != 0.f && map->isSolid(mX + 1, mY + 1));
	bool mayBeOnGround = map->isSolid(mX, mY + 2) || (mX - (x / tileSize) != 0.f && map->isSolid(mX + 1, mY + 2));
	distanceToTileBoundary = 32 + (mY - y / tileSize)*tileSize;

	Block& b = map->world(mX, mY);

	if(!onGround){// IN AIR (or water)
		yVel += GRAVITY;
		if(b.fluid != 0){
			yVel = SINK_RATE;
			if(key_states[SDL_SCANCODE_W]){
				yVel = -JUMP_IMPULE;
			}else if(key_states[SDL_SCANCODE_S]){
				yVel = JUMP_IMPULE;
			}
		}
		if(mayBeOnGround && distanceToTileBoundary < yVel)yVel = distanceToTileBoundary;
	}else{// ON GROUND
		yVel = 0;
		if(key_states[SDL_SCANCODE_W]){
			yVel = -JUMP_IMPULE;
		}
	}
}

void Player::render(){
	const SDL_FRect dest = {x - cameraX + wWidth / 2, y - cameraY + wHeight / 2, tileSize, tileSize};
	SDL_RenderTexture(renderer, tiles[Tile::PLAYER], &tileFRect, &dest);
	const SDL_FRect breakBlk = {map->posX(breakX),map->posY(breakY),tileSize,tileSize};
	char p = (char) (((float) breakDurability / (float) breakMaxDurability) * 0xff);
	SDL_SetRenderDrawColor(renderer,0,0,0,p);
	SDL_RenderFillRect(renderer, &breakBlk);


	for(size_t i = 0; i < INVENTORY_SIZE; i++){
		const SDL_FRect itemFrameRect = {wWidth / 2 - (INVENTORY_SIZE / 2.f - i) * tileSize * 3.f,wHeight - tileSize * 3.f,tileSize * 2.f,tileSize * 2.f};
		SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
		SDL_RenderRect(renderer, &itemFrameRect);
		if(i == selectedSlot){
			const SDL_FRect itemSelRect = {wWidth / 2 - (INVENTORY_SIZE / 2.f - i) * tileSize * 3.f+2,wHeight - tileSize * 3.f+2,tileSize * 2.f-4,tileSize * 2.f-4};
			SDL_RenderRect(renderer, &itemSelRect);
		}
		if(inventory[i].type != Tile::UNKNOWN && inventory[i].count != 0){
			const SDL_FRect itemRect = {wWidth / 2 - (INVENTORY_SIZE / 2.f - i) * tileSize * 3.f + tileSize / 2,wHeight - tileSize * 3.f + tileSize / 2, (float)tileSize, (float) tileSize};
			SDL_RenderTexture(renderer, tiles[inventory[i].type], &tileFRect, &itemRect);
			char string[4];
			SDL_snprintf(string, sizeof(string), "%d", inventory[i].count);//TODO: max stack size
			TTF_SetTextString(text, string, 0);
			int w = 0;
			TTF_MeasureString(font, string, 2, 0, &w, nullptr);
			TTF_DrawRendererText(text, itemRect.x+tileSize/2-w/2, itemRect.y + tileSize*3/2);
			
		}
	}
}
