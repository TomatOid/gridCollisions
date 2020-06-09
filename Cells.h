#ifndef CELLS_H_INCLUDED
#define CELLS_H_INCLUDED

#include "AABB.h"
#include <stdint.h>
#ifdef _OPENMP
    #include <omp.h>
#else
    #ifndef _ESCAPE_OPENMP
        #define omp_get_num_threads() 1
        #define omp_get_thread_num() 0
        #define omp_get_max_threads() 0
        #define omp_lock_t int
        #define omp_set_lock(lck) 0
        #define omp_unset_lock(lck) 0
        #define _ESCAPE_OMPENMP
    #endif
#endif

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
    omp_lock_t lck;
    LlElem* entry;
    /* this is to keep track of what cells are no longer up to date, and the entry will be treated as null if out of date */
    uint32_t lastUpdate;
} Cell;






#endif // CELLS_H_INCLUDED
