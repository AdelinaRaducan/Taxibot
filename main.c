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
#include <sys/time.h>

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

const int MAX_NUMBER_OF_TAXIBOTS = 20;
const int MAX_NUMBER_OF_ORDERS = 100;
const int MAX_NUMBER_OF_DEPOTS = 10;
const double TAXIBOT_SPEED = 4;
int xMouse, yMouse;


static struct {
	SDL_Window *Handler;
	SDL_Renderer *Renderer;
	struct nk_context *Nuklear;
} WindowManager;

typedef enum tile_type {
	ROAD_TILE,
	TOWER_TILE
} tile_type;

typedef enum passenger_status {
	WAITING,
	IN_TRANSIT,
	ARRIVED
} passenger_status;

typedef enum command_type {
	ADD_TAXIBOT,
	ADD_ORDER,
	ADD_DEPOT
} command_type;

typedef enum taxibot_status {
	TAXIBOT_AVAILABLE,
	TAXIBOT_WORKING,
	TAXIBOT_RECEIVED_ORDER,
	TAXIBOT_TO_ORDER,
	TAXIBOT_TO_DEST,
	TAXIBOT_CHARGING
} taxibot_status;

typedef struct v2 {
	union {
		struct {
			double X, Y;
		};

		struct {
			double W, H;
		}
	}
} v2;

typedef struct tile {
	tile_type Type;
} tile;

typedef struct tilemap {
	tile *Tiles;
	int Width, Height;
} tilemap;

typedef struct order {
	struct timeval tv;
	v2 Position;
	v2 Destination;
	passenger_status Status;
} order;

typedef struct taxibot_dispatcher;

typedef struct taxibot {
	int ID;
	v2 Position;
	v2 Direction;
	double Speed;
	order Order;
	taxibot_status Status;
	Tstack *Path;
	struct taxibot_dispatcher *Dispatcher;
	v2 NextPosition;
} taxibot;

typedef struct depot {
	v2 Position;
} depot;

typedef struct taxibot_dispatcher {
	Tqueue Orders;
	taxibot *Taxibots;
	depot *Depots;
	int TaxibotsLength;
	int OrdersLength;
	int DepotsLength;
} taxibot_dispatcher;

typedef struct game_state {
	tilemap Tilemap;
	astar_grid *AStarGrid;
	taxibot_dispatcher *Dispatcher;
	Tqueue Commands;
	Uint8 *PreviousKeyboardState;
} game_state;


void CreateWindow(int Width, int Height);
game_state * CreateGameState();
astar_grid * CreateAStarGrid();
taxibot_dispatcher * CreateDispatcher();
void CreateOrder(Tqueue *Orders, int *OrdersLength, astar_grid *AStarGrid);
void CreateDepot(depot *Depots, int *DepotsLength);

void HandleInput(game_state *GameState);
void UpdateAndRenderPlay(game_state *GameState);
void Update(game_state *GameState);
void UpdateDispatcher(taxibot_dispatcher *Dispatcher, astar_grid *AStarGrid);
void DispatcherRemoveOrder(taxibot_dispatcher *Dispatcher, order Order);
void UpdateOrder(order *Order);
void UpdateTaxibot(taxibot *Taxibot, astar_grid *AStarGrid);
void UpdateTaxibots(taxibot *Taxibots, int TaxibotsLength, astar_grid *AStarGrid);
void TaxibotFollowPath(taxibot *Taxibot, Tstack *Path, point LastPosition);

void Draw(game_state *GameState);
void StartDrawing();
void DrawGUI(game_state *GameState);
void EndDrawing();
void DrawTilemap(tilemap *Tilemap);
void DrawDepots(depot *Depots, int DepotsLength);
void DrawOrder(order *Order);
void DrawOrders(Tqueue *Orders);
void DrawTaxibot(taxibot *Taxibot);
void DrawTaxibots(taxibot *Taxibots, int TaxibotsLength);

void AddTaxibot(taxibot *Taxibot, int *TaxibotsLength, depot *Depots, int DepotsLength);
void AssignOrderToTaxibot(taxibot *Taxibot, order Order);
bool IsOpenCellFunction(point Location, void *AStarGrid);
v2	 TaxibotMoveTowardsPoint(taxibot *Taxibot, point Point);
point FindParkingSpot(v2 Point, astar_grid *AStarGrid);
bool TaxibotFinishedFollowPath(v2 TaxibotPosition, point LastPosition);
v2 	GetTaxiBotDirection(taxibot *Taxibot, point Point);

