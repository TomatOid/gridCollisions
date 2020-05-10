#ifndef HASHTABLE_H_INCLUDED
#define HASHTABLE_H_INCLUDED
#include <stdint.h>

typedef struct _hashItem
{
    uint32_t updateCount;
    intptr_t value;
} hashItem;

typedef struct _hashTable
{
    hashItem* items;
    size_t len;
    size_t num;
} hashTable;

void insertHashItem(hashTable* table, intptr_t value, int update);
int findHashItem(hashTable* table, intptr_t lookup, int update);

#endif // HASHTABLE_H_INCLUDED
