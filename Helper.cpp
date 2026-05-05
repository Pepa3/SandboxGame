#include "Helper.h"

int SDL_RenderCircle(SDL_Renderer* renderer, float x, float y, int radius){
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while(offsety >= offsetx){
        status += SDL_RenderPoint(renderer, x + offsetx, y + offsety);
        status += SDL_RenderPoint(renderer, x + offsety, y + offsetx);
        status += SDL_RenderPoint(renderer, x - offsetx, y + offsety);
        status += SDL_RenderPoint(renderer, x - offsety, y + offsetx);
        status += SDL_RenderPoint(renderer, x + offsetx, y - offsety);
        status += SDL_RenderPoint(renderer, x + offsety, y - offsetx);
        status += SDL_RenderPoint(renderer, x - offsetx, y - offsety);
        status += SDL_RenderPoint(renderer, x - offsety, y - offsetx);

        if(status < 0){
            status = -1;
            break;
        }

        if(d >= 2 * offsetx){
            d -= 2 * offsetx + 1;
            offsetx += 1;
        } else if(d < 2 * (radius - offsety)){
            d += 2 * offsety - 1;
            offsety -= 1;
        } else{
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}

int SDL_RenderFillCircle(SDL_Renderer* renderer, float x, float y, int radius){
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while(offsety >= offsetx){

        status += SDL_RenderLine(renderer, x - offsety, y + offsetx,
            x + offsety, y + offsetx);
        status += SDL_RenderLine(renderer, x - offsetx, y + offsety,
            x + offsetx, y + offsety);
        status += SDL_RenderLine(renderer, x - offsetx, y - offsety,
            x + offsetx, y - offsety);
        status += SDL_RenderLine(renderer, x - offsety, y - offsetx,
            x + offsety, y - offsetx);

        if(status < 0){
            status = -1;
            break;
        }

        if(d >= 2 * offsetx){
            d -= 2 * offsetx + 1;
            offsetx += 1;
        } else if(d < 2 * (radius - offsety)){
            d += 2 * offsety - 1;
            offsety -= 1;
        } else{
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}


bool isSolid(Tile t){
    switch(t){
    case Tile::AIR:
    case Tile::LEAVES:
        return false;
    default:
        return true;
    }
}
bool hasBackground(Tile t){
    switch(t){
    case Tile::AIR:
        return true;
    default:
        return false;
    }
}
Tile destroyResult(Tile t, Tool l){
    if(l == Tool::HAND){
        switch(t){
        case Tile::DIRT:
        case Tile::GRASS:
            return Tile::DIRT;
        case Tile::WOOD:
            return Tile::WOOD;
        case Tile::SAND:
            return Tile::SAND;
        default:
            return Tile::AIR;
        }
    } else if(l == Tool::PICKAXE){
        switch(t){
        case Tile::DIRT:
        case Tile::GRASS:
            return Tile::DIRT;
        case Tile::SAND:
            return Tile::SAND;
        case Tile::COAL:
            return Tile::COAL;
        case Tile::GLOW:
            return Tile::GLOW;
        case Tile::STONE:
            return Tile::STONE;
        default:
            return Tile::AIR;
        }
    } else{
        assert(false && "Unknown tool");
        return Tile::UNKNOWN;
    }
}
int durability(Tile t){
    switch(t){
    case Tile::SAND:
        return 10;
    case Tile::GRASS:
    case Tile::DIRT:
    case Tile::WOOD:
        return 20;
    case Tile::STONE:
    case Tile::GLOW:
    case Tile::COAL:
        return 100;
    default:
        return 1;
    }
}