void DestroyDispatcher(taxibot_dispatcher *Dispatcher);
void DestroyAStarGrid(astar_grid * AStarGrid);
void DestroyGameState(game_state *GameState);
void DestroyWindow();


int main( int argc, char* args[] ) 
{
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

game_state * CreateGameState()
{	
	game_state *GameState = (game_state *) malloc (sizeof(game_state));
	GameState->Tilemap.Width = SCREEN_WIDTH_PIXELS / TILE_SIZE_PIXELS;
	GameState->Tilemap.Height = SCREEN_HEIGHT_PIXELS / TILE_SIZE_PIXELS;
	GameState->Tilemap.Tiles = (tile *) calloc(GameState->Tilemap.Width * GameState->Tilemap.Height, sizeof(tile));
	GameState->AStarGrid = CreateAStarGrid();

	int k = 0;
	for (int i = 0; i < GameState->AStarGrid->NumberRows; i++) {
		for (int j = 0; j < GameState->AStarGrid->NumberCols; j++) {
			GameState->Tilemap.Tiles[k++].Type = GameState->AStarGrid->Map[i][j].MovementCost == 1 ? ROAD_TILE : TOWER_TILE;
		}
	}

	GameState->Dispatcher = CreateDispatcher();
	InitQueue(&GameState->Commands, sizeof(command_type), NULL);
	GameState->PreviousKeyboardState = NULL;

	return GameState;
}

astar_grid * CreateAStarGrid() 
{
	astar_grid  *AStarGrid = (astar_grid*) malloc(sizeof(astar_grid));
	AStarGrid->NumberRows = SCREEN_HEIGHT_PIXELS / TILE_SIZE_PIXELS;
	AStarGrid->NumberCols = SCREEN_WIDTH_PIXELS / TILE_SIZE_PIXELS;
	AStarGrid->IsOpenCellFunction = IsOpenCellFunction;
	AStarGrid->Map = (cell**) malloc(AStarGrid->NumberRows * sizeof(cell*));

	for (int i = 0; i < AStarGrid->NumberRows; i++) {
		AStarGrid->Map[i] = (cell*) calloc(AStarGrid->NumberCols, sizeof(cell));
	}

	int k = 0;
	for (int i = 0; i < AStarGrid->NumberRows; i++) {
		for (int j = 0; j < AStarGrid->NumberCols; j++) {
			if (i % 10 == 2 || j % 10 == 3) {
				AStarGrid->Map[i][j].MovementCost = my_random_function();
			} else {
				AStarGrid->Map[i][j].MovementCost = 1;
			}
			
			AStarGrid->Map[i][j].Location.Row = 0;
			AStarGrid->Map[i][j].Location.Col = 0;
			AStarGrid->Map[i][j].f = -1;
			AStarGrid->Map[i][j].g = 0.0;
            AStarGrid->Map[i][j].h = 0.0;
		}
	}

	return AStarGrid;
}

taxibot_dispatcher * CreateDispatcher()
{
	taxibot_dispatcher *Dispatcher = (taxibot_dispatcher *) malloc(sizeof(taxibot_dispatcher));
	Dispatcher->Taxibots = (taxibot *) malloc (MAX_NUMBER_OF_TAXIBOTS * sizeof(taxibot));
	for (int i = 0; i < MAX_NUMBER_OF_TAXIBOTS; ++i) {
		Dispatcher->Taxibots[i].Speed = TAXIBOT_SPEED;
		Dispatcher->Taxibots[i].ID = i;
	}

	Dispatcher->TaxibotsLength = 0;
	Dispatcher->Depots = (depot *) malloc(MAX_NUMBER_OF_DEPOTS * sizeof(depot));
	Dispatcher->DepotsLength = 0;


	// init orders
	InitQueue(&Dispatcher->Orders, sizeof(order), NULL);
	Dispatcher->OrdersLength = 0;
	return Dispatcher;
}

void CreateDepot(depot *Depots, int *DepotsLength) 
{	
	if ((*DepotsLength) < MAX_NUMBER_OF_DEPOTS) {
		Depots[(*DepotsLength)].Position.X = abs(yMouse - SCREEN_HEIGHT_PIXELS);
		Depots[(*DepotsLength)].Position.Y = xMouse;

		while ((int) (Depots[(*DepotsLength)].Position.X) % TILE_SIZE_PIXELS != 0) {
			Depots[(*DepotsLength)].Position.X -= 1;
		}

		while ((int) (Depots[(*DepotsLength)].Position.Y) % TILE_SIZE_PIXELS != 0) {
			Depots[(*DepotsLength)].Position.Y -= 1;
		}

		printf("depot (%.0f %.0f)\n", Depots[(*DepotsLength)].Position.X, Depots[(*DepotsLength)].Position.Y);
		(*DepotsLength)++;
	}
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
	Uint8* keyboard;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
			{
				DestroyGameState(GameState);
	      		DestroyWindow();
				exit(0);
	    	} break;

	    	case SDL_KEYDOWN:
	    	{
	    		switch (event.key.keysym.sym) {
	    			case SDLK_RETURN:
		    			PushQueue(&GameState->Commands, &(command_type){ADD_TAXIBOT});
				    	break;

				    case SDLK_o:
			    		PushQueue(&GameState->Commands, &(command_type){ADD_ORDER});
					    break;
	    		}
	    	} break;

	    	case SDL_MOUSEBUTTONDOWN:
	    	{
	    		PushQueue(&GameState->Commands, &(command_type){ADD_DEPOT});
	    		SDL_GetMouseState(&xMouse, &yMouse);
	    		printf("mouse event: %d %d\n", xMouse, yMouse);
	    	} break;
    	}
    	nk_sdl_handle_event(&event);
	}

	nk_input_end(WindowManager.Nuklear);
}

