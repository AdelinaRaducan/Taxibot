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
// #define DEBUG_MODE 

const int SCREEN_WIDTH_PIXELS = 1280;
const int SCREEN_HEIGHT_PIXELS = 720;
const int TILE_SIZE_PIXELS = 16; 

const int MAX_NUMBER_OF_RobotaxiS = 20;
const int MAX_NUMBER_OF_ORDERS = 100;
const int MAX_NUMBER_OF_DEPOTS = 10;
const double Robotaxi_SPEED = 4;
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
	ADD_Robotaxi,
	ADD_ORDER,
	ADD_DEPOT,
	RETURN_TO_DEPOTS
} command_type;

typedef enum Robotaxi_status {
	ROBOTAXI_AVAILABLE,
	ROBOTAXI_WORKING,
	ROBOTAXI_RECEIVED_ORDER,
	ROBOTAXI_TO_ORDER,
	ROBOTAXI_TO_DEST,
	ROBOTAXI_CHARGING,
	ROBOTAXI_END_SHIFT,
	ROBOTAXI_TO_DEPOT
} Robotaxi_status;

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

typedef struct Robotaxi_dispatcher;

typedef struct Robotaxi {
	v2 Position;
	v2 Direction;
	double Speed;
	order Order;
	Robotaxi_status Status;
	Tstack *Path;
	struct Robotaxi_dispatcher *Dispatcher;
	v2 NextPosition;
} Robotaxi;

typedef struct depot {
	v2 Position;
} depot;

typedef struct Robotaxi_dispatcher {
	Tqueue Orders;
	Robotaxi *Robotaxis;
	depot *Depots;
	int RobotaxisLength;
	int OrdersLength;
	int DepotsLength;
} Robotaxi_dispatcher;

typedef struct game_state {
	tilemap Tilemap;
	astar_grid *AStarGrid;
	Robotaxi_dispatcher *Dispatcher;
	Tqueue Commands;
	Uint8 *PreviousKeyboardState;
} game_state;

void CreateWindow(int Width, int Height);
game_state * CreateGameState();
astar_grid * CreateAStarGrid();
Robotaxi_dispatcher * CreateDispatcher();
void CreateOrder(Tqueue *Orders, int *OrdersLength, astar_grid *AStarGrid);
void CreateDepot(depot *Depots, int *DepotsLength);

void HandleInput(game_state *GameState);
void UpdateAndRenderPlay(game_state *GameState);
void Update(game_state *GameState);
void UpdateDispatcher(Robotaxi_dispatcher *Dispatcher, astar_grid *AStarGrid);
void DispatcherRemoveOrder(Robotaxi_dispatcher *Dispatcher, order Order);
void UpdateOrder(order *Order);
void UpdateRobotaxi(Robotaxi *Robotaxi, astar_grid *AStarGrid, depot *Depots, int DepotsLength);
void UpdateRobotaxis(Robotaxi *Robotaxis, int RobotaxisLength, astar_grid *AStarGrid, depot *Depots, int DepotsLength);
void RobotaxiFollowPath(Robotaxi *Robotaxi, Tstack *Path, point LastPosition);
void RobotaxisReturnToDepots(Robotaxi *Robotaxis, int RobotaxisLength);
v2 FindClosestDepot(v2 RobotaxiPosition, depot *Depots, int DepotsLength);

void Draw(game_state *GameState);
void StartDrawing();
void DrawGUI(game_state *GameState);
void EndDrawing();
void DrawTilemap(tilemap *Tilemap);
void DrawDepots(depot *Depots, int DepotsLength);
void DrawOrder(order *Order);
void DrawOrders(Tqueue *Orders);
void DrawRobotaxi(Robotaxi *Robotaxi);
void DrawRobotaxis(Robotaxi *Robotaxis, int RobotaxisLength);

