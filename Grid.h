#ifndef GRID_H_INCLUDED
#define GRID_H_INCLUDED

#include "Cells.h"
#include "AABB.h"
#include "HashTable.h"

typedef struct Grid
{
    float cell_size;
    Box* camera;
    int width;
    int height;
    Cell** cells;
    int len;
} Grid;

Grid* makeGrid(int width, int height, float cellSize);
Collider* makeCollider(Box hitbox, Grid* mainGrid, void* sprite);
void insertToGrid(Grid* grid, Collider* collider, int curr_update);
int queryBox(Grid* grid, Box box, Collider** ret_array, hashTable* table, int MAX_SIZE, int curr_update, int htable_use);

#endif // GRID_H_INCLUDED
