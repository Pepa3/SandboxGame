#include <iostream>
#include "Helper.h"
#include <SDL3/SDL_main.h>

SDL_Window* window;
SDL_Renderer* renderer;
int wWidth, wHeight;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv){
	if(!SDL_SetAppMetadata("Game", "0.1", "me.pepa3.game")){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't set metadata: %s", SDL_GetError());
	}
	if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't initialize SDL context: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	const SDL_DisplayID* id = SDL_GetDisplays(NULL);
	if(!id){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't get display info: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	const SDL_DisplayMode* dmode = SDL_GetDesktopDisplayMode(id[0]);
	if(!dmode){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't get display mode: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	wWidth = dmode->w;
	wHeight = dmode->h;
	if(!SDL_CreateWindowAndRenderer("Game", wWidth, wHeight, SDL_WINDOW_FULLSCREEN, &window, &renderer)){
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't create window/renderer: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	if(!SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_ADAPTIVE)){
		SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Couldn't set SDL_RENDERER_VSYNC_ADAPTIVE: %s", SDL_GetError());
		SDL_SetRenderVSync(renderer, 1);
	}
	return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppIterate(void* appstate){
	SDL_SetRenderDrawColor(renderer, 200, 0, 0, 0xff);
	SDL_RenderClear(renderer);

	SDL_RenderPresent(renderer);
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
	if(event->type == SDL_EVENT_QUIT){
		return SDL_APP_SUCCESS;
	} else if(event->type == SDL_EVENT_KEY_DOWN){
		char key = event->key.key;
		switch(key){
		case SDLK_ESCAPE:
			return SDL_APP_SUCCESS;
			break;
		}
	} else if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN){
		if(event->button.button == SDL_BUTTON_LEFT){
		} else if(event->button.button == SDL_BUTTON_RIGHT){
		}
	}
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result){
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