void AddRobotaxi(Robotaxi *Robotaxi, int *RobotaxisLength, depot *Depots, int DepotsLength);
void AssignOrderToRobotaxi(Robotaxi *Robotaxi, order Order);
bool IsOpenCellFunction(point Location, void *AStarGrid);
v2	 RobotaxiMoveTowardsPoint(Robotaxi *Robotaxi, point Point);
point FindParkingSpot(v2 Point, astar_grid *AStarGrid);
bool RobotaxiFinishedFollowPath(v2 RobotaxiPosition, point LastPosition);
v2 	GetRobotaxiDirection(Robotaxi *Robotaxi, point Point);

void DestroyDispatcher(Robotaxi_dispatcher *Dispatcher);
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
			if (i % 10 == 2 || j % 10 == 2) {
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

Robotaxi_dispatcher * CreateDispatcher()
{
	Robotaxi_dispatcher *Dispatcher = (Robotaxi_dispatcher *) malloc(sizeof(Robotaxi_dispatcher));
	Dispatcher->Robotaxis = (Robotaxi *) malloc (MAX_NUMBER_OF_RobotaxiS * sizeof(Robotaxi));
	for (int i = 0; i < MAX_NUMBER_OF_RobotaxiS; ++i) {
		Dispatcher->Robotaxis[i].Speed = Robotaxi_SPEED;
	}

	Dispatcher->RobotaxisLength = 0;
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
		    			PushQueue(&GameState->Commands, &(command_type){ADD_Robotaxi});
				    	break;

				    case SDLK_o:
			    		PushQueue(&GameState->Commands, &(command_type){ADD_ORDER});
					    break;

					case SDLK_d:
			    		PushQueue(&GameState->Commands, &(command_type){RETURN_TO_DEPOTS});
					    break;
	    		}
	    	} break;

	    	case SDL_MOUSEBUTTONDOWN:
	    	{
	    		PushQueue(&GameState->Commands, &(command_type){ADD_DEPOT});
	    		SDL_GetMouseState(&xMouse, &yMouse);
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
			case ADD_Robotaxi:
				if (GameState->Dispatcher->DepotsLength > 0) {
				AddRobotaxi(&GameState->Dispatcher->Robotaxis[GameState->Dispatcher->RobotaxisLength], &GameState->Dispatcher->RobotaxisLength,
							GameState->Dispatcher->Depots, GameState->Dispatcher->DepotsLength);
				}
				break;

			case ADD_ORDER:
				CreateOrder(&GameState->Dispatcher->Orders, &GameState->Dispatcher->OrdersLength, GameState->AStarGrid);
				break;

			case ADD_DEPOT:
				CreateDepot(GameState->Dispatcher->Depots, &GameState->Dispatcher->DepotsLength);
				break;

			case RETURN_TO_DEPOTS:
				RobotaxisReturnToDepots(GameState->Dispatcher->Robotaxis, GameState->Dispatcher->RobotaxisLength);

			default:
				break;
		}

		PopQueue(&GameState->Commands);
	}

	UpdateDispatcher(GameState->Dispatcher, GameState->AStarGrid);
	UpdateRobotaxis(GameState->Dispatcher->Robotaxis, GameState->Dispatcher->RobotaxisLength, GameState->AStarGrid,
					GameState->Dispatcher->Depots, GameState->Dispatcher->DepotsLength);
}

void Draw(game_state *GameState)
{	
	StartDrawing();
	{	
		DrawTilemap(&GameState->Tilemap);
		DrawDepots(GameState->Dispatcher->Depots, GameState->Dispatcher->DepotsLength);
		DrawRobotaxis(GameState->Dispatcher->Robotaxis, GameState->Dispatcher->RobotaxisLength);
		DrawOrders(&GameState->Dispatcher->Orders);
		// DrawGUI(GameState);
	}
	EndDrawing();
}

