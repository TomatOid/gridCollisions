#include <stdlib.h>
#include "HashTable.h"

void insertHashItem(hashTable* table, intptr_t value, int update)
{
    int mod = table->len;
    int i = value % mod;
    while (table->items[i % mod].value && (table->items[i % mod].updateCount == update) && i < (2 * mod)) { i++; } // increment untill there is a free space
    table->items[i % mod].value = value;
    table->items[i % mod].updateCount = update;
    table->num++;
}

int findHashItem(hashTable* table, intptr_t lookup, int update)
{
    int mod = table->len;
    int i = lookup % mod;
    // increment untill there is a free space or the item is found
    while (table->items[i % mod].value && (table->items[i % mod].updateCount == update) && i < (2 * mod))
    {
        if ((table->items[i % mod].value == lookup) && (table->items[i % mod].updateCount == update)) { return 1; }
        i++;
    }
    return 0;
}
