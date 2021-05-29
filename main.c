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

const int MAX_NUMBER_OF_TAXIBOTS = 10;
const int MAX_NUMBER_OF_ORDERS = 50;
const int MAX_NUMBER_OF_DEPOTS = 2;
const double TAXIBOT_SPEED = 2;


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
	ADD_ORDER
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
	v2 Position;
	v2 Direction;
	double Speed;
	order Order;
	taxibot_status Status;
	Tstack *Path;
	struct taxibot_dispatcher *Dispatcher;
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
void CreateDepots(depot *Depots);

void UpdateAndRenderPlay(game_state *GameState);

void Update(game_state *GameState);
void UpdateDispatcher(taxibot_dispatcher *Dispatcher, astar_grid *AStarGrid);
void DispatcherRemoveOrder(taxibot_dispatcher *Dispatcher, order Order);

void UpdateOrder(order *Order);
void UpdateTaxibot(taxibot *Taxibot, astar_grid *AStarGrid);
void UpdateTaxibots(taxibot *Taxibots, int TaxibotsLength, astar_grid *AStarGrid);
void TaxibotFollowPath(taxibot *Taxibot, Tstack *Path);

void Draw(game_state *GameState);
void StartDrawing();
void DrawGUI(game_state *GameState);
void EndDrawing();
void DrawTilemap(tilemap *Tilemap);
void DrawDepots(depot *Depots);
void DrawOrder(order *Order);
void DrawOrders(Tqueue *Orders);
void DrawTaxibot(taxibot *Taxibot);
void DrawTaxibots(taxibot *Taxibots, int TaxibotsLength);

void AddTaxibot(taxibot *Taxibot, int *TaxibotsLength, depot *Depots);
void AssignOrderToTaxibot(taxibot *Taxibot, order Order);
bool IsOpenCellFunction(point Location, void *AStarGrid);
int  CompareOrders(const void *OrderA, const void *OrderB);
v2 TaxibotMoveTowardsPoint(taxibot *Taxibot, point Point);
point CreatePoint(v2 Position);
point FindParkingSpot(v2 Point, astar_grid *AStarGrid);

void DestroyDispatcher(taxibot_dispatcher *Dispatcher);
void DestroyAStarGrid(astar_grid * AStarGrid);
void DestroyGameState(game_state *GameState);
void DestroyWindow();


void HandleInput(game_state *GameState);
v2 GetTaxiBotDirection(taxibot *Taxibot, point Point);

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
			if (i % 10 == 0 || j % 5 == 0) {
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
	}

	Dispatcher->TaxibotsLength = 0;
	Dispatcher->Depots = (depot *) malloc(MAX_NUMBER_OF_DEPOTS * sizeof(depot));
	Dispatcher->DepotsLength = 0;

	CreateDepots(Dispatcher->Depots);

	// init orders
	InitQueue(&Dispatcher->Orders, sizeof(order), NULL);
	Dispatcher->OrdersLength = 0;
	return Dispatcher;
}

