#include "Helper.h"

//TODO: refactor the entire movement code for entities and player
void Entity::update(const GameState& game){
	posTile m = pos;
	bool goingRight = false, goingLeft = false, goingUp = true, goingDown = false;
	if(pos.x < game.player->pos.x)goingRight = true;
	else if(pos.x > game.player->pos.x)goingLeft = true;
	const float cEntitySpeed = cPlayerSpeed/2;

	Tile tdr = game.map->world(m.down().right()).t;
	// on ground if both blocks under player are solid unless standing exactly on tile, then not on ground
	bool onGround = isSolid(game.map->world(m.down()).t) || (m.x - (pos.x / tileSize) != 0.f && isSolid(tdr));

	//can move if not going to be blocked and in air not blocked by both tiles
	if(goingRight){
		if(!isSolid(game.map->world((int) floorf((pos.x + cEntitySpeed) / tileSize) + 1, m.y).t)
			&& !(!onGround && isSolid(game.map->world((int) floorf((pos.x + cEntitySpeed) / tileSize) + 1, m.y + 1).t))
			){
			pos.x += cEntitySpeed;
		} else if(!isSolid(game.map->world(m.right()).t) //can snap?
			&& !(!onGround && isSolid(tdr))
			){
			pos.x = pos.x + (m.x - (pos.x / tileSize)) * tileSize + tileSize;
		}
	} else if(goingLeft){
		if(!isSolid(game.map->world((int) floorf((pos.x - cEntitySpeed) / tileSize), m.y).t)
			&& !(!onGround && isSolid(game.map->world((int) floorf((pos.x - cEntitySpeed) / tileSize), m.y + 1).t))
			){
			pos.x -= cEntitySpeed;
		} else{
			pos.x = pos.x + (m.x - (pos.x / tileSize)) * tileSize;
		}
	}
	m.x = (int) floorf(pos.x / tileSize);

	float distanceToTileBoundary = (m.y - pos.y / tileSize) * tileSize;
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
	distanceToTileBoundary = 32 + (m.y - pos.y / tileSize) * tileSize;

	const Block& b = game.map->world(m.x, m.y);

	if(!onGround){// IN AIR (or water)
		yVel += cGravity;
		if(b.fluid){
			yVel = cSinkRate;
			if(goingUp){
				yVel = -cJumpImpulse;
			} else if(goingDown){
				yVel = cJumpImpulse;
			}
		}
		if(mayBeOnGround && distanceToTileBoundary < yVel)yVel = distanceToTileBoundary;
	} else{// ON GROUND
		yVel = 0;
		if(goingUp){
			yVel = -cJumpImpulse;
		}
	}
}

void Entity::render(const GameState& game)const{
	const SDL_FRect dest = {pos.x - game.camera.x + game.wWidth / 2, pos.y - game.camera.y + game.wHeight / 2, tileSize, tileSize};
	SDL_RenderTexture(game.renderer, game.tiles[(int) Tile::PLAYER], &tileFRect, &dest);
}

void Entity::save(std::ofstream& out) const{
	write(out, &pos.x);
	write(out, &pos.y);
	write(out, &yVel);
}

void Entity::load(std::ifstream& in){
	read(in, &pos.x);
	read(in, &pos.y);
	read(in, &yVel);
}