#include "Helper.h"
#include <iostream>

Player::Player(GameState& game, posWorld p) :pos(p), game(game){}

bool Player::addInventory(Block b)noexcept{
	for(uint8_t i = 0; i < cInventorySize; i++){
		if(inventory[i].type == b.t){
			inventory[i].count++;
			return true;
		}
	}
	for(uint8_t i = 0; i < cInventorySize; i++){
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
	const int button = SDL_GetMouseState(&mox, &moy);
	const int tx = game.map->tPosX(mox);
	const int ty = game.map->tPosY(moy);
	if(brokenBlock.x != tx || brokenBlock.y != ty){
		breakDurability = 0;
		breakMaxDurability = durability(game.map->world(tx,ty).t);
		brokenBlock.x = tx;
		brokenBlock.y = ty;
	}
	if(button & SDL_BUTTON_RMASK){
		if(lastPlaceTicks <= SDL_GetTicks() - cPlaceTimeoutMillis){
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
		const Block& b = game.map->world(tx, ty);
		if(isSolid(b.t)){
			breakDurability++;
			if(game.debugMode || breakDurability >= breakMaxDurability){
				breakDurability = 0;
				const Block d = game.map->destroy(tx, ty, Tool::HAND);
				if(d.t!=Tile::AIR && !addInventory(d)){
					std::cout << "No space in inventory!" << std::endl;
				}
			}
		}
	}
	posTile m = pos;//m.x,m.y
	// on ground if both blocks under player are solid unless standing exactly on tile, then not on ground
	onGround = game.map->isSolid(m.x, m.y + 1) || (m.x-(pos.x/tileSize) != 0.f && game.map->isSolid(m.x + 1, m.y + 1));

	//can move if not going to be blocked and in air not blocked by both tiles

	if(key_states[SDL_SCANCODE_D]){
		if(!game.map->isSolid((int) floorf((pos.x + cPlayerSpeed) / tileSize) + 1, m.y)
			&& !(!onGround && game.map->isSolid((int) floorf((pos.x + cPlayerSpeed) / tileSize) + 1, m.y + 1))
			){
			pos.x += cPlayerSpeed;
		} else if(!game.map->isSolid(m.x + 1, m.y) //can snap?
			&& !(!onGround && game.map->isSolid(m.x + 1, m.y + 1))
			){
			pos.x = pos.x + (m.x - (pos.x / tileSize)) * tileSize + tileSize;
		}
	} else if(key_states[SDL_SCANCODE_A]){
		if(!game.map->isSolid((int) floorf((pos.x - cPlayerSpeed) / tileSize), m.y)
			&& !(!onGround && game.map->isSolid((int) floorf((pos.x - cPlayerSpeed) / tileSize), m.y + 1))
			){
			pos.x -= cPlayerSpeed;
		} else{
			pos.x = pos.x + (m.x - (pos.x / tileSize))*tileSize;
		}
	}
	m.x = (int) floorf(pos.x / tileSize);

	float distanceToTileBoundary = (m.y - pos.y / tileSize)*tileSize;
	if(!game.map->isSolid(m.x, (int) floorf((pos.y + yVel) / tileSize))
		&& !(m.x - (pos.x / tileSize) != 0.f && game.map->isSolid(m.x + 1, (int) floorf((pos.y + yVel) / tileSize)))){
		pos.y += yVel;
	} else{
		yVel = distanceToTileBoundary;
	}

	m.y = (int) floorf(pos.y / tileSize);
	onGround = game.map->isSolid(m.x, m.y + 1) || (m.x - (pos.x / tileSize) != 0.f && game.map->isSolid(m.x + 1, m.y + 1));
	const bool mayBeOnGround = game.map->isSolid(m.x, m.y + 2) || (m.x - (pos.x / tileSize) != 0.f && game.map->isSolid(m.x + 1, m.y + 2));
	distanceToTileBoundary = 32 + (m.y - pos.y / tileSize)*tileSize;

	const Block& b = game.map->world(m.x, m.y);

	if(!onGround){// IN AIR (or water)
		yVel += cGravity;
		if(b.fluid != 0){
			yVel = cSinkRate;
			if(key_states[SDL_SCANCODE_W]){
				yVel = -cJumpImpulse;
			}else if(key_states[SDL_SCANCODE_S]){
				yVel = cJumpImpulse;
			}
		}
		if(mayBeOnGround && distanceToTileBoundary < yVel)yVel = distanceToTileBoundary;
	}else{// ON GROUND
		yVel = 0;
		if(key_states[SDL_SCANCODE_W]){
			yVel = -cJumpImpulse;
		}
	}
}

void Player::render()noexcept{
	const SDL_FRect dest = {pos.x - game.camera.x + game.wWidth / 2, pos.y - game.camera.y + game.wHeight / 2, tileSize, tileSize};
	SDL_RenderTexture(game.renderer, game.tiles[(int) Tile::PLAYER], &tileFRect, &dest);
	const SDL_FRect breakBlk = {game.map->posX(brokenBlock.x),game.map->posY(brokenBlock.y),tileSize,tileSize};
	const char p = (char) (((float) breakDurability / (float) breakMaxDurability) * 0xff);
	SDL_SetRenderDrawColor(game.renderer,0,0,0,p);
	SDL_RenderFillRect(game.renderer, &breakBlk);


	for(uint8_t i = 0; i < cInventorySize; i++){
		const SDL_FRect itemFrameRect = {game.wWidth / 2 - (cInventorySize / 2.f - i) * tileSize * 3.f,game.wHeight - tileSize * 3.f,tileSize * 2.f,tileSize * 2.f};
		SDL_SetRenderDrawColor(game.renderer, 0xff, 0xff, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &itemFrameRect);
		if(i == selectedSlot){
			const SDL_FRect itemSelRect = {game.wWidth / 2 - (cInventorySize / 2.f - i) * tileSize * 3.f+2,game.wHeight - tileSize * 3.f+2,tileSize * 2.f-4,tileSize * 2.f-4};
			SDL_RenderRect(game.renderer, &itemSelRect);
		}
		if(inventory[i].type != Tile::UNKNOWN && inventory[i].count != 0){
			const SDL_FRect itemRect = {game.wWidth / 2 - (cInventorySize / 2.f - i) * tileSize * 3.f + tileSize / 2,game.wHeight - tileSize * 3.f + tileSize / 2, (float)tileSize, (float) tileSize};
			SDL_RenderTexture(game.renderer, game.tiles[(int) inventory[i].type], &tileFRect, &itemRect);
			char string[4];
			SDL_snprintf(string, sizeof(string), "%u", inventory[i].count);//TODO: max stack size (255)
			TTF_SetTextString(game.text, string, 0);
			int w = 0;
			TTF_MeasureString(game.font, string, 2, 0, &w, nullptr);
			TTF_DrawRendererText(game.text, itemRect.x+tileSize/2-w/2.f, itemRect.y + tileSize*3/2);
			
		}
	}
}

void Player::save(std::ofstream& out) const{
	write(out, &pos.x);
	write(out, &pos.y);
	write(out, &yVel);
	write(out, &onGround);
	write(out, &inventory);
	write(out, &selectedSlot);
	write(out, &breakDurability);
	write(out, &breakMaxDurability);
	write(out, &brokenBlock.x);
	write(out, &brokenBlock.y);
}

void Player::load(std::ifstream& in){
	read(in, &pos.x);
	read(in, &pos.y);
	read(in, &yVel);
	read(in, &onGround);
	read(in, &inventory);
	read(in, &selectedSlot);
	lastPlaceTicks = 0;
	read(in, &breakDurability);
	read(in, &breakMaxDurability);
	read(in, &brokenBlock.x);
	read(in, &brokenBlock.y);
}