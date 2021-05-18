#include "aStar.h"

cell * GetCell(point Location, astar_grid *Grid) {
    if ((Location.Row >= 0) && (Location.Row < Grid->NumberCols)
         && (Location.Col >= 0) && (Location.Col < Grid->NumberRows)) {
        return &Grid->Map[Location.Row][Location.Col];
    }
    
    return NULL;
}

bool EqualPoints(point PointA, point PointB) {
    if (PointA.Row == PointB.Row && PointA.Col == PointB.Col) 
        return 1;
    return 0;
}

bool IsNeighbour(point Location, point Neighbour, astar_grid *Grid) {
    if (GetCell(Neighbour, Grid) == NULL) 
        return false;

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

void PrintQueue(Tqueue **Queue) {
    Tqueue *Temp = *Queue;
    while (Temp != NULL) {
        printf("r:%d c:%d f:%.2f\n", Temp->Data.Location.Row, Temp->Data.Location.Col, Temp->Data.f);
        Temp = Temp->next;
    }
}

Tqueue * NewQueueNode(cell Node) {
    Tqueue *NewNode = malloc(sizeof(Tqueue));
    NewNode->Data.f = Node.f;
    NewNode->Data.Location.Row = Node.Location.Row;
    NewNode->Data.Location.Col = Node.Location.Col;
    NewNode->next = NULL;

    return NewNode;
}

void PushQueue(Tqueue **Queue, cell Node) {
    Tqueue *NewNode = NewQueueNode(Node);
    Tqueue *Start = *Queue;
    if (IsQueueEmpty(*Queue)) {
        (*Queue) = NewNode;
        return;
    }

    if ((*Queue)->Data.f > NewNode->Data.f) {
        NewNode->next = *Queue;
        (*Queue) = NewNode;
    } else {
        while (Start->next != NULL && Start->next->Data.f < NewNode->Data.f) {
            Start = Start->next;
        }

        NewNode->next = Start->next;
        Start->next = NewNode;
    }
}

Tqueue * PeekQueue(Tqueue **Queue) {
    return (*Queue);
}

Tqueue * PopQueue(Tqueue **Queue) { 
    Tqueue *Temp = (*Queue);
    (*Queue) = (*Queue)->next;

    return Temp;
}

int IsQueueEmpty(Tqueue *Queue) {
    if (Queue == NULL) 
        return 1;
    return 0;
}

void DestroyQueue(Tqueue **Queue) {
    Tqueue *Temp = NULL;
    while((*Queue) != NULL) {
        Temp = *Queue;
        (*Queue) = (*Queue)->next;
        free(Temp);
    }
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

void PrintStack(Tstack **Stack) {
    Tstack *Temp = *Stack;
    while (Temp != NULL) {
        printf("-> (%d,%d)", Temp->Data.Location.Row, Temp->Data.Location.Col);
        Temp = Temp->next;
    }
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
    Tqueue *Temp = Queue;
    while (Temp != NULL) {
        if (Temp->Data.f == Node.f 
            && (Temp->Data.Location.Row == Node.Location.Row) 
            && (Temp->Data.Location.Col == Node.Location.Col)) {
            return true;
        }

        Temp = Temp->next;
    }

    return false;
}

Tstack * FindPath(point Start, point End, astar_grid *Grid) {
    if (GetCell(Start, Grid) == NULL) {
        printf("Invalid source\n");
        return NULL;
    }

    if (GetCell(End, Grid) == NULL) {
        printf("Invalid Destination\n");
        return NULL;
    }

    if (!Grid->IsOpenCellFunction(Start, (void*)(Grid)) || !Grid->IsOpenCellFunction(End, (void*)(Grid))) {
        printf("Source or Destination is blocked\n");
        return NULL;
    }

    if (EqualPoints(Start, End)) {
        printf("We are already at the Destination\n");
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

    Tqueue *OpenList = NULL;
    bool ClosedList[Grid->NumberRows][Grid->NumberCols];
    memset(ClosedList, false, sizeof(ClosedList));

    PushQueue(&OpenList, Grid->Map[Start.Row][Start.Col]);

    while (!IsQueueEmpty(OpenList)) {
        Tqueue *PeekedCell = PeekQueue(&OpenList);
        RefCoord.Row = PeekedCell->Data.Location.Row;
        RefCoord.Col = PeekedCell->Data.Location.Col;
        Tqueue *PoopedCell = PopQueue(&OpenList);
        free(PoopedCell);
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
                        printf("The Destination cell has been found\n");
                        Tstack *FinalPath  = TracePath(End, Grid);
                        DestroyQueue(&OpenList);
                        return FinalPath;
                    } else if (!ClosedList[Neighbour.Row][Neighbour.Col] && Grid->IsOpenCellFunction(Neighbour, (void*)(Grid))) {
                        double fNew = CalculateHeuristic(Neighbour, End, Grid->Map[RefCoord.Row][RefCoord.Col]);

                        if (Grid->Map[Neighbour.Row][Neighbour.Col].f > fNew || 
                            Grid->Map[Neighbour.Row][Neighbour.Col].f < 0) {
                            
                            Grid->Map[Neighbour.Row][Neighbour.Col].Location.Row = Neighbour.Row;
                            Grid->Map[Neighbour.Row][Neighbour.Col].Location.Col = Neighbour.Col;
                            Grid->Map[Neighbour.Row][Neighbour.Col].f = fNew;
                            PushQueue(&OpenList, Grid->Map[Neighbour.Row][Neighbour.Col]);

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

    printf("Failed to find the Destination Cell\n");
    return NULL;
}
