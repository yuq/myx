#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

int main ( int argc, char *argv[] )
{
	/* initialize SDL */
	SDL_Init(SDL_INIT_VIDEO);

	/* create window */
	SDL_Surface* screen = SDL_SetVideoMode(1600, 1200, 32, SDL_FULLSCREEN);

	sleep(5);

	/* cleanup SDL */
	SDL_Quit();

	return 0;
}
