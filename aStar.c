#include "aStar.h"

cell * GetCell(int X, int Y, astar_grid *Grid) {
    if ((X >= 0) && (X < Grid->NumberRows)
         && (Y >= 0) && (Y < Grid->NumberCols)) {
        DEBUG_PRINTL("astar grid value: [%d][%d] = %d ", X, Y, Grid->Map[X][Y]);
        return &Grid->Map[X][Y];
    }
    
    return NULL;
} 

bool EqualPoints(point PointA, point PointB) {
    if (PointA.Row == PointB.Row && PointA.Col == PointB.Col) 
        return 1;
    return 0;
}

bool IsNeighbour(point Location, point Neighbour, astar_grid *Grid) {
    if (Neighbour.Row < 0 || Neighbour.Row >= Grid->NumberRows || Neighbour.Col < 0 || Neighbour.Col >= Grid->NumberCols) {
        return false;
    }

    if (Location.Col == Neighbour.Col && (Location.Row == Neighbour.Row + 1 || Location.Row == Neighbour.Row - 1))
        return true;
    if (Location.Row == Neighbour.Row && (Location.Col == Neighbour.Col + 1 || Location.Col == Neighbour.Col - 1))
        return true;
    
    return false;
}

double CalculateHeuristic(point Source, point Dest, cell Node) {
    double hNew = (abs(Source.Row - Dest.Row) + abs(Source.Col - Dest.Col));
    double gNew = Node.g + 1.0;
    return (gNew + hNew);
}

void CloneQueue(Tqueue *Queue, Tqueue *Clone) {
    node *temp = Queue->head;
    while(temp != NULL) {
        PushQueue(Clone, temp->Data);
        temp = temp->next;
    }
}

void InitQueue(Tqueue *Queue, size_t memSize, compare_function Function)
{
   Queue->CompareFunction = Function;
   Queue->memSize = memSize;
   Queue->head  = NULL;
}

void PrintQueue(Tqueue *Queue) {
    node *Temp = Queue->head;
    while (Temp != NULL) {
        DEBUG_PRINT("(%d %d %.0f)->", ((cell*)((Temp)->Data))->Location.Row, ((cell*)((Temp)->Data))->Location.Col, ((cell*)((Temp)->Data))->f);
        Temp = Temp->next;
    }
}

int ComparePriority(const void *x1, const void *x2) {
    if (((cell*)(((node*)(x1))->Data))->f > ((cell*)(x2))->f)
        return 1;
    return 0;
}

void PushQueue(Tqueue *Queue, void* data) {
    node *newNode = (node*) malloc(sizeof(node));
    node *temp = Queue->head;

    if(newNode == NULL) {
        return -1;
    }

    newNode->Data = malloc(Queue->memSize);

    if(newNode->Data == NULL) {
        free(newNode);
        return -1;
    }

    newNode->next = NULL;

    memcpy(newNode->Data, data, Queue->memSize);

    if(IsQueueEmpty(Queue)) {
        Queue->head  = newNode;
    } else {
        if (Queue->CompareFunction == NULL) {
            while (temp->next != NULL) {
                temp = temp->next;
            }
            
            newNode->next = temp->next;
            temp->next = newNode;
            return;
        }

        if (Queue->CompareFunction((void*)(temp), data) == 1) {
            newNode->next = Queue->head;
            Queue->head = newNode;
        } else {
            while (temp->next != NULL && Queue->CompareFunction((void*)(temp), data) == 0) {
                temp = temp->next;
            }

            newNode->next = temp->next;
            temp->next = newNode;
        }
    }
}

void PeekQueue(Tqueue *Queue, void *data) {
   if(!IsQueueEmpty(Queue)) {
       node *temp = Queue->head;
       memcpy(data, temp->Data, Queue->memSize);
    }
}

void PopQueue(Tqueue *Queue) { 
    if(!IsQueueEmpty(Queue)) {
        node *temp = Queue->head;
        // memcpy(data, temp->data, Queue->memSize);

        if(Queue->head->next != NULL) {
            Queue->head = Queue->head->next;
        } else {
            Queue->head = NULL;
        }

        free(temp->Data);
        free(temp);
    }
}

