#include "Helper.h"

void Item::update(const GameState& game){
	if(pickedUp)return;
	if(pos.dist(game.player->pos + posWorld{tileSize/2.f,tileSize/2.f}) < tileSize*2){
		if(game.player->addInventory(t)){
			pickedUp = true;
		}
	}
	if(!game.map->isSolid(pos.down(itemSize))){
		yVel += cGravity;
	} else{
		yVel = 0;
	}
	pos.y += yVel;
}

void Item::render(const GameState& game)const{
	if(pickedUp)return;
	posWorld p = pos - game.camera;
	const SDL_FRect itemRect = {p.x + game.wWidth / 2.f, p.y + game.wHeight / 2.f, itemSize, itemSize};
	SDL_RenderTexture(game.renderer, game.tiles[(int)t], &tileFRect, &itemRect);
}
