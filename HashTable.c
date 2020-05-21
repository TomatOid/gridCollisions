#include <stdlib.h>
#include "HashTable.h"

// Returns 1 if the item already exists in the hash table, and returns 0 if a new item was added
int insertHashItem(hashTable* table, intptr_t value, int update)
{
    int mod = table->len;
    int i = 80933 * (unsigned int)(value ^ (value >> 5)) % mod;
    while (table->items[i % mod].value && (table->items[i % mod].updateCount == update) && i < (2 * mod))
    { 
        if (table->items[i % mod].value == value) { return 1; }
        i++; 
    } // increment untill there is a free space
    table->items[i % mod].value = value;
    table->items[i % mod].updateCount = update;
    table->num++;
    return 0;
}

int findHashItem(hashTable* table, intptr_t lookup, int update)
{
    int mod = table->len;
    int i = 80933 * (unsigned int)lookup % mod;
    // increment untill there is a free space or the item is found
    while (table->items[i % mod].value && (table->items[i % mod].updateCount == update) && i < (2 * mod))
    {
        if ((table->items[i % mod].value == lookup) && (table->items[i % mod].updateCount == update)) { return 1; }
        i++;
    }
    return 0;
}