void Update(game_state *GameState)
{	
	while (!IsQueueEmpty(&GameState->Commands)) {
		command_type Command;
		PeekQueue(&GameState->Commands, &Command);

		switch(Command) {
			case ADD_TAXIBOT:
				if (GameState->Dispatcher->DepotsLength > 0) {
				AddTaxibot(&GameState->Dispatcher->Taxibots[GameState->Dispatcher->TaxibotsLength], &GameState->Dispatcher->TaxibotsLength,
							GameState->Dispatcher->Depots, GameState->Dispatcher->DepotsLength);
				}
				break;

			case ADD_ORDER:
				CreateOrder(&GameState->Dispatcher->Orders, &GameState->Dispatcher->OrdersLength, GameState->AStarGrid);
				break;

			case ADD_DEPOT:
				CreateDepot(GameState->Dispatcher->Depots, &GameState->Dispatcher->DepotsLength);
				break;

			default:
				break;
		}

		PopQueue(&GameState->Commands);
	}

	UpdateDispatcher(GameState->Dispatcher, GameState->AStarGrid);
	UpdateTaxibots(GameState->Dispatcher->Taxibots, GameState->Dispatcher->TaxibotsLength, GameState->AStarGrid);
}

void Draw(game_state *GameState)
{	
	StartDrawing();
	{	
		DrawTilemap(&GameState->Tilemap);
		DrawDepots(GameState->Dispatcher->Depots, GameState->Dispatcher->DepotsLength);
		DrawTaxibots(GameState->Dispatcher->Taxibots, GameState->Dispatcher->TaxibotsLength);
		DrawOrders(&GameState->Dispatcher->Orders);
		// DrawGUI(GameState);
	}
	EndDrawing();
}

void UpdateDispatcher(taxibot_dispatcher *Dispatcher, astar_grid *AStarGrid) 
{	
	if (IsQueueEmpty(&Dispatcher->Orders)) {
		return;
	}

	for (int i = 0; i < Dispatcher->TaxibotsLength; i++) {
		if (IsQueueEmpty(&Dispatcher->Orders)) {
			break;
		}

		if (Dispatcher->Taxibots[i].Status == TAXIBOT_AVAILABLE) {
			order aux = {};
			PeekQueue(&Dispatcher->Orders, &aux);
			PopQueue(&Dispatcher->Orders);
			AssignOrderToTaxibot(&Dispatcher->Taxibots[i], aux);
			Dispatcher->OrdersLength--;
		}
	}
}

void AssignOrderToTaxibot(taxibot *Taxibot, order Order)
{
	Taxibot->Order = Order;
	Taxibot->Order.Status = WAITING;
	Taxibot->Status = TAXIBOT_RECEIVED_ORDER;
}

void UpdateTaxibots(taxibot *Taxibots, int TaxibotsLength, astar_grid *AStarGrid) 
{
	for (int i = 0; i < TaxibotsLength; i++) {
		UpdateTaxibot(&Taxibots[i], AStarGrid);
	}
}

