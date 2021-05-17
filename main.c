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
#include <time.h>

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
#include "aStar.c"

#define forever while(1)
#define MS_PER_FRAME 16

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

typedef struct taxi {
	double X;
	double Y;
	double Speed;
	int Direction;
} taxi;

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
	srand(time(NULL));
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

int GetTaxiDirection(point Start, point End) {
	if (End.Row - Start.Row > 0) {
		return 1;
	} else if (End.Row - Start.Row < 0) {
		return -1;
	} else {
		if (End.Col - Start.Col > 0) {
			return 1;
		} else {
			return -1;
		}
	}
}

Tstack *Path = NULL;
Tstack *TaxiPath = NULL;
taxi Taxi = {};
void Update(game_state *GameState)
{
	if (TaxiPath == NULL) {
		point Start = {rand() % (GameState->AStarGrid->NumberRows + 1), rand() % (GameState->AStarGrid->NumberCols + 1)};
		point End = {rand() % (GameState->AStarGrid->NumberRows + 1), rand() % (GameState->AStarGrid->NumberCols + 1)};
		
		// point Start = {33, 43};
		// point End = {44, 33};

		DestroyStack(&Path);
		Path = FindPath(Start, End, GameState->AStarGrid);
		// PrintStack(&Path);
		ClonePath();
	}

	if (TaxiPath != NULL) {
		cell Cell = PopStack(&TaxiPath)->Data;
		Taxi.X = Cell.Location.Row; 
		Taxi.Y = Cell.Location.Col;
	}
}

void DrawRectangle(SDL_FRect r, int R, int G, int B)
{	
    r.y = SCREEN_HEIGHT_PIXELS - r.y - TILE_SIZE_PIXELS;
    SDL_SetRenderDrawColor( WindowManager.Renderer, R, G, B, 255 );
    SDL_RenderFillRectF( WindowManager.Renderer, &r );
}

void DrawPath(Tstack **FoundPath)
{	
	Tstack *temp = *FoundPath;
	while (temp != NULL) {
		cell Cell = temp->Data;
		SDL_FRect r = {.x = TILE_SIZE_PIXELS * Cell.Location.Col, 
					   .y = TILE_SIZE_PIXELS * Cell.Location.Row, 
					   .w = TILE_SIZE_PIXELS, .h = TILE_SIZE_PIXELS};
		DrawRectangle(r, 0, 0, 205);	
		temp = temp->next;
	}
}

void ClonePath() 
{
	Tstack *temp = NULL;;
	while (!IsStackEmpty(Path)) {
		cell Cell = PopStack(&Path)->Data;
		PushStack(&temp, Cell);
	}

	while (!IsStackEmpty(temp)) {
		cell Cell = PopStack(&temp)->Data;
		PushStack(&Path, Cell);
		PushStack(&TaxiPath, Cell);
	}
}



void DrawTaxi() 
{			   
	SDL_FRect r = {.x = TILE_SIZE_PIXELS * Taxi.Y, 
				   .y = TILE_SIZE_PIXELS * Taxi.X, 
				   .w = TILE_SIZE_PIXELS, .h = TILE_SIZE_PIXELS};

	DrawRectangle(r, 255, 105, 180);
}

void Draw(game_state *GameState)
{	
	StartDrawing();
	{	
		DrawTilemap(&GameState->Tilemap);
		DrawPath(&Path); 
		DrawTaxi();
		// DrawGUI(GameState);
	}
	EndDrawing();
}

void DrawGUI(game_state *GameState)
{
	overview(WindowManager.Nuklear);
}

void StartDrawing()
{
	SDL_SetRenderDrawColor(WindowManager.Renderer, 192, 192, 192, 255);
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

	int k = 0;
	for (int i = 0; i < SCREEN_WIDTH_PIXELS / TILE_SIZE_PIXELS; i++) { 
		for (int j = 0; j < SCREEN_HEIGHT_PIXELS / TILE_SIZE_PIXELS; j++) {
			if (Tilemap->Tiles[k++].Type == TOWER_TILE) {
				SDL_FRect r = {.x = TILE_SIZE_PIXELS * i, 
					   		   .y = TILE_SIZE_PIXELS * j, 
					   		   .w = TILE_SIZE_PIXELS, .h = TILE_SIZE_PIXELS};

			    DrawRectangle(r, 0, 0, 0);
			}
		}
	}
 
}

int my_random_function() { 
    return  (rand() % 2);
}

game_state * CreateGameState(is_open_cell_function function)
{	
	game_state *GameState = (game_state *) malloc (sizeof(game_state));
	GameState->Tilemap.Width = SCREEN_WIDTH_PIXELS / TILE_SIZE_PIXELS;
	GameState->Tilemap.Height = SCREEN_HEIGHT_PIXELS / TILE_SIZE_PIXELS;
	GameState->Tilemap.Tiles = (tile *) calloc(GameState->Tilemap.Width * GameState->Tilemap.Height, sizeof(tile));

	GameState->AStarGrid = malloc(sizeof(astar_grid));
	GameState->AStarGrid->NumberRows = SCREEN_WIDTH_PIXELS / TILE_SIZE_PIXELS;
	GameState->AStarGrid->NumberCols = SCREEN_HEIGHT_PIXELS / TILE_SIZE_PIXELS;
	GameState->AStarGrid->IsOpenCellFunction = function;
	GameState->AStarGrid->Map = malloc(GameState->AStarGrid->NumberRows * sizeof(cell*));

	for (int i = 0; i < GameState->AStarGrid->NumberRows; i++) {
		GameState->AStarGrid->Map[i] = calloc(GameState->AStarGrid->NumberCols, sizeof(cell));
	}

	int k = 0;
	for (int i = 0; i < GameState->AStarGrid->NumberRows; i++) {
		for (int j = 0; j < GameState->AStarGrid->NumberCols; j++) {
			if (i % 5 == 0 || j % 10 == 0) {
				GameState->AStarGrid->Map[i][j].MovementCost = my_random_function();
			
			} else {
				GameState->AStarGrid->Map[i][j].MovementCost = 1;
			}
			
			GameState->Tilemap.Tiles[k++].Type = GameState->AStarGrid->Map[i][j].MovementCost == 1 ? ROAD_TILE : TOWER_TILE;
			GameState->AStarGrid->Map[i][j].f = -1;
			GameState->AStarGrid->Map[i][j].g = 0.0;
            GameState->AStarGrid->Map[i][j].h = 0.0;
		}
	} 

	return GameState;
} 

void DestroyGameState(game_state *GameState)
{
	free(GameState->Tilemap.Tiles);
	for (int i = 0; i < GameState->AStarGrid->NumberRows; i++) {
		free(GameState->AStarGrid->Map[i]);
	}
	free(GameState->AStarGrid->Map);
	free(GameState);
}

bool IsOpenCellFunction(point Location, void *AStarGrid) {
	if ((((astar_grid*)(AStarGrid))->Map[Location.Row][Location.Col].MovementCost) == 1) {
		return true;
	} else {
		return false;
	}	
}





