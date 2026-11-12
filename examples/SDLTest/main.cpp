#include <iostream>
#include <SDL3/SDL.h>

int main(){
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return 1;
	}

	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	if (!SDL_CreateWindowAndRenderer("SDL3 Example", 800, 600, 0, &window, &renderer)) {
		SDL_Log("CreateWindowAndRenderer failed: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	bool running = true;
	SDL_Event event;
	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}

		SDL_SetRenderDrawColor(renderer, 30, 60, 120, 255);
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
