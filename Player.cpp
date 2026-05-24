#include "Helper.h"
#include <iostream>

Player::Player(GameState& game, posWorld p) :pos(p), game(game){}

bool Player::addInventory(Tile t){
	for(uint8_t i = 0; i < cHotbarSize; i++){
		if(hotbar[i].type == t){
			if(hotbar[i].count < cItemStackSize){
				hotbar[i].count++;
				return true;
			} else{
				continue;
			}
		}
	}
	for(uint8_t i = 0; i < cHotbarSize; i++){
		if(hotbar[i].type == Tile::UNKNOWN || hotbar[i].count == 0){
			hotbar[i] = {t,1};
			return true;
		}
	}
	for(uint8_t i = 0; i < cInventorySize; i++){
		ItemSlot& slot = inventory[i];
		if(slot.type == t || slot.type == Tile::UNKNOWN){
			if(slot.count < cItemStackSize){
				slot.type = t;
				slot.count++;
				return true;
			} else{
				continue;
			}
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
	if(!openInventory){
		if(button & SDL_BUTTON_RMASK){
			if(lastPlaceTicks <= SDL_GetTicks() - cPlaceTimeoutMillis){
				if(hotbar[selectedSlot].count != 0){
					if(game.map->place(tx, ty, hotbar[selectedSlot].type)){
						lastPlaceTicks = SDL_GetTicks();
						hotbar[selectedSlot].count--;
						game.map->updateLight(tx, ty);
						breakMaxDurability = durability(hotbar[selectedSlot].type);
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
					game.map->destroy(tx, ty, Tool::HAND);
				}
			}
		}
	}
	posTile m = pos;
	Tile tdr = game.map->world(m.down().right()).t;
	// on ground if both blocks under player are solid unless standing exactly on tile, then not on ground
	onGround = isSolid(game.map->world(m.down()).t) || (m.x-(pos.x/tileSize) != 0.f && isSolid(tdr));

	//can move if not going to be blocked and in air not blocked by both tiles

	if(key_states[SDL_SCANCODE_D]){
		if(!isSolid(game.map->world((int) floorf((pos.x + cPlayerSpeed) / tileSize) + 1, m.y).t)
			&& !(!onGround && isSolid(game.map->world((int) floorf((pos.x + cPlayerSpeed) / tileSize) + 1, m.y + 1).t))
			){
			pos.x += cPlayerSpeed;
		} else if(!isSolid(game.map->world(m.right()).t) //can snap?
			&& !(!onGround && isSolid(tdr))
			){
			pos.x = pos.x + (m.x - (pos.x / tileSize)) * tileSize + tileSize;
		}
	} else if(key_states[SDL_SCANCODE_A]){
		if(!isSolid(game.map->world((int) floorf((pos.x - cPlayerSpeed) / tileSize), m.y).t)
			&& !(!onGround && isSolid(game.map->world((int) floorf((pos.x - cPlayerSpeed) / tileSize), m.y + 1).t))
			){
			pos.x -= cPlayerSpeed;
		} else{
			pos.x = pos.x + (m.x - (pos.x / tileSize))*tileSize;
		}
	}
	m.x = (int) floorf(pos.x / tileSize);

	float distanceToTileBoundary = (m.y - pos.y / tileSize)*tileSize;
	if(!isSolid(game.map->world(m.x, (int) floorf((pos.y + yVel) / tileSize)).t)
		&& !(m.x - (pos.x / tileSize) != 0.f && isSolid(game.map->world(m.x + 1, (int) floorf((pos.y + yVel) / tileSize)).t))){
		pos.y += yVel;
	} else{
		yVel = distanceToTileBoundary;
	}

	m.y = (int) floorf(pos.y / tileSize);
	onGround = isSolid(game.map->world(m.x, m.y + 1).t) || (m.x - (pos.x / tileSize) != 0.f && isSolid(game.map->world(m.x + 1, m.y + 1).t));
	const bool mayBeOnGround = isSolid(game.map->world(m.x, m.y + 2).t) || 
		(m.x - (pos.x / tileSize) != 0.f && isSolid(game.map->world(m.x + 1, m.y + 2).t));
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

void Player::handleMouseWheel(SDL_MouseWheelEvent event){
	const bool dir = event.integer_y < 0;
	if(dir){
		selectedSlot++;
		if(selectedSlot >= cHotbarSize) selectedSlot = 0;
	} else{
		selectedSlot--;
		if(selectedSlot < 0)selectedSlot = cHotbarSize - 1;
	}
}

void Player::handleKeyDown(char key){
	if(key == SDLK_I){
		openInventory = !openInventory;
	} else if(key == SDLK_ESCAPE){
		openInventory = false;
	}
}

void Player::handleMouseDown(SDL_MouseButtonEvent e){
	if(openInventory){
		for(uint8_t row = 0; row < cInventorySize / cHotbarSize; row++){
			for(uint8_t i = 0; i < cHotbarSize; i++){
				const SDL_FRect itemFrameRect = {game.wWidth / 2 - (cHotbarSize / 2.f - i) * tileSize * 3.f,
					game.wHeight / 2 - tileSize * 3.f + row * tileSize * 3,tileSize * 2.f,tileSize * 2.f};
				if(e.x >= itemFrameRect.x && e.y >= itemFrameRect.y &&
					e.x <= itemFrameRect.x + itemFrameRect.w && e.y <= itemFrameRect.y + itemFrameRect.h){
					ItemSlot& slot = inventory[i + row * cHotbarSize];
					if(holding.type == Tile::UNKNOWN && slot.type == Tile::UNKNOWN)continue;
					if(slot.type == holding.type){
						uint8_t cc = slot.count + holding.count;
						if(cc <= cItemStackSize){
							slot.count = cc;
							holding.count = 0;
							holding.type = Tile::UNKNOWN;
						} else{
							slot.count = 10;
							holding.count = cc - cItemStackSize;
						}
					} else{
						std::swap(slot, holding);
					}
					return;
				}
			}
		}
		for(uint8_t i = 0; i < cHotbarSize; i++){
			const SDL_FRect itemFrameRect = {game.wWidth / 2 - (cHotbarSize / 2.f - i) * tileSize * 3.f,
				game.wHeight - tileSize * 3.f,tileSize * 2.f,tileSize * 2.f};
			if(e.x >= itemFrameRect.x && e.y >= itemFrameRect.y &&
				e.x <= itemFrameRect.x + itemFrameRect.w && e.y <= itemFrameRect.y + itemFrameRect.h){
				ItemSlot& slot = hotbar[i];
				if(holding.type == Tile::UNKNOWN && slot.type == Tile::UNKNOWN)continue;
				if(slot.type == holding.type){
					uint8_t cc = slot.count + holding.count;
					if(cc <= cItemStackSize){
						slot.count = cc;
						holding.count = 0;
						holding.type = Tile::UNKNOWN;
					} else{
						slot.count = 10;
						holding.count = cc - cItemStackSize;
					}
				} else{
					std::swap(slot, holding);
				}
				return;
			}
		}
	}
}

void Player::handleMouseUp(SDL_MouseButtonEvent e){
}

void Player::handleMouseMotion(SDL_MouseMotionEvent e){
}

void Player::renderInventory()const{
	const SDL_FRect bg = {game.wWidth / 2 - cHotbarSize / 2.f * tileSize * 3.f - tileSize / 2.f,
		game.wHeight / 2 - tileSize * 3.f - tileSize / 2.f, tileSize * cHotbarSize * 3.f, tileSize * cInventorySize / cHotbarSize * 3.f};
	SDL_SetRenderDrawColor(game.renderer, 0xaa, 0xaa, 0xaa, 0x88);
	SDL_RenderFillRect(game.renderer, &bg);
	for(uint8_t row = 0; row < cInventorySize / cHotbarSize; row++){
		for(uint8_t i = 0; i < cHotbarSize; i++){
			const SDL_FRect itemFrameRect = {game.wWidth / 2 - (cHotbarSize / 2.f - i) * tileSize * 3.f,
				game.wHeight / 2 - tileSize * 3.f + row * tileSize * 3,tileSize * 2.f,tileSize * 2.f};
			SDL_SetRenderDrawColor(game.renderer, 0xff, 0xff, 0xff, 0xff);
			SDL_RenderRect(game.renderer, &itemFrameRect);
			const ItemSlot& slot = inventory[i + row * cHotbarSize];
			if(slot.type != Tile::UNKNOWN && slot.count != 0){
				const SDL_FRect itemRect = {game.wWidth / 2 - (cHotbarSize / 2.f - i) * tileSize * 3.f + tileSize / 2,
					game.wHeight/2 - tileSize * 3.f + row * tileSize * 3 + tileSize / 2, (float) tileSize, (float) tileSize};
				SDL_RenderTexture(game.renderer, game.tiles[(int) slot.type], &tileFRect, &itemRect);
				FC_Draw(game.font, game.renderer, itemRect.x + tileSize / 3, itemRect.y + tileSize * 3 / 2, "%u", slot.count);
			}
		}
	}
	if(holding.count > 0){
		float mx, my;
		SDL_GetMouseState(&mx,&my);
		const SDL_FRect itemRect = {mx, my, (float) tileSize, (float) tileSize};
		SDL_RenderTexture(game.renderer, game.tiles[(int) holding.type], &tileFRect, &itemRect);
		FC_Draw(game.font, game.renderer, itemRect.x + tileSize / 3, itemRect.y + tileSize * 3 / 2, "%u", holding.count);
	}
}

void Player::render()const{
	const SDL_FRect dest = {pos.x - game.camera.x + game.wWidth / 2, pos.y - game.camera.y + game.wHeight / 2, tileSize, tileSize};
	SDL_RenderTexture(game.renderer, game.tiles[(int) Tile::PLAYER], &tileFRect, &dest);
	const SDL_FRect breakBlk = {game.map->posX(brokenBlock.x),game.map->posY(brokenBlock.y),tileSize,tileSize};
	const char p = (char) (((float) breakDurability / (float) breakMaxDurability) * 0xff);
	SDL_SetRenderDrawColor(game.renderer,0,0,0,p);
	SDL_RenderFillRect(game.renderer, &breakBlk);

	for(uint8_t i = 0; i < cHotbarSize; i++){
		const SDL_FRect itemFrameRect = {game.wWidth / 2 - (cHotbarSize / 2.f - i) * tileSize * 3.f,
			game.wHeight - tileSize * 3.f,tileSize * 2.f,tileSize * 2.f};
		SDL_SetRenderDrawColor(game.renderer, 0xff, 0xff, 0xff, 0xff);
		SDL_RenderRect(game.renderer, &itemFrameRect);
		if(i == selectedSlot){
			const SDL_FRect itemSelRect = {game.wWidth / 2 - (cHotbarSize / 2.f - i) * tileSize * 3.f+2,
				game.wHeight - tileSize * 3.f+2,tileSize * 2.f-4,tileSize * 2.f-4};
			SDL_RenderRect(game.renderer, &itemSelRect);
		}
		if(hotbar[i].type != Tile::UNKNOWN && hotbar[i].count != 0){
			const SDL_FRect itemRect = {game.wWidth / 2 - (cHotbarSize / 2.f - i) * tileSize * 3.f + tileSize / 2,
				game.wHeight - tileSize * 3.f + tileSize / 2, (float)tileSize, (float) tileSize};
			SDL_RenderTexture(game.renderer, game.tiles[(int) hotbar[i].type], &tileFRect, &itemRect);
			FC_Draw(game.font, game.renderer, itemRect.x+tileSize/3, itemRect.y + tileSize*3/2, "%u", hotbar[i].count);
		}
	}
	if(openInventory){
		renderInventory();
	} else{
		float mx, my;
		SDL_GetMouseState(&mx, &my);
		const int tx = game.map->tPosX(mx);
		const int ty = game.map->tPosY(my);

		const SDL_FRect cursorRect = {game.map->posX(tx), game.map->posY(ty),tileSize,tileSize};
		SDL_SetRenderDrawColor(game.renderer, 0, 0, 0, 0xff);
		SDL_RenderRect(game.renderer, &cursorRect);
		if(game.debugMode){
			FC_Draw(game.font,game.renderer,mx,my-18,"%d:%d:%s",tx,ty,biomeName(game.map->getBiome(posTile(tx,ty))));
		}
	}
}

void Player::save(std::ofstream& out) const{
	write(out, &pos.x);
	write(out, &pos.y);
	write(out, &yVel);
	write(out, &onGround);
	write(out, &hotbar);
	write(out, &inventory);
	write(out, &holding);
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
	read(in, &hotbar);
	for(uint8_t i = 0; i < cHotbarSize; i++){
		if(hotbar[i].count == 0)hotbar[i].type = Tile::UNKNOWN;
	}
	read(in, &inventory);
	for(uint8_t i = 0; i < cInventorySize; i++){
		if(inventory[i].count == 0)inventory[i].type = Tile::UNKNOWN;
	}
	read(in, &holding);
	if(holding.count == 0)holding.type = Tile::UNKNOWN;
	read(in, &selectedSlot);
	lastPlaceTicks = 0;
	read(in, &breakDurability);
	read(in, &breakMaxDurability);
	read(in, &brokenBlock.x);
	read(in, &brokenBlock.y);
}