void UpdateTaxibot(taxibot *Taxibot, astar_grid *AStarGrid)
{	
	if (!Taxibot) return;

	switch (Taxibot->Status) {
		case TAXIBOT_AVAILABLE:
			break;

		case TAXIBOT_RECEIVED_ORDER:
		{
			Taxibot->Path = FindPath(
								(point) {(int) (Taxibot->Position.X / TILE_SIZE_PIXELS),(int) (Taxibot->Position.Y / TILE_SIZE_PIXELS)}, 
								FindParkingSpot(Taxibot->Order.Position, AStarGrid), AStarGrid
							);
			if (Taxibot->Path != NULL) {
				Taxibot->Status = TAXIBOT_TO_ORDER;
			} else {
				Taxibot->Status = TAXIBOT_AVAILABLE;
			}
		} break;

		case TAXIBOT_TO_ORDER:
		{	
			point LastPosition = FindParkingSpot(Taxibot->Order.Position, AStarGrid);
			TaxibotFollowPath(Taxibot, Taxibot->Path, LastPosition);

			if ((!Taxibot->Path || IsStackEmpty(Taxibot->Path)) && 
				TaxibotFinishedFollowPath(Taxibot->Position, LastPosition)) {
				Taxibot->Path = FindPath(
									(point) {(int) (Taxibot->Position.X / TILE_SIZE_PIXELS),(int) (Taxibot->Position.Y / TILE_SIZE_PIXELS)}, 
									FindParkingSpot(Taxibot->Order.Destination, AStarGrid), AStarGrid
								);		
				if (Taxibot->Path != NULL) {
					Taxibot->Status = TAXIBOT_TO_DEST;
					Taxibot->Order.Status = IN_TRANSIT;
				} else {
					Taxibot->Status = TAXIBOT_AVAILABLE;
				}
			}
		} break;

		case TAXIBOT_TO_DEST:
		{
			point LastPosition = FindParkingSpot(Taxibot->Order.Destination, AStarGrid);
			TaxibotFollowPath(Taxibot, Taxibot->Path, LastPosition);

			if ((!Taxibot->Path || IsStackEmpty(Taxibot->Path)) &&
				TaxibotFinishedFollowPath(Taxibot->Position, LastPosition)) {
				Taxibot->Status = TAXIBOT_AVAILABLE;
				Taxibot->Order.Status = ARRIVED;
			}
		} break;	
	}
}

void TaxibotFollowPath(taxibot *Taxibot, Tstack *Path, point LastPosition)
{
	if ((!Path || IsStackEmpty(Taxibot->Path)) && TaxibotFinishedFollowPath(Taxibot->Position, LastPosition))
		return;

	v2 Distance = TaxibotMoveTowardsPoint(Taxibot, (point) {(int) (Taxibot->NextPosition.X), (int) (Taxibot->NextPosition.Y)});
	if (Distance.X < TAXIBOT_SPEED && Distance.Y < TAXIBOT_SPEED) {
		Tstack *stack = PopStack(&Taxibot->Path);
		Taxibot->NextPosition = (v2) {stack->Data.Location.Row * TILE_SIZE_PIXELS + TILE_SIZE_PIXELS/2, stack->Data.Location.Col * TILE_SIZE_PIXELS + TILE_SIZE_PIXELS/2};
		free(stack); 
	}
}

bool TaxibotFinishedFollowPath(v2 TaxibotPosition, point LastPosition)
{
	if (((TaxibotPosition.X - TILE_SIZE_PIXELS / 2) / TILE_SIZE_PIXELS) == (double)LastPosition.Row && 
		((TaxibotPosition.Y - TILE_SIZE_PIXELS / 2) / TILE_SIZE_PIXELS) == (double)LastPosition.Col)
		return true;
	return false;
}

point FindParkingSpot(v2 Location, astar_grid *AStarGrid) {
	point Point = {.Row = (int)(Location.X), .Col = (int)(Location.Y)};

	for (int add_Row = -1; add_Row <= 1; add_Row++) {
    	for (int add_Col = -1; add_Col <= 1; add_Col++) {
    		point ParkingSpot = {.Row = Point.Row + add_Row, .Col = Point.Col + add_Col};
    		if (IsNeighbour(Point, ParkingSpot, AStarGrid) == true && AStarGrid->Map[ParkingSpot.Row][ParkingSpot.Col].MovementCost == 1) {
    			return ParkingSpot;
    		}
    	}
    }
}

