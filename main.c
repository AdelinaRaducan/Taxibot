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

const int MAX_NUMBER_OF_TAXIBOTS = 1;
const int MAX_NUMBER_OF_ORDERS = 20;
const int MAX_NUMBER_OF_DEPOTS = 2;

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
	AVAILABLE,
	WORKING,
	CHARGING,
	TO_ORDER_LOCATION,
	TO_DESTINATION
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

typedef struct taxibot {
	v2 Position;
	double Speed;
	int DirectionX, DirectionY;
	order *Order;
	taxibot_status Status;
	Tstack *Path;
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
void UpdateOrder(order *Order);
void UpdateTaxibot(taxibot *Taxibot, astar_grid *AStarGrid);
void UpdateTaxibots(taxibot *Taxibots, int TaxibotsLength, astar_grid *AStarGrid);

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
void AssignOrderToTaxibot(taxibot *Taxibot, order *Order);
bool IsOpenCellFunction(point Location, void *AStarGrid);
int  CompareOrders(const void *OrderA, const void *OrderB);
int  MoveTowardsPoint(v2 Position, taxibot Taxibot);
point CreatePoint(v2 Position);
point FindParkingSpot(astar_grid *AStarGrid, v2 Point);
void FilterCommands(Tqueue *Queue);

void DestroyDispatcher(taxibot_dispatcher *Dispatcher);
void DestroyAStarGrid(astar_grid * AStarGrid);
void DestroyGameState(game_state *GameState);
void DestroyWindow();


void HandleInput(game_state *GameState);
int  GetTaxiDirection(int Value1, int Value2);

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
	for (int i = 0; i < MAX_NUMBER_OF_TAXIBOTS; i++) {
		Dispatcher->Taxibots[i].Order = NULL;
	}

	Dispatcher->TaxibotsLength = 0;
	Dispatcher->Depots = (depot *) malloc(MAX_NUMBER_OF_DEPOTS * sizeof(depot));
	Dispatcher->DepotsLength = 0;
	CreateDepots(Dispatcher->Depots);
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

// TODO 0.0: Make use of the brain!!!! We code small and test frequently!!!!
// TODO 0.1: First I implement the adding of taxibots, and I test if the drawing is ok with only 1 taxibot
// TODO 0.2: I implement adding order at random places, I test that are drawed correctly
// TODO 0.3: With only 1 taxibot the dispatcher should give him order after order!!!!!!!!!!!!!!!!!!!!!!
// TODO 0.4: After I test with 2 taxibots and 1 command
// TODO 0.5: After I test with 2 taxibots and 2 command


// TODO 2: define AddTaxibot function, don't forget about TaxibotLength++ 

// TODO 3.0: an order should have a timestamp field, based on that we compare orders priority
// TODO 3: define CreateOrder, push to queue, first every order should be in WAITING status

// TODO 3.1: define DrawTaxibot(taxibot *TAxibot)
// TODO 3.2: define DrawOrder(order *Order)

// TODO 4: When I press a button, a new taxibot is created & starts from a depot location -> SDL how to read keyboards pressed

// TODO 5: When I press a button, an order is created at a random spot

// TODO 6: At each update the dispatcher delegates orders to taxibots
// TODO 6.1: Delegates == pop an order from the queue based on priority !!!!ONLY!!!! if it has available/free taxibots
// TODO 6.2: If I have more available taxibots who I assign the order? Who is the closest!!! Functie proasta care face diff pe pozitii

// TODO 6.3: TOATE FUNCTIILE DE CREATE SUCCESIVE, TOATE DE UPDATE SUCCESIVE, TOATE DE DRAW SUCCESIVE, TOATE DE DESTROY SUCCESIVE...IN ORDINEA ASTA
// TODO 6.4: FUNCTIILE PRINCIPALE DE UPDATE, DRAW, SA NU CONTINA MAI MULT DE 10 LINII....DEFINESTE FUNCTII!!!!!!!!

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
			    	printf("<RETURN> is pressed.\n");
			    	break;

			    case SDLK_o:
		    		PushQueue(&GameState->Commands, &(command_type){ADD_ORDER});
				    printf("<O> is pressed.\n");
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
	if (!IsQueueEmpty(&Dispatcher->Orders)) {
		FilterCommands(&Dispatcher->Orders);
	} 

	if (IsQueueEmpty(&Dispatcher->Orders)) {
		return;
	}


	Tqueue CloneOrders;
	InitQueue(&CloneOrders, sizeof(order), NULL);
	CloneQueue(&Dispatcher->Orders, &CloneOrders);

	for (int i = 0; i < Dispatcher->TaxibotsLength; i++) {
		if (IsQueueEmpty(&Dispatcher->Orders)) {
			break;
		}

		if (Dispatcher->Taxibots[i].Status == AVAILABLE) {
			order Order = {0};
			PeekQueue(&CloneOrders, &Order);
			PopQueue(&CloneOrders);
			PopQueue(&Dispatcher->Orders);
			AssignOrderToTaxibot(&Dispatcher->Taxibots[i], &Order);
			Dispatcher->OrdersLength--;
		}
	}

	UpdateTaxibots(Dispatcher->Taxibots, Dispatcher->TaxibotsLength, AStarGrid);
}

