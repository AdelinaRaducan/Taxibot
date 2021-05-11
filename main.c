#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GL2_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_sdlrenderer.h"
#include "overview.c"
#include "aStar.h"

#define forever while(1)

const int SCREEN_WIDTH_PIXELS = 1280;
const int SCREEN_HEIGHT_PIXELS = 720;
const int TILE_SIZE_PIXELS = 16;

static struct {
	SDL_Window *Handler;
	SDL_Renderer *Renderer;
	struct nk_context *Nuklear;
} WindowManager;

enum tile_type {
	ROAD_TILE,
	TOWER_TILE
};

typedef struct tile {
	enum tile_type Type;
} tile;

typedef struct tilemap {
	tile *Tiles;
	int Width, Height;
} tilemap;

typedef struct game_state {
	tilemap Tilemap;
	astar_grid *AStarGrid;
} game_state;


void CreateWindow(int Width, int Height);
void DestroyWindow();
game_state * CreateGameState(is_open_cell_function function);
void DestroyGameState(game_state *GameState);

void UpdateAndRenderPlay(game_state *GameState);
void HandleInput(game_state *GameState);
void Update(game_state *GameState);
void Draw(game_state *GameState);
void DrawGUI(game_state *GameState);
void StartDrawing();
void EndDrawing();
void DrawTilemap(tilemap *Tilemap);
bool IsOpenCellFunction(point Location, void *AStarGrid);

int main( int argc, char* args[] ) {
	
	CreateWindow(SCREEN_WIDTH_PIXELS, SCREEN_HEIGHT_PIXELS);

	game_state *GameState = CreateGameState(IsOpenCellFunction);

	UpdateAndRenderPlay(GameState);

	DestroyGameState(GameState);
	DestroyWindow();
	return 0;
}

void CreateWindow(int Width, int Height)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Could not initialize SDL!\n");
		exit(-1);
	}

	WindowManager.Handler = SDL_CreateWindow( "SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, 0);
	if (!WindowManager.Handler) {
		printf("Could not create a window!\n");
		exit(-1);
	}

	WindowManager.Renderer = SDL_CreateRenderer(WindowManager.Handler, 0, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
	if (!WindowManager.Renderer) {
		printf("Could not create a renderer!\n");
		exit(-1);
	}

	WindowManager.Nuklear = nk_sdl_init(WindowManager.Handler, WindowManager.Renderer);
    {
    	struct nk_font_atlas *atlas;
    	nk_sdl_font_stash_begin(&atlas);
	    nk_sdl_font_stash_end();
    }
}

void DestroyWindow()
{
	nk_sdl_shutdown();
	SDL_DestroyRenderer(WindowManager.Renderer);
	SDL_DestroyWindow(WindowManager.Handler);
	SDL_Quit();
}

void UpdateAndRenderPlay(game_state *GameState)
{
	forever {
		HandleInput(GameState);
		Update(GameState);
        Draw(GameState);
	}
}

void HandleInput(game_state *GameState)
{
	nk_input_begin(WindowManager.Nuklear);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			DestroyGameState(GameState);
            DestroyWindow();
			exit(0);
        }
        nk_sdl_handle_event(&event);
	}
	nk_input_end(WindowManager.Nuklear);
}

void Update(game_state *GameState)
{
	// Find Path
}

void Draw(game_state *GameState)
{
	StartDrawing();
	{
		DrawTilemap(&GameState->Tilemap);
		// Draw Path
		DrawGUI(GameState);
	}
	EndDrawing();
}

void DrawGUI(game_state *GameState)
{
	overview(WindowManager.Nuklear);
}

void StartDrawing()
{
	SDL_SetRenderDrawColor(WindowManager.Renderer, 255, 255, 0, 255);
    SDL_RenderClear(WindowManager.Renderer);
}

void EndDrawing()
{
	nk_sdl_render(NK_ANTI_ALIASING_ON);
	SDL_RenderPresent(WindowManager.Renderer);
}

void DrawLine(int x1, int y1, int x2, int y2)
{
	lineRGBA(WindowManager.Renderer, x1, y1, x2, y2, 0, 0, 0, 255);
}

void DrawTilemap(tilemap *Tilemap)
{
	for (int i = 0; i < Tilemap->Width; ++i)
	{
		DrawLine(i * TILE_SIZE_PIXELS, 0, i * TILE_SIZE_PIXELS, SCREEN_HEIGHT_PIXELS);
	}

	for (int i = 0; i < Tilemap->Height; ++i)
	{
		DrawLine(0, i * TILE_SIZE_PIXELS, SCREEN_WIDTH_PIXELS, i * TILE_SIZE_PIXELS);
	} 
 
}

game_state * CreateGameState(is_open_cell_function function)
{	

	game_state *GameState = (game_state *) malloc (sizeof(game_state));
	GameState->Tilemap.Width = SCREEN_WIDTH_PIXELS / TILE_SIZE_PIXELS;
	GameState->Tilemap.Height = SCREEN_HEIGHT_PIXELS / TILE_SIZE_PIXELS;
	GameState->Tilemap.Tiles = (tile *) malloc (sizeof(tile) * GameState->Tilemap.Width * GameState->Tilemap.Height);

	GameState->AStarGrid = malloc(sizeof(astar_grid));
	GameState->AStarGrid->NumberRows = GameState->Tilemap.Height;
	GameState->AStarGrid->NumberCols = GameState->Tilemap.Width;
	GameState->AStarGrid->IsOpenCellFunction = function;
	GameState->AStarGrid->Map = malloc(GameState->AStarGrid->NumberRows * sizeof(cell*));

	for (int i = 0; i < GameState->AStarGrid->NumberRows; i++) {
		GameState->AStarGrid->Map[i] = calloc(GameState->AStarGrid->NumberCols, sizeof(cell));
	}

	int k = 0;
	for (int i = 0; i < GameState->AStarGrid->NumberRows; i++) {
		for (int j = 0; j < GameState->AStarGrid->NumberCols; j++) {
			GameState->Tilemap.Tiles[k++].Type = ROAD_TILE;
			GameState->AStarGrid->Map[i][j].MovementCost = 1;
			GameState->AStarGrid->Map[i][j].f = -1;
		}
	} 

	return GameState;
} 

void DestroyGameState(game_state *GameState)
{
	free(GameState->Tilemap.Tiles);
	// for (int i = 0; i < GameState->AStarGrid->NumberRows; i++) {
	// 	free(GameState->AStarGrid->Map[i]);
	// }
	// free(GameState->AStarGrid->Map);
	free(GameState);
}

bool IsOpenCellFunction(point Location, void *AStarGrid) {
	if ((((astar_grid*)(AStarGrid))->Map[Location.Row][Location.Col].MovementCost) == 1) {
		return true;
	} else {
		return false;
	}	
}


// calculatePath(random start node, random end node)
// drawPath(path *)