void UpdateDispatcher(Robotaxi_dispatcher *Dispatcher, astar_grid *AStarGrid) 
{	
	if (IsQueueEmpty(&Dispatcher->Orders)) {
		return;
	}

	for (int i = 0; i < Dispatcher->RobotaxisLength; i++) {
		if (IsQueueEmpty(&Dispatcher->Orders)) {
			break;
		}

		if (Dispatcher->Robotaxis[i].Status == ROBOTAXI_AVAILABLE) {
			order aux = {};
			PeekQueue(&Dispatcher->Orders, &aux);
			PopQueue(&Dispatcher->Orders);
			AssignOrderToRobotaxi(&Dispatcher->Robotaxis[i], aux);
			Dispatcher->OrdersLength--;
		}
	}
}

void AssignOrderToRobotaxi(Robotaxi *Robotaxi, order Order)
{
	Robotaxi->Order = Order;
	Robotaxi->Order.Status = WAITING;
	Robotaxi->Status = ROBOTAXI_RECEIVED_ORDER;
}

void UpdateRobotaxis(Robotaxi *Robotaxis, int RobotaxisLength, astar_grid *AStarGrid, depot *Depots, int DepotsLength) 
{
	for (int i = 0; i < RobotaxisLength; i++) {
		UpdateRobotaxi(&Robotaxis[i], AStarGrid, Depots, DepotsLength);
	}
}

void UpdateRobotaxi(Robotaxi *Robotaxi, astar_grid *AStarGrid, depot *Depots, int DepotsLength)
{	
	if (!Robotaxi) return;

	switch (Robotaxi->Status) {
		case ROBOTAXI_AVAILABLE:
			break;

		case ROBOTAXI_RECEIVED_ORDER:
		{
			Robotaxi->Path = FindPath(
								(point) {(int) (Robotaxi->Position.X / TILE_SIZE_PIXELS),(int) (Robotaxi->Position.Y / TILE_SIZE_PIXELS)}, 
								FindParkingSpot(Robotaxi->Order.Position, AStarGrid), AStarGrid
							);
			if (Robotaxi->Path != NULL) {
				Robotaxi->Status = ROBOTAXI_TO_ORDER;
			} else {
				Robotaxi->Status = ROBOTAXI_AVAILABLE;
			}
		} break;

		case ROBOTAXI_TO_ORDER:
		{	
			point LastPosition = FindParkingSpot(Robotaxi->Order.Position, AStarGrid);
			RobotaxiFollowPath(Robotaxi, Robotaxi->Path, LastPosition);

			if ((!Robotaxi->Path || IsStackEmpty(Robotaxi->Path)) && 
				RobotaxiFinishedFollowPath(Robotaxi->Position, LastPosition)) {
				Robotaxi->Path = FindPath(
									(point) {(int) (Robotaxi->Position.X / TILE_SIZE_PIXELS),(int) (Robotaxi->Position.Y / TILE_SIZE_PIXELS)}, 
									FindParkingSpot(Robotaxi->Order.Destination, AStarGrid), AStarGrid
								);		
				if (Robotaxi->Path != NULL) {
					Robotaxi->Status = ROBOTAXI_TO_DEST;
					Robotaxi->Order.Status = IN_TRANSIT;
				} else {
					Robotaxi->Status = ROBOTAXI_AVAILABLE;
				}
			}
		} break;

		case ROBOTAXI_TO_DEST:
		{
			point LastPosition = FindParkingSpot(Robotaxi->Order.Destination, AStarGrid);
			RobotaxiFollowPath(Robotaxi, Robotaxi->Path, LastPosition);

			if ((!Robotaxi->Path || IsStackEmpty(Robotaxi->Path)) && RobotaxiFinishedFollowPath(Robotaxi->Position, LastPosition)) {
				Robotaxi->Status = ROBOTAXI_AVAILABLE;
				Robotaxi->Order.Status = ARRIVED;
			}
		} break;	

		case ROBOTAXI_END_SHIFT:
		{	
			v2 ClosestDepot = FindClosestDepot(Robotaxi->Position, Depots, DepotsLength);
			if ((!Robotaxi->Path || IsStackEmpty(Robotaxi->Path))) {
				Robotaxi->Path = FindPath(
									(point) {(int) (Robotaxi->Position.X / TILE_SIZE_PIXELS),(int) (Robotaxi->Position.Y / TILE_SIZE_PIXELS)}, 
									(point) {(int) (ClosestDepot.X / TILE_SIZE_PIXELS), (int) (ClosestDepot.Y / TILE_SIZE_PIXELS)},
									 AStarGrid
								);	
				Robotaxi->Status = ROBOTAXI_TO_DEPOT;
			}

		} break;

		case ROBOTAXI_TO_DEPOT:
		{
			v2 ClosestDepot = FindClosestDepot(Robotaxi->Position, Depots, DepotsLength);
			if (RobotaxiFinishedFollowPath(Robotaxi->Position, (point) {(int) (ClosestDepot.X / TILE_SIZE_PIXELS), (int) (ClosestDepot.Y / TILE_SIZE_PIXELS)}) == 0) {
				RobotaxiFollowPath(Robotaxi, Robotaxi->Path, (point) {(int) (ClosestDepot.X / TILE_SIZE_PIXELS), (int) (ClosestDepot.Y / TILE_SIZE_PIXELS)});
			} else {
				Robotaxi->Status = ROBOTAXI_AVAILABLE;
			}
		} break;
	}
}