int IsQueueEmpty(Tqueue *Queue) {
    if (Queue->head == NULL) 
        return 1;
    return 0;
}

void DestroyQueue(Tqueue *Queue) {
    node *temp;

     while(!IsQueueEmpty(Queue)) {
        temp = Queue->head;
        Queue->head = temp->next;
        free(temp->Data);
        free(temp);
    }

    Queue->head = NULL;
}

Tstack * NewStackNode(cell Node) {
    Tstack *NewNode = malloc(sizeof(Tstack));
    NewNode->Data.f = Node.f;
    NewNode->Data.g = Node.g;
    NewNode->Data.h = Node.h;
    NewNode->Data.Location.Row = Node.Location.Row;
    NewNode->Data.Location.Col = Node.Location.Col;
    NewNode->next = NULL;

    return NewNode;
}


int IsStackEmpty(Tstack *Stack) {
    if (Stack == NULL) 
        return 1;
    return 0;
}

void PushStack(Tstack **Stack, cell Node) {
    Tstack *NewNode = NewStackNode(Node);

    if (IsStackEmpty(*Stack)) {
        (*Stack) = NewNode;
    } else {
        NewNode->next = (*Stack);
        (*Stack) = NewNode;
    }
}

Tstack * PeekStack(Tstack **Stack) {
    return (*Stack);
}

Tstack * PopStack(Tstack **Stack) {
    Tstack *Temp = (*Stack);
    (*Stack) = (*Stack)->next;

    return Temp;
}

void PrintStack(Tstack *Stack) {
    Tstack *Temp = Stack;
    while (Temp != NULL) {
        DEBUG_PRINT("-> (%d,%d)", Temp->Data.Location.Row, Temp->Data.Location.Col);
        Temp = Temp->next;
    }
    DEBUG_PRINT("\n");
}

void DestroyStack(Tstack **Stack) {
    Tstack *Temp = NULL;
    while((*Stack) != NULL) {
        Temp = *Stack;
        (*Stack) = (*Stack)->next;
        free(Temp);
    }

   *Stack = NULL;
}

Tstack * TracePath(point Dest, astar_grid *Grid) {
    Tstack *stack = NULL;
 
    int r = Dest.Row;
    int c = Dest.Col;
    cell NextNode = {};
    NextNode.Location.Row = r;
    NextNode.Location.Col = c;

    PushStack(&stack, NextNode);

    while(!(Grid->Map[r][c].Location.Row == r && Grid->Map[r][c].Location.Col == c)) {
        NextNode.Location.Row = Grid->Map[r][c].Location.Row;
        NextNode.Location.Col = Grid->Map[r][c].Location.Col;
        PushStack(&stack, NextNode);
        r = NextNode.Location.Row;
        c = NextNode.Location.Col;
    }

    return stack;
}

bool ElementIsInOpenList(Tqueue *Queue, cell Node) {
    node *Temp = Queue->head;
    while (Temp != NULL) {
        if (((cell*)((Temp)->Data))->f == Node.f 
            && (((cell*)((Temp)->Data))->Location.Row == Node.Location.Row) 
            && (((cell*)((Temp)->Data))->Location.Col == Node.Location.Col)) {
            return true;
        }

        Temp = Temp->next;
    }

    return false;
}

