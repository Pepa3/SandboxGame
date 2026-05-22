#define HELPER_INIT
#include "Helper.h"
#include <SDL3/SDL_main.h>

static GameState game{};
static double lastFPS = 0;
static uint64_t frames = 0, time1 = 0;
void AppUpdate(std::stop_token stoken);

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
	game.font = FC_CreateFont();
	if(game.font == nullptr){
		SDL_Log("Couldn't create font: %s\n", SDL_GetError());
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
	SDL_SetRenderVSync(game.renderer, 1);
	uint8_t code = 0;
	if(!(code = FC_LoadFont(game.font, game.renderer, "Roboto-Regular.ttf", 18, FC_MakeColor(0xff, 0xff, 0xff, 0xff), TTF_STYLE_NORMAL))){
		SDL_Log("Couldn't open font: %u: %s\n", code, SDL_GetError());
		return SDL_APP_FAILURE;
	}

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

	for(size_t i = 0; i < tileMapHeight; i++){
		for(size_t j = 0; j < tileMapWidth; j++){
			SDL_Rect srcRect = {tileSize * (int)j,tileSize * (int)i,tileSize,tileSize};
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
	if(!game.map->load("test1.save"))
	game.map->generateWorld();

	game.updateThread = std::jthread(AppUpdate);

	return SDL_APP_CONTINUE;
}

void AppUpdate(std::stop_token stoken){
	while(!stoken.stop_requested()){
		const auto t = std::chrono::high_resolution_clock::now();
		game.map->update();
		const auto t2 = std::chrono::high_resolution_clock::now();
		/*if(t2 > t + std::chrono::milliseconds(1000 / 40)){
			std::cerr << "Update took too long: " << (t2-t).count()/1000000 << "ms\n";
		}*/
		std::this_thread::sleep_until(t+std::chrono::milliseconds(1000/40));
	}
}

SDL_AppResult SDL_AppIterate(void* appstate){
	frames++;
	uint64_t time2 = SDL_GetTicksNS();
	if(time1+1e9<time2){
		lastFPS = (double)frames/(time2 - time1)*1e9;
		time1 = time2;
		frames = 0;
	}
	SDL_SetRenderDrawColor(game.renderer, 111, 255, 226, 0xff);
	SDL_SetRenderDrawBlendMode(game.renderer,SDL_BLENDMODE_BLEND);
	SDL_RenderClear(game.renderer);

	game.map->render();
	FC_Draw(game.font, game.renderer, 10, 10, "FPS:%.2f LU:%d CG:%d", lastFPS, game.countLightUpdates, game.countChunkGen);

	SDL_RenderPresent(game.renderer);

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
	if(event->type == SDL_EVENT_QUIT){
		return SDL_APP_SUCCESS;
	} else if(event->type == SDL_EVENT_KEY_DOWN){
		const char key = event->key.key;
		switch(key){
		case SDLK_Q://TODO: exit screen/confirmation
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
			game.player->handleKeyDown(key);
			break;
		}
	} else if(event->type == SDL_EVENT_KEY_UP){
	} else if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN){
		game.player->handleMouseDown(event->button);
	} else if(event->type == SDL_EVENT_MOUSE_BUTTON_UP){
		game.player->handleMouseUp(event->button);
	} else if(event->type == SDL_EVENT_MOUSE_MOTION){
		game.player->handleMouseMotion(event->motion);
	} else if(event->type == SDL_EVENT_MOUSE_WHEEL){
		game.player->handleMouseWheel(event->wheel);
	}
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result){
	game.updateThread.request_stop();
	if(game.map){
		game.map->save("test1.save");
	}
	FC_FreeFont(game.font);
	TTF_Quit();
	SDL_DestroyRenderer(game.renderer);
	SDL_DestroyWindow(game.window);
	SDL_Quit();
}
