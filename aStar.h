
typedef struct point {
	int Row;
	int Col;
} point;

typedef struct cell {
	double g, f, h;
	point Location;
	int MovementCost;
} cell;

typedef struct queue {
	cell Data;
	struct queue *next;
} Tqueue;

typedef struct stack {
	cell Data;
	struct stack *next;
} Tstack;

typedef bool (*is_open_cell_function)(point , void*);

typedef struct astar_grid {
	int NumberRows, NumberCols;
	cell **Map;
	is_open_cell_function IsOpenCellFunction;
} astar_grid;

Tstack * 			FindPath(point Start, point End, astar_grid *Grid);

static cell * 		GetCell(point Location, astar_grid *Grid);
static bool 		EqualPoints(point PointA, point PointB);
static bool 		IsNeighbour(point Location, point Neighbour, astar_grid *Grid);
double				CalculateHeuristic(point Source, point Dest, cell Node);
Tstack* 			TracePath(point Dest, astar_grid *Grid);
Tqueue*	 			NewQueueNode(cell Node);
Tqueue* 			PeekQueue(Tqueue **Queue);
Tqueue*				PopQueue(Tqueue **Queue);
void				PushQueue(Tqueue **Queue, cell Node);
int 				IsQueueEmpty(Tqueue *Queue);
void 				DestroyQueue(Tqueue **Queue);
Tstack*				NewStackNode(cell Node);
Tstack* 			PeekStack(Tstack **Stack);
Tstack*				PopStack(Tstack **Stack);
void				PushStack(Tstack **Stack, cell Node);
int 				IsStackEmpty(Tstack *Stack);
void				PrintQueue(Tqueue **Queue);
void 				PrintStack(Tstack **Stack);
void                DestroyStack(Tstack **Stack);