Tstack * FindPath(point Start, point End, astar_grid *Grid) {
    if (Grid->IsOpenCellFunction(Start, Grid) == false || Grid->IsOpenCellFunction(End, Grid) == false) {
        DEBUG_PRINTL("Source or Destination is blocked\n");
        return NULL;
    }

    if (EqualPoints(Start, End)) {
        DEBUG_PRINTL("We are already at the Destination\n");
        return NULL;
    }

    for (int i = 0; i < Grid->NumberRows; i++) {
        for (int j = 0; j < Grid->NumberCols; j++) {
            Grid->Map[i][j].f = -1;
            Grid->Map[i][j].g = 0.0;
            Grid->Map[i][j].h = 0.0;
        }
    }
    point RefCoord = {Start.Row, Start.Col};
    Grid->Map[Start.Row][Start.Col].Location.Row = RefCoord.Row;
    Grid->Map[Start.Row][Start.Col].Location.Col = RefCoord.Col;
    Grid->Map[Start.Row][Start.Col].f = 0.0;
    Grid->Map[Start.Row][Start.Col].g = 0.0;
    Grid->Map[Start.Row][Start.Col].h = 0.0;

    Tqueue OpenList;
    InitQueue(&OpenList, sizeof(cell), ComparePriority);

    bool ClosedList[Grid->NumberRows][Grid->NumberCols];
    memset(ClosedList, false, sizeof(ClosedList));

    PushQueue(&OpenList, &Grid->Map[Start.Row][Start.Col]);

    while (!IsQueueEmpty(&OpenList)) {
        cell PeekedCell;
        PeekQueue(&OpenList, &PeekedCell);
        RefCoord.Row = PeekedCell.Location.Row;
        RefCoord.Col = PeekedCell.Location.Col;
        PopQueue(&OpenList);
        ClosedList[RefCoord.Row][RefCoord.Col] = true;

        /*
                Generating all the 8 successor of this cell
                         N 
                         | 
                         | 
                   W----Cell----E
                         |   
                         | 
                         S 
 
                Cell-->Popped Cell (i, j)
                N --> North     (i-1, j)
                S --> South     (i+1, j)
                E --> East      (i, j+1)
                W --> West      (i, j-1)

        */

        for (int add_Row = -1; add_Row <= 1; add_Row++) {
            for (int add_Col = -1; add_Col <= 1; add_Col++) {
                point Neighbour = {RefCoord.Row + add_Row, RefCoord.Col + add_Col};
                if (IsNeighbour(RefCoord, Neighbour, Grid)) {
                    if (EqualPoints(Neighbour, End)) {
                        Grid->Map[Neighbour.Row][Neighbour.Col].Location.Row = RefCoord.Row;
                        Grid->Map[Neighbour.Row][Neighbour.Col].Location.Col = RefCoord.Col;
                        // printf("The Destination cell has been found\n");
                        Tstack *FinalPath  = TracePath(End, Grid);
                        DestroyQueue(&OpenList);
                        return FinalPath;
                    } else if (!ClosedList[Neighbour.Row][Neighbour.Col] && (Grid->IsOpenCellFunction(Neighbour, Grid) == true)) {
                        double fNew = CalculateHeuristic(Neighbour, End, Grid->Map[RefCoord.Row][RefCoord.Col]);

                        if (Grid->Map[Neighbour.Row][Neighbour.Col].f > fNew || 
                            Grid->Map[Neighbour.Row][Neighbour.Col].f < 0) {
                            
                            Grid->Map[Neighbour.Row][Neighbour.Col].Location.Row = Neighbour.Row;
                            Grid->Map[Neighbour.Row][Neighbour.Col].Location.Col = Neighbour.Col;
                            Grid->Map[Neighbour.Row][Neighbour.Col].f = fNew;
                            PushQueue(&OpenList, &Grid->Map[Neighbour.Row][Neighbour.Col]);

                            // Update the details of this cell
                            Grid->Map[Neighbour.Row][Neighbour.Col].g = Grid->Map[RefCoord.Row][RefCoord.Col].g + 1.0;
                            Grid->Map[Neighbour.Row][Neighbour.Col].h = (abs(Neighbour.Row - End.Row) + abs(Neighbour.Col - End.Col));;
                            Grid->Map[Neighbour.Row][Neighbour.Col].Location.Row = RefCoord.Row;
                            Grid->Map[Neighbour.Row][Neighbour.Col].Location.Col = RefCoord.Col;
                        }
                    }
                }
            }
        }
    }

    DEBUG_PRINTL("Failed to find the Destination Cell\n");
    return NULL;
}
