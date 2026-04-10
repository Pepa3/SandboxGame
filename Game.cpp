#define HELPER_INIT
#include "Helper.h"
#include <SDL3/SDL_main.h>
#include <iostream>
#include <cassert>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv){
	if(!SDL_SetAppMetadata("Game", "0.1", "me.pepa3.game")){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't set metadata: %s\n", SDL_GetError());
	}
	if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't initialize SDL context: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	if(!TTF_Init()){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't initialize SDL_ttf: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	font = TTF_OpenFont("Roboto-Regular.ttf",18.f);
	if(!font){
		SDL_Log("Couldn't open font: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	const SDL_DisplayID* id = SDL_GetDisplays(NULL);
	if(!id){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't get display info: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	const SDL_DisplayMode* dmode = SDL_GetDesktopDisplayMode(id[0]);
	if(!dmode){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't get display mode: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	wWidth = dmode->w;
	wHeight = dmode->h;
	if(!SDL_CreateWindowAndRenderer("Game", wWidth, wHeight, SDL_WINDOW_FULLSCREEN, &window, &renderer)){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't create window/renderer: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	if(!SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_ADAPTIVE)){
		SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Couldn't set SDL_RENDERER_VSYNC_ADAPTIVE: %s\n", SDL_GetError());
		SDL_SetRenderVSync(renderer, 1);
	}
	engine = TTF_CreateRendererTextEngine(renderer);
	if(!engine){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,"Couldn't create text engine: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	text = TTF_CreateText(engine, font, "", 0);
	if(!text){
		SDL_Log("Couldn't create text: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	TTF_SetTextColor(text, 255, 255, 255, SDL_ALPHA_OPAQUE);

	SDL_Surface* tilemap = IMG_Load(TILEMAP_PATH);
	if(tilemap==nullptr){
		SDL_LogWarn(SDL_LOG_CATEGORY_ERROR, "Couldn't load the tilemap: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	SDL_Log("Tilemap size %d:%d",tilemap->w,tilemap->h);
	assert(tilemap->w == tileSize * tileMapWidth && "Bad tilemap width.");
	assert(tilemap->h == tileSize * tileMapHeight && "Bad tilemap height.");

	SDL_SetSurfaceBlendMode(tilemap,SDL_BLENDMODE_NONE);

	SDL_Surface* temp = SDL_CreateSurface(32,32,SDL_PIXELFORMAT_RGBA8888);

	for(int i = 0; i < tileMapHeight; i++){
		for(int j = 0; j < tileMapWidth; j++){
			SDL_Rect srcRect = {tileSize * j,tileSize * i,tileSize,tileSize};
			if(!SDL_BlitSurface(tilemap, &srcRect, temp, &tileRect)){
				SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't process the tilemap: %s\n", SDL_GetError());
				return SDL_APP_FAILURE;
			}
			tiles[i * tileMapWidth + j] = SDL_CreateTextureFromSurface(renderer,temp);
			if(tiles[i * tileMapWidth + j]==nullptr){
				SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't upload tiles to GPU: %s\n", SDL_GetError());
				return SDL_APP_FAILURE;
			}
		}
	}
	SDL_DestroySurface(temp);
	SDL_DestroySurface(tilemap);

	map = new Map();
	if(!map->load("test1.save"))
	map->generateWorld();

	return SDL_APP_CONTINUE;
}
static uint64_t frames = 0, lastFPSTime = 0;

SDL_AppResult SDL_AppIterate(void* appstate){
	frames++;

	uint64_t t = SDL_GetTicks();
	if(t - lastUpdateTicks >= 25){//40 tps
		lastUpdateTicks=t;
		map->update();
	}

	SDL_SetRenderDrawColor(renderer, 200, 0, 0, 0xff);
	SDL_RenderClear(renderer);

	map->render();

	static char string[6];
	if(lastFPSTime + 1000 <= SDL_GetTicks()){
		SDL_snprintf(string, sizeof(string), "%.2f", frames/(float)(SDL_GetTicks()-lastFPSTime)*1000.f);
		lastFPSTime = SDL_GetTicks();
		frames = 0;
	}
	TTF_SetTextString(text, string, 0);
	TTF_DrawRendererText(text, 10, 10);

	SDL_RenderPresent(renderer);

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
	if(event->type == SDL_EVENT_QUIT){
		return SDL_APP_SUCCESS;
	} else if(event->type == SDL_EVENT_KEY_DOWN){
		char key = event->key.key;
		map->handleKeyDown(key);
		switch(key){
		case SDLK_ESCAPE:
			return SDL_APP_SUCCESS;
			break;
		default:
			break;
		}
	} else if(event->type == SDL_EVENT_KEY_UP){
	} else if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN){
		if(event->button.button == SDL_BUTTON_LEFT){
		} else if(event->button.button == SDL_BUTTON_RIGHT){
		}
	} else if(event->type == SDL_EVENT_MOUSE_WHEEL){
		map->handleMouseWheel(event->wheel);
	}
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result){
	map->save("test1.save");
	delete map;

	TTF_Quit();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