void CreateDepots(depot *Depots) 
{
	Depots[0].Position.X = 0;
	Depots[0].Position.Y = 0;

	Depots[1].Position.X = 44;
	Depots[1].Position.Y = 79;
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
		if (event.type == SDL_QUIT) {
			DestroyGameState(GameState);
      		DestroyWindow();
			exit(0);
    	}

    	if (event.type == SDL_KEYDOWN) {
    		switch (event.key.keysym.sym) {
    			case SDLK_RETURN:
	    			PushQueue(&GameState->Commands, &(command_type){ADD_TAXIBOT});
			    	break;

			    case SDLK_o:
		    		PushQueue(&GameState->Commands, &(command_type){ADD_ORDER});
				    break;
    		}
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
				AddTaxibot(&GameState->Dispatcher->Taxibots[GameState->Dispatcher->TaxibotsLength], &GameState->Dispatcher->TaxibotsLength,
							GameState->Dispatcher->Depots);
				break;

			case ADD_ORDER:
				CreateOrder(&GameState->Dispatcher->Orders, &GameState->Dispatcher->OrdersLength, GameState->AStarGrid);
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
		DrawDepots(GameState->Dispatcher->Depots);
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
			Taxibot->Path = FindPath(
								CreatePoint((v2) {Taxibot->Position.X / TILE_SIZE_PIXELS, Taxibot->Position.Y / TILE_SIZE_PIXELS}), 
								FindParkingSpot(Taxibot->Order.Position, AStarGrid), AStarGrid
							);
			if (Taxibot->Path != NULL) {
				Taxibot->Status = TAXIBOT_TO_ORDER;
			} else {
				Taxibot->Status = TAXIBOT_AVAILABLE;
			}
			break;

		case TAXIBOT_TO_ORDER:
			TaxibotFollowPath(Taxibot, Taxibot->Path);
			if (!Taxibot->Path || IsStackEmpty(Taxibot->Path)) {
				Taxibot->Path = FindPath(
									CreatePoint((v2) {Taxibot->Position.X / TILE_SIZE_PIXELS, Taxibot->Position.Y / TILE_SIZE_PIXELS}), 
									FindParkingSpot(Taxibot->Order.Destination, AStarGrid), AStarGrid
								);		
				if (Taxibot->Path != NULL) {
					Taxibot->Status = TAXIBOT_TO_DEST;
					Taxibot->Order.Status = IN_TRANSIT;
				} else {
					Taxibot->Status = TAXIBOT_AVAILABLE;
				}
			}
			break;

		case TAXIBOT_TO_DEST:
			TaxibotFollowPath(Taxibot, Taxibot->Path);
			if (!Taxibot->Path || IsStackEmpty(Taxibot->Path)) {
				Taxibot->Status = TAXIBOT_AVAILABLE;
				Taxibot->Order.Status = ARRIVED;
			}
			break;	
	}
}

void TaxibotFollowPath(taxibot *Taxibot, Tstack *Path)
{
	if (!Path || IsStackEmpty(Taxibot->Path)) {
		return;
	}

	static v2 CurrentPointDestination = {0};
	if (CurrentPointDestination.X == 0 && CurrentPointDestination.Y == 0) {
		CurrentPointDestination = Taxibot->Position;
	}

	v2 Distance = TaxibotMoveTowardsPoint(Taxibot, CreatePoint(CurrentPointDestination));
	printf("Distance: %f %f\n", Distance.X, Distance.Y);
	if (Distance.X < TAXIBOT_SPEED && Distance.Y < TAXIBOT_SPEED) {
		Tstack *stack = PopStack(&Taxibot->Path); // TODooooooooooooooooooooo WTFF??????!!!!!!!!!!!!
		CurrentPointDestination = (v2) {stack->Data.Location.Row * TILE_SIZE_PIXELS + TILE_SIZE_PIXELS/2, stack->Data.Location.Col * TILE_SIZE_PIXELS + TILE_SIZE_PIXELS/2};
		
		free(stack); 
	}
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

point CreatePoint(v2 Position)
{	
	point Point = {0};
	Point.Row = (int) (Position.X);
	Point.Col = (int) (Position.Y);

	return Point;
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

	if ((DirectionX == 1 || DirectionX == -1) && (DirectionY == 1 || DirectionY == -1))
		printf("");

	return (v2) {DirectionX, DirectionY};
}

v2 TaxibotMoveTowardsPoint(taxibot *Taxibot, point Point) 
{
	v2 Distance = (v2) { abs(Taxibot->Position.X - Point.Row), 
		                 abs(Taxibot->Position.Y - Point.Col)};

	if (Distance.X < TAXIBOT_SPEED && Distance.Y < TAXIBOT_SPEED) {
		return Distance;
	}

	printf("taxi: %f %f\n", Taxibot->Position.X, Taxibot->Position.Y);
	printf("point: %d %d\n", Point.Row, Point.Col);
	Taxibot->Direction = GetTaxiBotDirection(Taxibot, Point);
	printf("Direct : %f %f\n", Taxibot->Direction.X, Taxibot->Direction.Y);
	
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

void AddTaxibot(taxibot *Taxibot, int *TaxibotsLength, depot *Depots)
{	
	if ((*TaxibotsLength) >= MAX_NUMBER_OF_TAXIBOTS)
		return;

	int i = rand() % MAX_NUMBER_OF_DEPOTS;
	Taxibot->Position.X = TILE_SIZE_PIXELS * Depots[i].Position.X + TILE_SIZE_PIXELS/2;
	Taxibot->Position.Y = TILE_SIZE_PIXELS * Depots[i].Position.Y + TILE_SIZE_PIXELS/2;
	Taxibot->Status = TAXIBOT_AVAILABLE;
	Taxibot->Path = NULL;
	(*TaxibotsLength)++;
}

void ShowQueue(Tqueue *Queue)
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
			// ShowQueue(Orders);
		} else {
			printf("Invalid order\n");
		}
	}
}

