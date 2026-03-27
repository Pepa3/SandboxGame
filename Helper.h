#pragma once
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>

int SDL_RenderCircle(SDL_Renderer* renderer, int x, int y, int radius);
int SDL_RenderFillCircle(SDL_Renderer* renderer, int x, int y, int radius);