void RobotaxiFollowPath(Robotaxi *Robotaxi, Tstack *Path, point LastPosition)
{
	if ((!Path || IsStackEmpty(Robotaxi->Path)) && RobotaxiFinishedFollowPath(Robotaxi->Position, LastPosition))
		return;

	v2 Distance = RobotaxiMoveTowardsPoint(Robotaxi, (point) {(int) (Robotaxi->NextPosition.X), (int) (Robotaxi->NextPosition.Y)});
	if (Distance.X < Robotaxi_SPEED && Distance.Y < Robotaxi_SPEED) {
		Tstack *stack = PopStack(&Robotaxi->Path);
		Robotaxi->NextPosition = (v2) {stack->Data.Location.Row * TILE_SIZE_PIXELS + TILE_SIZE_PIXELS/2, stack->Data.Location.Col * TILE_SIZE_PIXELS + TILE_SIZE_PIXELS/2};
		free(stack); 
	}
}

bool RobotaxiFinishedFollowPath(v2 RobotaxiPosition, point LastPosition)
{
	if (((RobotaxiPosition.X - TILE_SIZE_PIXELS / 2) / TILE_SIZE_PIXELS) == (double)LastPosition.Row && 
		((RobotaxiPosition.Y - TILE_SIZE_PIXELS / 2) / TILE_SIZE_PIXELS) == (double)LastPosition.Col)
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

v2 GetRobotaxiDirection(Robotaxi *Robotaxi, point Point) 
{
	int DirectionX = 0;
	int DirectionY = 0;

	if ((int) (Robotaxi->Position.X - Point.Row) < 0)
		DirectionX = 1;
	else if ((int) (Robotaxi->Position.X - Point.Row) > 0)
		DirectionX = -1;

	if ((int) (Robotaxi->Position.Y - Point.Col) < 0)
		DirectionY = 1;
	else if ((int) (Robotaxi->Position.Y - Point.Col) > 0)
		DirectionY = -1;

	return (v2) {DirectionX, DirectionY};
}

v2 RobotaxiMoveTowardsPoint(Robotaxi *Robotaxi, point Point) 
{
	v2 Distance = (v2) { abs(Robotaxi->Position.X - Point.Row), 
		                 abs(Robotaxi->Position.Y - Point.Col)};

	if (Distance.X < Robotaxi_SPEED && Distance.Y < Robotaxi_SPEED) {
		return Distance;
	}

	Robotaxi->Direction = GetRobotaxiDirection(Robotaxi, Point);
	
	if (Distance.X < Robotaxi_SPEED)
		Robotaxi->Position.X = Point.Row;
	else
		Robotaxi->Position.X += Robotaxi->Direction.X * Robotaxi->Speed;

	if (Distance.Y < Robotaxi_SPEED)
		Robotaxi->Position.Y = Point.Col;
	else
		Robotaxi->Position.Y += Robotaxi->Direction.Y * Robotaxi->Speed;

	return Distance;
}

void AddRobotaxi(Robotaxi *Robotaxi, int *RobotaxisLength, depot *Depots, int DepotsLength)
{	
	if ((*RobotaxisLength) >= MAX_NUMBER_OF_RobotaxiS)
		return;

	int i = rand() % DepotsLength;
	Robotaxi->Position.X = Depots[i].Position.X + TILE_SIZE_PIXELS/2;
	Robotaxi->Position.Y = Depots[i].Position.Y + TILE_SIZE_PIXELS/2;
	Robotaxi->NextPosition = Robotaxi->Position;
	Robotaxi->Status = ROBOTAXI_AVAILABLE;
	Robotaxi->Path = NULL;
	(*RobotaxisLength)++;
}

void ShowOrdersQueue(Tqueue *Queue)
{
	node *Temp = Queue->head;
    while (Temp != NULL) {
        DEBUG_PRINT("(O:%.0f %.0f)->", ((order*)((Temp)->Data))->Position.X, ((order*)((Temp)->Data))->Position.Y);
        Temp = Temp->next;
    }

    DEBUG_PRINT("\n");
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
			DEBUG_PRINTL("->Order: (%.0f %.0f) (%.0f %.0f)\n", Order.Position.X, Order.Position.Y, Order.Destination.X, Order.Destination.Y);
			PushQueue(Orders, &Order);
			(*OrdersLength)++;
		} else {
			DEBUG_PRINTL("Invalid order\n");
		}
	}
}