int CompareOrders(const void *OrderA, const void *OrderB)
{
	// CAST LA ORDER SI DIFERENTA PE TIMESTAMPS
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

void DrawDepots(depot *Depots)
{
	for (int i = 0; i < MAX_NUMBER_OF_DEPOTS; i++) {
		SDL_FRect r = {.x = TILE_SIZE_PIXELS * Depots[i].Position.Y, 
					   .y = TILE_SIZE_PIXELS * Depots[i].Position.X, 
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

// Tstack *Path = NULL;
// Tstack *TaxibotPath = NULL;
// taxibot Taxibot = {0, 0, 2.0};
// point Point = {0, 0};
// int First = 0;

// void SearchForPath(game_state *GameState, point Start, point End) {
// 	Taxibot.X = 0;
// 	Taxibot.Y = 0;
// 	Point.Row = 0;
// 	Point.Col = 0;
// 	First = 0;

// 	DestroyStack(&Path);
// 	Path = FindPath(Start, End, GameState->AStarGrid);
// 	ClonePath();
// }

// void GetTaxiCoordinates() {
// 	if (TaxibotPath != NULL || MoveTowardsPoint(Point, Taxibot) == 0) {
// 		if (MoveTowardsPoint(Point, Taxibot) == 1) {
// 			Point  = PopStack(&TaxibotPath)->Data.Location;
// 		} 

// 		if (First == 0) {
// 			Taxibot.X = Point.Row * TILE_SIZE_PIXELS;
// 			Taxibot.Y = Point.Col * TILE_SIZE_PIXELS;
// 			First++;
// 		}

// 		Taxibot.DirectionX = GetTaxiDirection(Point.Row, Taxibot.X);
// 		Taxibot.DirectionY = GetTaxiDirection(Point.Col, Taxibot.Y);
// 		Taxibot.X += Taxibot.DirectionX * Taxibot.Speed; 
// 		Taxibot.Y += Taxibot.DirectionY * Taxibot.Speed;
// 	}
// }

// point * FindParkingSpot(astar_grid *AStarGrid, point Point) {
// 	for (int add_Row = -1; add_Row <= 1; add_Row++) {
//     	for (int add_Col = -1; add_Col <= 1; add_Col++) {
//     		point * ParkingSpot = calloc(1, sizeof(point));
//     		if (ParkingSpot == NULL) {
//     			return NULL;
//     		}

//     		ParkingSpot->Row = Point.Row + add_Row;
//     		ParkingSpot->Col = Point.Col + add_Col;
//     		if (IsNeighbour(Point, *ParkingSpot, AStarGrid) && AStarGrid->Map[ParkingSpot->Row][ParkingSpot->Col].MovementCost == 1) {
//     			return ParkingSpot;
//     		}
//     	}
//     }

//     return NULL;
// }

// void GetPassengerPosition(game_state *GameState, point *Position) {
// 	point *ParkingSpot = NULL;

// 	do {
// 		Position->Row = rand() % (GameState->AStarGrid->NumberRows + 1);
// 		Position->Col = rand() % (GameState->AStarGrid->NumberCols + 1);
// 		ParkingSpot = FindParkingSpot(GameState->AStarGrid, *Position);
// 	} while (GetCell(*Position, GameState->AStarGrid) == NULL ||
// 			 ParkingSpot == NULL ||
// 			 GameState->AStarGrid->Map[Position->Row][Position->Col].MovementCost != 0);

// 	free(ParkingSpot);
// }

// passenger GeneratePassenger(game_state *GameState){
// 	passenger Passenger = {};
// 	Passenger.Status = WAITING;

// 	GetPassengerPosition(GameState, &Passenger.Command.Start);
// 	GetPassengerPosition(GameState, &Passenger.Command.Destination);

// 	return Passenger;
// }

// point Aux = {};
// passenger Passenger = {};	
// command Command = {};
// Tqueue ListOfCommands;

// int CompareLocations(const void *Value1, const void *Value2) {
// 	if (((point*)(Value1))->Row < ((point*)(Value2))->Row) {
// 		return 1;
// 	} else if (((point*)(Value1))->Col < ((point*)(Value2))->Col) {
// 		return 1;
// 	}
// 	return 0;
// }

// void ShowQueue(Tqueue *Queue) {
// 	node *Temp = Queue->head;
//     while (Temp != NULL) {
//         printf("(%d %d) (%d %d)->", ((command*)(Temp->Data))->Start.Row, ((command*)((Temp)->Data))->Start.Col,
//          						((command*)((Temp)->Data))->Destination.Row, ((command*)((Temp)->Data))->Destination.Col);
//         Temp = Temp->next;
//     }
// }

// int q = 0;
// void CommandsList(game_state *GameState) {
// 	point *ParkingSpotLocation = NULL;
// 	point *ParkingSpotDestination = NULL;
// 	while (q < 10) {
// 		Passenger = GeneratePassenger(GameState);
// 		printf("?????(%d %d) (%d %d)\n", Passenger.Command.Start.Row,  Passenger.Command.Start.Col,
//          						 Passenger.Command.Destination.Row,  Passenger.Command.Destination.Col);
// 		// ParkingSpotLocation = FindParkingSpot(GameState->AStarGrid, Passenger.Command.Start);
// 		// ParkingSpotDestination = FindParkingSpot(GameState->AStarGrid, Passenger.Command.Destination);

// 		// Passenger.Command.Start.Row = ParkingSpotLocation->Row;
// 		// Passenger.Start.Col = ParkingSpotLocation->Col;
// 		// Passenger.Destination.Row = ParkingSpotDestination->Row;
// 		// Passenger.Destination.Col = ParkingSpotDestination->Col;
// 		// free(ParkingSpotLocation);
// 		// free(ParkingSpotDestination);

// 		PushQueue(&ListOfCommands, &Passenger);
// 		q++;
// 	}

// 	printf("(========)\n");
// 	ShowQueue(&ListOfCommands);
// 	printf("=======\n");
// }


// void DrawPath(Tstack **FoundPath)
// {	
// 	Tstack *temp = *FoundPath;
// 	while (temp != NULL) {
// 		cell Cell = temp->Data;
// 		SDL_FRect r = {.x = TILE_SIZE_PIXELS * Cell.Location.Col, 
// 					   .y = TILE_SIZE_PIXELS * Cell.Location.Row, 
// 					   .w = TILE_SIZE_PIXELS, .h = TILE_SIZE_PIXELS};
// 		DrawRectangle(r, 0, 0, 205);	
// 		temp = temp->next;
// 	}
// }

// void ClonePath() 
// {
// 	Tstack *temp = NULL;;
// 	while (!IsStackEmpty(Path)) {
// 		cell Cell = PopStack(&Path)->Data;
// 		PushStack(&temp, Cell);
// 	}

// 	while (!IsStackEmpty(temp)) {
// 		cell Cell = PopStack(&temp)->Data;
// 		PushStack(&Path, Cell);
// 		PushStack(&TaxibotPath, Cell);
// 	}
// }



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