v2 GetTaxiBotDirection(taxibot *Taxibot, point Point) 
{
	int DirectionX = 0;
	int DirectionY = 0;

	if ((int) (Taxibot->Position.X - Point.Row) < 0)
		DirectionX = 1;
	else if ((int) (Taxibot->Position.X - Point.Row) > 0)
		DirectionX = -1;

	if ((int) (Taxibot->Position.Y - Point.Col) < 0)
		DirectionY = 1;
	else if ((int) (Taxibot->Position.Y - Point.Col) > 0)
		DirectionY = -1;

	return (v2) {DirectionX, DirectionY};
}

v2 TaxibotMoveTowardsPoint(taxibot *Taxibot, point Point) 
{
	v2 Distance = (v2) { abs(Taxibot->Position.X - Point.Row), 
		                 abs(Taxibot->Position.Y - Point.Col)};

	if (Distance.X < TAXIBOT_SPEED && Distance.Y < TAXIBOT_SPEED) {
		return Distance;
	}

	Taxibot->Direction = GetTaxiBotDirection(Taxibot, Point);
	
	if (Distance.X < TAXIBOT_SPEED)
		Taxibot->Position.X = Point.Row;
	else
		Taxibot->Position.X += Taxibot->Direction.X * Taxibot->Speed;

	if (Distance.Y < TAXIBOT_SPEED)
		Taxibot->Position.Y = Point.Col;
	else
		Taxibot->Position.Y += Taxibot->Direction.Y * Taxibot->Speed;

	return Distance;
}

void AddTaxibot(taxibot *Taxibot, int *TaxibotsLength, depot *Depots, int DepotsLength)
{	
	if ((*TaxibotsLength) >= MAX_NUMBER_OF_TAXIBOTS)
		return;

	int i = rand() % DepotsLength;
	Taxibot->Position.X = Depots[i].Position.X + TILE_SIZE_PIXELS/2;
	Taxibot->Position.Y = Depots[i].Position.Y + TILE_SIZE_PIXELS/2;
	Taxibot->NextPosition = Taxibot->Position;
	Taxibot->Status = TAXIBOT_AVAILABLE;
	Taxibot->Path = NULL;
	(*TaxibotsLength)++;
}

void ShowOrdersQueue(Tqueue *Queue)
{
	node *Temp = Queue->head;
    while (Temp != NULL) {
        printf("(O:%.0f %.0f)->", ((order*)((Temp)->Data))->Position.X, ((order*)((Temp)->Data))->Position.Y);
        Temp = Temp->next;
    }

    printf("\n");
}

void CreateOrder(Tqueue *Orders, int *OrdersLength, astar_grid *AStarGrid) 
{	
	if ((*OrdersLength) < MAX_NUMBER_OF_ORDERS) {
		order Order = {0};
		int counter = 0;

		do {
			Order.Position.X = rand() % (SCREEN_HEIGHT_PIXELS / TILE_SIZE_PIXELS);
			Order.Position.Y = rand() % (SCREEN_WIDTH_PIXELS / TILE_SIZE_PIXELS);
			counter++;
		} while (AStarGrid->Map[(int)(Order.Position.X)][(int)(Order.Position.Y)].MovementCost != 0 && counter < 10);

		counter = 0;
		do {
			Order.Destination.X = rand() % (SCREEN_HEIGHT_PIXELS / TILE_SIZE_PIXELS);
			Order.Destination.Y = rand() % (SCREEN_WIDTH_PIXELS / TILE_SIZE_PIXELS);
			counter++;
		} while (AStarGrid->Map[(int)(Order.Destination.X)][(int)(Order.Destination.Y)].MovementCost != 0 && counter < 10);

		gettimeofday(&(Order.tv),NULL);
		Order.Status = WAITING;

		if (AStarGrid->Map[(int)(Order.Position.X)][(int)(Order.Position.Y)].MovementCost == 0 &&
			AStarGrid->Map[(int)(Order.Destination.X)][(int)(Order.Destination.Y)].MovementCost == 0) { 
			printf("---Order: (%.0f %.0f) (%.0f %.0f)\n", Order.Position.X, Order.Position.Y, Order.Destination.X, Order.Destination.Y);
			PushQueue(Orders, &Order);
			(*OrdersLength)++;
		} else {
			printf("Invalid order\n");
		}
	}
}

bool IsOpenCellFunction(point Location, void *AStarGrid) 
{
	if (((cell)((astar_grid*)(AStarGrid))->Map[Location.Row][Location.Col]).MovementCost == 1) {
		return true;
	} else {
		return false;
	}	
}

int my_random_function() 
{ 
    return (rand() % 2);
}

void DrawTaxibots(taxibot *Taxibots, int TaxibotsLength) 
{
	for (int i = 0; i < TaxibotsLength; i++) {
		DrawTaxibot(&Taxibots[i]);
	}
}