v2 FindClosestDepot(v2 RobotaxiPosition, depot *Depots, int DepotsLength)
{	
	v2 ClosestDepot = {0};
	double Distance = INT_MAX;
	for (int i = 0; i < DepotsLength; i++) {
		double NewDistance = abs(RobotaxiPosition.X - Depots[i].Position.X) + abs(RobotaxiPosition.Y - Depots[i].Position.Y);
		if (NewDistance < Distance) {
			Distance = NewDistance;
			ClosestDepot.X = Depots[i].Position.X;
			ClosestDepot.Y = Depots[i].Position.Y;
		}
	}

	return ClosestDepot;
}

void RobotaxisReturnToDepots(Robotaxi *Robotaxis, int RobotaxisLength)
{
	for (int i = 0; i < RobotaxisLength; i++) {
		if (Robotaxis[i].Status == ROBOTAXI_AVAILABLE) {
			Robotaxis[i].Status = ROBOTAXI_END_SHIFT;
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

void DrawRobotaxis(Robotaxi *Robotaxis, int RobotaxisLength) 
{
	for (int i = 0; i < RobotaxisLength; i++) {
		DrawRobotaxi(&Robotaxis[i]);
	}
}

void DrawRobotaxi(Robotaxi *Robotaxi) 
{	
	SDL_FRect r = {.x = Robotaxi->Position.Y - TILE_SIZE_PIXELS/2, 
				   .y = Robotaxi->Position.X - TILE_SIZE_PIXELS/2, 
				   .w = TILE_SIZE_PIXELS, .h = TILE_SIZE_PIXELS};

	DrawRectangle(r, 248, 215, 99);

	if (Robotaxi->Status == ROBOTAXI_TO_ORDER || Robotaxi->Status == ROBOTAXI_TO_DEST)
		DrawOrder(&Robotaxi->Order);

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

void DestroyDispatcher(Robotaxi_dispatcher *Dispatcher)
{
	free(Dispatcher->Robotaxis);
	free(Dispatcher->Depots);
	DestroyQueue(&Dispatcher->Orders);
	free(Dispatcher);
}

void DestroyAStarGrid(astar_grid * AStarGrid) 
{
	for (int i = 0; i < AStarGrid->NumberRows; i++) {
		free(AStarGrid->Map[i]);
	}

	free(AStarGrid->Map);
	free(AStarGrid);
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