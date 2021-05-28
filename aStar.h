
typedef struct point {
	int Row;
	int Col;
} point;

typedef struct cell {
	double g, f, h;
	point Location;
	int MovementCost;
} cell;

typedef struct Node {
  void *Data;
  struct Node *next;
}node;

typedef int (*compare_function)(const void *d1, const void *d2);

typedef struct QueueList {
    size_t memSize;
    node *head;
    compare_function CompareFunction;
}Tqueue;

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

static cell * 		GetCell(int X, int Y, astar_grid *Grid);
static bool 		EqualPoints(point PointA, point PointB);
static bool 		IsNeighbour(point Location, point Neighbour, astar_grid *Grid);
double				CalculateHeuristic(point Source, point Dest, cell Node);
Tstack* 			TracePath(point Dest, astar_grid *Grid);
Tstack*				NewStackNode(cell Node);
Tstack* 			PeekStack(Tstack **Stack);
Tstack*				PopStack(Tstack **Stack);
void				PushStack(Tstack **Stack, cell Node);
int 				IsStackEmpty(Tstack *Stack);
void 				PrintStack(Tstack *Stack);
void                DestroyStack(Tstack **Stack);

void 	 			InitQueue(Tqueue *Queue, size_t allocSize, compare_function Function);
void 				PeekQueue(Tqueue *Queue, void *data);
void				PopQueue(Tqueue *Queue);
void				PushQueue(Tqueue *Queue, void *data);
int 				IsQueueEmpty(Tqueue *Queue);
void 				DestroyQueue(Tqueue *Queue);
void 				PrintQueue(Tqueue *Queue);

