#pragma once

#include <stdint.h>

class HashSet
{
    private:
        typedef struct sEntry
        {
            uint64_t key;
            uint64_t value;
        } Entry;

        HashSet &operator=(const HashSet &);
        HashSet(const HashSet &);

        Entry *table;
        int mask;

        int hash(uint64_t key);

    public:
        HashSet(int depth);
        ~HashSet();

        int increment(uint64_t key);
        int decrement(uint64_t key);
        int count(uint64_t key);

        void clear();
};