void FilterCommands(Tqueue *Queue) 
{
	node *temp = NULL;
	node *prev = NULL;
	node *aux = NULL;

	while (((order*)(Queue->head->Data))->Status == ARRIVED) {
		aux = Queue->head;
		Queue->head = aux->next;
		free(aux->Data);
		free(aux);
	} 

	temp = Queue->head->next;
	prev = temp;
	while (temp != NULL) {
		if (((order*)((node*)(temp->Data)))->Status == ARRIVED) {
			aux = temp;
			prev->next = aux->next;
			free(aux->Data);
			free(aux);
		} else {
			temp = temp->next;
		}

		prev = temp;
	}
}

void AssignOrderToTaxibot(taxibot *Taxibot, order *Order)
{	
	Taxibot->Order = Order;
	Taxibot->Status = WORKING;
}

void UpdateTaxibots(taxibot *Taxibots, int TaxibotsLength, astar_grid *AStarGrid) 
{
	for (int i = 0; i < TaxibotsLength; i++) {
		UpdateTaxibot(&Taxibots[i], AStarGrid);
	}
}

void UpdateTaxibot(taxibot *Taxibot, astar_grid *AStarGrid)
{	
	point Start = {0};
	point End = {0};
	if (Taxibot->Status == WORKING && IsStackEmpty(Taxibot->Path)) {
		Start  = CreatePoint(Taxibot->Position);
		End = FindParkingSpot(AStarGrid, Taxibot->Order->Position);
		Taxibot->Status = TO_ORDER_LOCATION;
		printf("->To passenger: Taxi(%d %d) Dest(%d %d)\n\n", Start.Row, Start.Col, End.Row, End.Col);
		Taxibot->Path = FindPath(Start, End, AStarGrid);
	}

	if (Taxibot->Status == TO_ORDER_LOCATION && IsStackEmpty(Taxibot->Path)) {
		Start = FindParkingSpot(AStarGrid, Taxibot->Order->Position);
		End = FindParkingSpot(AStarGrid, Taxibot->Order->Destination);
		printf("->To final dest: Taxi(%d %d)  Dest(%d %d)\n\n", Start.Row, Start.Col, End.Row, End.Col);
		Taxibot->Status = TO_DESTINATION;
		Taxibot->Path = FindPath(Start, End, AStarGrid);
	}

	if (!IsStackEmpty(Taxibot->Path)) {
		Tstack *Cell = PopStack(&Taxibot->Path);
		Taxibot->Position.X = Cell->Data.Location.Row;
		Taxibot->Position.Y = Cell->Data.Location.Col;
		free(Cell);
	} else {
		if (Taxibot->Status == TO_DESTINATION) {
			Taxibot->Status = AVAILABLE;
			Taxibot->Order->Status = ARRIVED;
			Taxibot->Order = NULL;
		}
	}

}

point FindParkingSpot(astar_grid *AStarGrid, v2 Location) {
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

int GetTaxiDirection(int Value1, int Value2) 
{
	if (Value1 * TILE_SIZE_PIXELS - Value2 > 0) {
		return 1;
	} else if (Value1 * TILE_SIZE_PIXELS - Value2 < 0) {
		return -1;
	} 

	return 0;
}

int MoveTowardsPoint(v2 Position, taxibot Taxibot) 
{	
	if (Position.X * TILE_SIZE_PIXELS == Taxibot.Position.X && Position.Y * TILE_SIZE_PIXELS == Taxibot.Position.Y) {
		return 1;
	}

	return 0;
}

void AddTaxibot(taxibot *Taxibot, int *TaxibotsLength, depot *Depots)
{	
	if ((*TaxibotsLength) < MAX_NUMBER_OF_TAXIBOTS) {
		int i = rand() % MAX_NUMBER_OF_DEPOTS;
		Taxibot->Position.X = Depots[i].Position.X;
		Taxibot->Position.Y = Depots[i].Position.Y;
		Taxibot->Status = AVAILABLE;
		Taxibot->Path = NULL;
		(*TaxibotsLength)++;

		// printf("Taxi adaugat::%.0f %.0f\n", Taxibot->Position.X, Taxibot->Position.Y);
	}
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
	SDL_FRect r = {.x = TILE_SIZE_PIXELS * Taxibot->Position.Y, 
				   .y = TILE_SIZE_PIXELS * Taxibot->Position.X, 
				   .w = TILE_SIZE_PIXELS, .h = TILE_SIZE_PIXELS};

	DrawRectangle(r, 248, 215, 99);
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
	if (Order->Status != WAITING) 
		return;

	SDL_FRect r = {.x = TILE_SIZE_PIXELS * Order->Position.Y + (TILE_SIZE_PIXELS * 0.25), 
				   .y = TILE_SIZE_PIXELS * Order->Position.X - (TILE_SIZE_PIXELS * 0.25), 
				   .w = TILE_SIZE_PIXELS / 2, .h = TILE_SIZE_PIXELS / 2};


	DrawRectangle(r, 10, 215, 99);

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


