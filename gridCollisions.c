#include <stdlib.h>
#include "AABB.h"
#include "Cells.h"
#include "Grid.h"
#include "HashTable.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>

Grid* makeGrid(int width, int height, double cellSize)
{
    Grid* res = malloc(sizeof(Grid));
    res->width = width;
    res->height = height;
    res->cell_size = cellSize;
    // Let's do it all in one malloc for cache-friendlyness,
    // and pretend like the rest of the code is also cache-friendly
    res->cells = malloc(width * height * sizeof(Cell));
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            (res->cells + i * height + j)->entry = NULL;
            (res->cells + i * height + j)->lastUpdate = -5; // this is overflowing into 
        }
    }
    return res;
}

Collider* makeCollider(Box hitbox, Grid* mainGrid, void* sprite)
{
    if (!sprite) { printf("Sprite is null"); fflush(stdout); }
    // first, allocate some memory for the collider struct
    Collider* res = malloc(sizeof(Collider));
    if (!res) { return NULL; }
    // then, calculate the maximum number of cells the rectangle could intersect
    res->memMax = ceil(1 + (hitbox.X1 - hitbox.X0) / mainGrid->cell_size) * ceil(1 + (hitbox.Y1 - hitbox.Y0) / mainGrid->cell_size);
    res->hitbox = hitbox;
    res->sprite = sprite;
    // allocate memory for linked list elements, the idea being that they will be part of linked lists which lead back to the cells it touches
    res->memPool = malloc((res->memMax) * sizeof(LlElem));
    if (!res->memPool) { printf("Error allocating memPool. \n"); }
    res->index = 0;
    return res;
}

void freeCollider(Collider* collider)
{
    free(collider->memPool);
    free(collider);
}

void insertToGrid(Grid* grid, Collider* collider, int curr_update)
{
    if (!grid->cell_size) { return; }
    // we will find the floor of top left and ceiling of bottom right corners after converting to grid space
    int LX = floor(collider->hitbox.X0 / grid->cell_size);
    int TY = floor(collider->hitbox.Y0 / grid->cell_size);
    int RX = ceil(collider->hitbox.X1 / grid->cell_size);
    int BY = ceil(collider->hitbox.Y1 / grid->cell_size);
    // we are going to validate ahead of time
    if (LX < 0) { LX = 0; }
    if (TY < 0) { TY = 0; }
    if (RX > grid->width) { RX = grid->width; }
    if (BY > grid->height) { BY = grid->height; }
    if (!collider->memPool) { return; }
    // now that we know our variables are safe, we can loop over the collided cells
    collider->index = 0;
    Cell* cellij;
    for (int i = LX; i < RX; i++)
    {
        for (int j = TY; j < BY; j++)
        {
            // ensure that we don't write to memory we aren't supposed to
            // this fixes the buffer overflow bug
            if (collider->index >= collider->memMax) return;
            cellij = (grid->cells + i * grid->height + j);

            collider->memPool[collider->index].obj = collider;
            // possible candidate for the issues we're having
            if ((!cellij->entry) || cellij->lastUpdate != curr_update) // if there are no elements at the cell
            {
                collider->memPool[collider->index].next = NULL; // This is the first element of the linked list
            }
            else // insert the object at the begining of the linked list
            {
                
                //My best guess for the bug is that for whatever reason, the above condition is not passing when it should, and is causing
                //the next pointer to be filled with a garbage value
                
                // Follow-up: the bug was caused by buffer overflow on memPool
                collider->memPool[collider->index].next = cellij->entry;
            }
            cellij->entry = &collider->memPool[collider->index];
            cellij->lastUpdate = curr_update;

            collider->index++;
        }
    }
    return;
}

int queryBox(Grid* grid, Box box, Collider** ret_array, hashTable* table, int MAX_SIZE, int curr_update, int htable_use, int revx, int revy)
{
    // we will find the floor of top left and ceiling of bottom right corners after converting to grid space
    int LX = floor(box.X0 / grid->cell_size);
    int TY = floor(box.Y0 / grid->cell_size);
    int RX = ceil(box.X1 / grid->cell_size);
    int BY = ceil(box.Y1 / grid->cell_size);
    // we are going to validate ahead of time
    if (LX < 0) { LX = 0; }
    if (TY < 0) { TY = 0; }
    if (RX > grid->width) { RX = grid->width; }
    if (BY > grid->height) { BY = grid->height; }
    // now, if passed, we loop over the intersecting cells
    int index = 0;
    int db = 0;
    Cell* cellij;
    for (int i = LX; i < RX; i++)
    {
        for (int j = TY; j < BY; j++)
        {
            cellij = (grid->cells + ((revx ? RX - i + LX - 1 : i) * grid->height + (revy ? BY - j + TY - 1 : j)));
            if ((!cellij->entry) || (cellij->lastUpdate != curr_update)) { continue; }
            else
            {
                LlElem* elem = cellij->entry;
                db = 0;
                do
                {
                    // we need to check if the result already contains this value
                    if (!findHashItem(table, (intptr_t)(elem->obj->sprite), htable_use))
                    {
                        if (index >= MAX_SIZE) { return index; }
                        ret_array[index] = elem->obj;
                        index++;
                        insertHashItem(table, (intptr_t)elem->obj->sprite, htable_use);
                    }
                db++;
                } while ((elem = elem->next));
            }
        }
    }
    return index;
}
