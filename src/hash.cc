#include "hash.h"
#include <cstring>

HashSet::HashSet(int depth)
{
    table = new Entry[1 << depth];
    memset(table, 0, sizeof(Entry) * (1 << depth));
    mask = (1 << depth) - 1;
}

HashSet::~HashSet()
{
    delete[] table;
}

int HashSet::hash(uint64_t key)
{
    return (int) key & mask;
}

void HashSet::clear()
{
    memset(table, 0, sizeof(Entry) * (mask + 1));
}

int HashSet::increment(uint64_t key)
{
    int index = hash(key), i = index;
    do
    {
        if (table[i].value == 0)
        {
            table[i].key = key;
            table[i].value = 1;
            return 1;
        }
        else if (table[i].key == key)
        {
            ++table[i].value;
            return (int) table[i].value;
        }
        else
            i = (i + 1) & mask;
    } while (i != index);

    int old_size = mask + 1;
    int new_size = (mask + 1) * 2;
    mask = new_size - 1;
    Entry *tmp = table;
    table = new Entry[new_size];
    memset(table, 0, sizeof(Entry) * new_size);
    for (int i = 0; i < old_size; ++i)
        for (int j = 0; j < (int) tmp[i].value; ++j)
            increment(tmp[i].key);
    delete []tmp;

    return increment(key);
}

int HashSet::count(uint64_t key)
{
    int index = hash(key), i = index;
    do
    {
        if (table[i].value == 0)
            return 0;
        else if (table[i].key == key)
            return (int) table[i].value;
        else
            i = (i + 1) & mask;
    } while (i != index);

    return 0;
}

int HashSet::decrement(uint64_t key)
{
    int index = hash(key), i = index;
    int pos = -1;
    do
    {
        if (table[i].value == 0)
            break;
        else if (table[i].key == key)
        {
            --table[i].value;
            if (table[i].value)
                return table[i].value;
            else
            {
                pos = i;
                break;
            }
        }
        else
            i = (i + 1) & mask;
    } while (i != index);

    if (pos == -1)
        return 0;

    i = (pos + 1) & mask;
    int hole = pos;
    while (i != pos)
    {
        if (table[i].value == 0)
            break;

        bool b1 = hole < i;
        int h = hash(table[i].key);
        bool b2 = (h > hole && h <= i);
        if ((b1 || b2) && (!b1 || !b2))
        {
            table[hole] = table[i];
            table[i].value = 0;
            hole = i;
        }

        i = (i + 1) & mask;
    }
    return 0;
}