void DrawTaxibot(taxibot *Taxibot) 
{	
	SDL_FRect r = {.x = Taxibot->Position.Y - TILE_SIZE_PIXELS/2, 
				   .y = Taxibot->Position.X - TILE_SIZE_PIXELS/2, 
				   .w = TILE_SIZE_PIXELS, .h = TILE_SIZE_PIXELS};

	DrawRectangle(r, 248, 215, 99);

	if (Taxibot->Status == TAXIBOT_TO_ORDER || Taxibot->Status == TAXIBOT_TO_DEST)
		DrawOrder(&Taxibot->Order);

}

void DrawOrders(Tqueue *Orders) {
	node *Temp = Orders->head;

	while (Temp != NULL) {
		order *Order = (order *) (Temp->Data);
		DrawOrder(Order);
		Temp = Temp->next;
	}
}

void DrawOrder(order *Order) {
	if (!Order)	return;

	switch (Order->Status) {
		case WAITING:
		{
			SDL_FRect r = {.x = TILE_SIZE_PIXELS * Order->Position.Y + (TILE_SIZE_PIXELS * 0.25), 
						   .y = TILE_SIZE_PIXELS * Order->Position.X - (TILE_SIZE_PIXELS * 0.25), 
						   .w = TILE_SIZE_PIXELS / 2, .h = TILE_SIZE_PIXELS / 2};

			DrawRectangle(r, 10, 215, 99);
		} break;
			
		case IN_TRANSIT:
		{
			SDL_FRect r = {.x = TILE_SIZE_PIXELS * Order->Destination.Y + (TILE_SIZE_PIXELS * 0.25), 
						   .y = TILE_SIZE_PIXELS * Order->Destination.X - (TILE_SIZE_PIXELS * 0.25), 
						   .w = TILE_SIZE_PIXELS / 2, .h = TILE_SIZE_PIXELS / 2};

			DrawRectangle(r, 239, 98, 68);
		} break;			
	}


}

void DrawDepots(depot *Depots, int DepotsLength)
{
	for (int i = 0; i < DepotsLength; i++) {
		SDL_FRect r = {.x = Depots[i].Position.Y, 
					   .y = Depots[i].Position.X, 
					   .w = TILE_SIZE_PIXELS, .h = TILE_SIZE_PIXELS};

		DrawRectangle(r, 255, 0, 145);
	}
}

void DrawRectangle(SDL_FRect r, int R, int G, int B)
{	
    r.y = SCREEN_HEIGHT_PIXELS - r.y - TILE_SIZE_PIXELS;
    SDL_SetRenderDrawColor( WindowManager.Renderer, R, G, B, 255);
    SDL_RenderFillRectF( WindowManager.Renderer, &r );
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
	for (int i = 0; i < SCREEN_HEIGHT_PIXELS / TILE_SIZE_PIXELS; i++) { 
		for (int j = 0; j < SCREEN_WIDTH_PIXELS / TILE_SIZE_PIXELS; j++) {
			if (Tilemap->Tiles[k++].Type == TOWER_TILE) {
				SDL_FRect r = {.x = TILE_SIZE_PIXELS * j, 
					   		   .y = TILE_SIZE_PIXELS * i, 
					   		   .w = TILE_SIZE_PIXELS, .h = TILE_SIZE_PIXELS};

			    DrawRectangle(r, 0, 0, 0);
			}
		}
	}
}

void DestroyDispatcher(taxibot_dispatcher *Dispatcher)
{
	free(Dispatcher->Taxibots);
	free(Dispatcher->Depots);
	DestroyQueue(&Dispatcher->Orders);
}

void DestroyAStarGrid(astar_grid * AStarGrid) 
{
	for (int i = 0; i < AStarGrid->NumberRows; i++) {
		free(AStarGrid->Map[i]);
	}

	free(AStarGrid->Map);
}

void DestroyGameState(game_state *GameState)
{
	free(GameState->Tilemap.Tiles);
	DestroyAStarGrid(GameState->AStarGrid);
	DestroyDispatcher(GameState->Dispatcher);
	DestroyQueue(&GameState->Commands);
	free(GameState->PreviousKeyboardState);
	free(GameState);
}

void DestroyWindow()
{
	nk_sdl_shutdown();
	SDL_DestroyRenderer(WindowManager.Renderer);
	SDL_DestroyWindow(WindowManager.Handler);
	SDL_Quit();
}