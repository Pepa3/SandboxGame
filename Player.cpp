#include "Helper.h"
#include <iostream>

Player::Player(GameState& game, float x1, float y1) :x(x1),y(y1), game(game){}

bool Player::addInventory(Block b){
	for(uint8_t i = 0; i < INVENTORY_SIZE; i++){
		if(inventory[i].type == b.t){
			inventory[i].count++;
			return true;
		}
	}
	for(uint8_t i = 0; i < INVENTORY_SIZE; i++){
		if(inventory[i].type == Tile::UNKNOWN || inventory[i].count == 0){
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
	int tx = game.map->tPosX((int) mox);
	int ty = game.map->tPosY((int) moy);
	if(breakX != tx || breakY != ty){
		breakDurability = 0;
		breakMaxDurability = durability(game.map->world(tx,ty).t);
		breakX = tx;
		breakY = ty;
	}
	if(button & SDL_BUTTON_RMASK){
		if(lastPlaceTicks <= SDL_GetTicks() - PLACE_MILLIS){
			if(inventory[selectedSlot].count != 0){
				if(game.map->place(tx, ty, inventory[selectedSlot].type)){
					lastPlaceTicks = SDL_GetTicks();
					inventory[selectedSlot].count--;
					game.map->updateLight(tx, ty);
				}
			} else if(game.debugMode){
				game.map->world(tx, ty).lightSource = !game.map->world(tx, ty).lightSource;
				game.map->updateLight(tx, ty);
			}
		}
	} else if(button & SDL_BUTTON_LMASK){
		Block& b = game.map->world(tx, ty);
		if(isSolid(b.t)){
			breakDurability++;
			if(game.debugMode || breakDurability >= breakMaxDurability){
				breakDurability = 0;
				Block d = game.map->destroy(tx, ty);
				if(!addInventory(d)){
					std::cout << "No space in inventory!" << std::endl;
				}
			}
		}
	}

	int mX = (int)floorf(x / tileSize);
	int mY = (int) floorf(y / tileSize);
	// on ground if both blocks under player are solid unless standing exactly on tile, then not on ground
	onGround = game.map->isSolid(mX, mY + 1) || (mX-(x/tileSize) != 0.f && game.map->isSolid(mX + 1, mY + 1));

	//can move if not going to be blocked and in air not blocked by both tiles

	if(key_states[SDL_SCANCODE_D]){
		if(!game.map->isSolid((int) floorf((x + PLAYER_SPEED) / tileSize) + 1, mY)
			&& !(!onGround && game.map->isSolid((int) floorf((x + PLAYER_SPEED) / tileSize) + 1, mY + 1))
			){
			x += PLAYER_SPEED;
		} else if(!game.map->isSolid(mX + 1, mY) //can snap?
			&& !(!onGround && game.map->isSolid(mX + 1, mY + 1))
			){
			x = x + (mX - (x / tileSize)) * tileSize +tileSize;
		}
	} else if(key_states[SDL_SCANCODE_A]){
		if(!game.map->isSolid((int) floorf((x - PLAYER_SPEED) / tileSize), mY)
			&& !(!onGround && game.map->isSolid((int) floorf((x - PLAYER_SPEED) / tileSize), mY + 1))
			){
			x -= PLAYER_SPEED;
		} else{
			x = x + (mX - (x / tileSize))*tileSize;
		}
	}
	mX = (int) floorf(x / tileSize);

	float distanceToTileBoundary = (mY - y / tileSize)*tileSize;
	if(!game.map->isSolid(mX, (int) floorf((y + yVel) / tileSize))
		&& !(mX - (x / tileSize) != 0.f && game.map->isSolid(mX + 1, (int) floorf((y + yVel) / tileSize)))){
		y += yVel;
	} else{
		yVel = distanceToTileBoundary;
	}

	mY = (int) floorf(y / tileSize);
	onGround = game.map->isSolid(mX, mY + 1) || (mX - (x / tileSize) != 0.f && game.map->isSolid(mX + 1, mY + 1));
	bool mayBeOnGround = game.map->isSolid(mX, mY + 2) || (mX - (x / tileSize) != 0.f && game.map->isSolid(mX + 1, mY + 2));
	distanceToTileBoundary = 32 + (mY - y / tileSize)*tileSize;

	Block& b = game.map->world(mX, mY);

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
	const SDL_FRect dest = {x - game.cameraX + game.wWidth / 2, y - game.cameraY + game.wHeight / 2, tileSize, tileSize};
	SDL_RenderTexture(game.renderer, game.tiles[Tile::PLAYER], &tileFRect, &dest);
	const SDL_FRect breakBlk = {game.map->posX(breakX),game.map->posY(breakY),tileSize,tileSize};
	char p = (char) (((float) breakDurability / (float) breakMaxDurability) * 0xff);
	SDL_SetRenderDrawColor(game.renderer,0,0,0,p);
	SDL_RenderFillRect(game.renderer, &breakBlk);


	for(uint8_t i = 0; i < INVENTORY_SIZE; i++){
		const SDL_FRect itemFrameRect = {game.wWidth / 2 - (INVENTORY_SIZE / 2.f - i) * tileSize * 3.f,game.wHeight - tileSize * 3.f,tileSize * 2.f,tileSize * 2.f};
		SDL_SetRenderDrawColor(game.renderer, 0xff, 0xff, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &itemFrameRect);
		if(i == selectedSlot){
			const SDL_FRect itemSelRect = {game.wWidth / 2 - (INVENTORY_SIZE / 2.f - i) * tileSize * 3.f+2,game.wHeight - tileSize * 3.f+2,tileSize * 2.f-4,tileSize * 2.f-4};
			SDL_RenderRect(game.renderer, &itemSelRect);
		}
		if(inventory[i].type != Tile::UNKNOWN && inventory[i].count != 0){
			const SDL_FRect itemRect = {game.wWidth / 2 - (INVENTORY_SIZE / 2.f - i) * tileSize * 3.f + tileSize / 2,game.wHeight - tileSize * 3.f + tileSize / 2, (float)tileSize, (float) tileSize};
			SDL_RenderTexture(game.renderer, game.tiles[inventory[i].type], &tileFRect, &itemRect);
			char string[4];
			SDL_snprintf(string, sizeof(string), "%d", inventory[i].count);//TODO: max stack size (255)
			TTF_SetTextString(game.text, string, 0);
			int w = 0;
			TTF_MeasureString(game.font, string, 2, 0, &w, nullptr);
			TTF_DrawRendererText(game.text, itemRect.x+tileSize/2-w/2, itemRect.y + tileSize*3/2);
			
		}
	}
}

void Player::save(std::ofstream& out) const{
	write(out, &x);
	write(out, &y);
	write(out, &yVel);
	write(out, &onGround);
	write(out, &inventory);
	write(out, &selectedSlot);
	write(out, &breakDurability);
	write(out, &breakMaxDurability);
	write(out, &breakX);
	write(out, &breakY);
}

void Player::load(std::ifstream& in){
	read(in, &x);
	read(in, &y);
	read(in, &yVel);
	read(in, &onGround);
	read(in, &inventory);
	read(in, &selectedSlot);
	lastPlaceTicks = 0;
	read(in, &breakDurability);
	read(in, &breakMaxDurability);
	read(in, &breakX);
	read(in, &breakY);
}