#pragma once
#include <cstring>
#include <cstdlib>

// simple open-addressing hash map with linear probing
// key = c-string, value = template type
// resize at 70% load factor
template<typename V>
class HashMap {
    struct Entry {
        char key[128];
        V    val;
        bool used;
        bool deleted;
        Entry() : used(false), deleted(false) { key[0] = '\0'; }
    };

    Entry* table;
    int    cap;
    int    cnt;

    int hash(const char* k) const {
        // djb2
        unsigned long h = 5381;
        int c;
        while ((c = *k++)) h = ((h << 5) + h) + c;
        return (int)(h % cap);
    }

    void rehash() {
        int oldCap = cap;
        Entry* oldTable = table;
        cap = cap * 2;
        table = new Entry[cap];
        cnt = 0;
        for (int i = 0; i < oldCap; i++) {
            if (oldTable[i].used && !oldTable[i].deleted) {
                set(oldTable[i].key, oldTable[i].val);
            }
        }
        delete[] oldTable;
    }

public:
    HashMap(int initCap = 64) : cap(initCap), cnt(0) {
        table = new Entry[cap];
    }

    ~HashMap() { delete[] table; }

    void set(const char* key, const V& val) {
        if (cnt * 10 >= cap * 7) rehash(); // resize at ~70%
        int idx = hash(key);
        while (table[idx].used && !table[idx].deleted && strcmp(table[idx].key, key) != 0) {
            idx = (idx + 1) % cap;
        }
        if (!table[idx].used || table[idx].deleted) cnt++;
        strncpy(table[idx].key, key, 127);
        table[idx].key[127] = '\0';
        table[idx].val     = val;
        table[idx].used    = true;
        table[idx].deleted = false;
    }

    // returns nullptr if not found
    V* get(const char* key) {
        int idx = hash(key);
        int probes = 0;
        while (table[idx].used && probes < cap) {
            if (!table[idx].deleted && strcmp(table[idx].key, key) == 0)
                return &table[idx].val;
            idx = (idx + 1) % cap;
            probes++;
        }
        return nullptr;
    }

    bool contains(const char* key) {
        return get(key) != nullptr;
    }

    void remove(const char* key) {
        int idx = hash(key);
        int probes = 0;
        while (table[idx].used && probes < cap) {
            if (!table[idx].deleted && strcmp(table[idx].key, key) == 0) {
                table[idx].deleted = true;
                cnt--;
                return;
            }
            idx = (idx + 1) % cap;
            probes++;
        }
    }

    int size() const { return cnt; }

    // iterate: call fn(key, val) for each live entry
    template<typename Fn>
    void forEach(Fn fn) const {
        for (int i = 0; i < cap; i++) {
            if (table[i].used && !table[i].deleted)
                fn(table[i].key, table[i].val);
        }
    }
};
