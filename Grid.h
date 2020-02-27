#ifndef GRID_H_INCLUDED
#define GRID_H_INCLUDED

#include "Cells.h"
#include "AABB.h"
#include "HashTable.h"

typedef struct Grid
{
    double cell_size;
    Box* camera;
    int width;
    int height;
    Cell* cells;
    int len;
} Grid;

Grid* makeGrid(int width, int height, double cellSize);
Collider* makeCollider(Box hitbox, Grid* mainGrid, void* sprite);
void freeCollider(Collider* collider);
void insertToGrid(Grid* grid, Collider* collider, int curr_update);
int queryBox(Grid* grid, Box box, Collider** ret_array, hashTable* table, int MAX_SIZE, int curr_update, int htable_use);

#endif // GRID_H_INCLUDED
