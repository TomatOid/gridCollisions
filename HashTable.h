#ifndef HASHTABLE_H_INCLUDED
#define HASHTABLE_H_INCLUDED
#include <stdint.h>

typedef struct _hashItem
{
    int updateCount;
    intptr_t value;
} hashItem;

typedef struct _hashTable
{
    hashItem* items;
    int len;
    int num;
} hashTable;

void insertHashItem(hashTable* table, intptr_t value, int update);
int findHashItem(hashTable* table, intptr_t lookup, int update);

#endif // HASHTABLE_H_INCLUDED
