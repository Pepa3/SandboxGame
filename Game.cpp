#define HELPER_INIT
#include "Helper.h"
#include <SDL3/SDL_main.h>

static GameState game;
static uint64_t frames = 0, lastFPSTime = 0, lastUpdateTicks = 0;

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
	game.font = TTF_OpenFont("Roboto-Regular.ttf",18.f);
	if(!game.font){
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
	game.wWidth = dmode->w;
	game.wHeight = dmode->h;
	if(!SDL_CreateWindowAndRenderer("Game", game.wWidth, game.wHeight, SDL_WINDOW_FULLSCREEN, &game.window, &game.renderer)){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't create window/renderer: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	if(!SDL_SetRenderVSync(game.renderer, SDL_RENDERER_VSYNC_ADAPTIVE)){
		SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Couldn't set SDL_RENDERER_VSYNC_ADAPTIVE: %s\n", SDL_GetError());
		SDL_SetRenderVSync(game.renderer, 1);
	}
	game.engine = TTF_CreateRendererTextEngine(game.renderer);
	if(!game.engine){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,"Couldn't create text engine: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	game.text = TTF_CreateText(game.engine, game.font, "", 0);
	if(!game.text){
		SDL_Log("Couldn't create text: %s\n", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	TTF_SetTextColor(game.text, 255, 255, 255, SDL_ALPHA_OPAQUE);

	SDL_Surface* tilemap = IMG_Load(cTilemapPath);
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
			game.tiles[i * tileMapWidth + j] = SDL_CreateTextureFromSurface(game.renderer,temp);
			if(game.tiles[i * tileMapWidth + j]==nullptr){
				SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't upload tiles to GPU: %s\n", SDL_GetError());
				return SDL_APP_FAILURE;
			}
		}
	}
	SDL_DestroySurface(temp);
	SDL_DestroySurface(tilemap);

	game.map = std::make_unique<Map>(game);
	//if(!game.map->load("test1.save"))
	game.map->generateWorld();

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate){
	frames++;

	const uint64_t t = SDL_GetTicks();
	if(t - lastUpdateTicks >= minUpdateTimeMillis){
		lastUpdateTicks=t;
		game.map->update();
	}

	SDL_SetRenderDrawColor(game.renderer, 111, 255, 226, 0xff);
	SDL_SetRenderDrawBlendMode(game.renderer,SDL_BLENDMODE_BLEND);
	SDL_RenderClear(game.renderer);

	game.map->render();
	//"\0" "FPS:" "60.00" " LU:" "9999" " CG:" "999"
	//[sizeof("FPS:60.00 LU:1000 CG:300\0")]
	static char string[sizeof("FPS:60.00 LU:1000 CG:300\0")];
	if(lastFPSTime + 1000 <= SDL_GetTicks()){
		SDL_snprintf(string, sizeof(string), "FPS:%.2f LU:%d CG:%d", frames / (double) (SDL_GetTicks() - lastFPSTime) * 1000.f, game.countLightUpdates, game.countChunkGen);
		lastFPSTime = SDL_GetTicks();
		frames = 0;
	}
	TTF_SetTextString(game.text, string, 0);
	TTF_DrawRendererText(game.text, 10, 10);

	SDL_RenderPresent(game.renderer);

	return SDL_APP_CONTINUE;
}
#pragma warning(suppress: 26461)
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
	if(event->type == SDL_EVENT_QUIT){
		return SDL_APP_SUCCESS;
	} else if(event->type == SDL_EVENT_KEY_DOWN){
		const char key = event->key.key;
		//game.map.handleKeyDown(key);
		switch(key){
		case SDLK_ESCAPE:
			return SDL_APP_SUCCESS;
			break;
		case SDLK_V:
			game.overlayFluid = !game.overlayFluid;
			break;
		case SDLK_B:
			game.overlayLight = !game.overlayLight;
			break;
		case SDLK_C:
			game.debugMode = !game.debugMode;
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
		game.map->handleMouseWheel(event->wheel);
	}
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result){
	game.map->save("test1.save");

	TTF_Quit();
	SDL_DestroyRenderer(game.renderer);
	SDL_DestroyWindow(game.window);
	SDL_Quit();
}
