#ifndef CELLS_H_INCLUDED
#define CELLS_H_INCLUDED

#include "AABB.h"
#include <stdint.h>

typedef struct LlElem
{
    struct Collider* obj;
    struct LlElem* next;
} LlElem;

/* Yeah, yeah, I know circular dependencies are weird, but it works because they are pointers, and I really didn't see any other choice */
// Later note: there is another option but it involves a redesign and simplification such that instead of a literal array of cell objects,
// just use a hash table indexing into it with x and y concatinated together into a uint64_t. This may improve speed as well as improve memory
// complexity

typedef struct Collider
{
    Box hitbox;
    void* sprite;
    struct LlElem* memPool;
    int memMax;
    int index; // remember to reset this every update
} Collider;

typedef struct Cell
{
    LlElem* entry;
    /* this is to keep track of what cells are no longer up to date, and the entry will be treated as null if out of date */
    uint32_t lastUpdate;
} Cell;






#endif // CELLS_H_INCLUDED
