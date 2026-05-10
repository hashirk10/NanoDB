#pragma once
#include "Table.h"
#include "../utils/HashMap.h"
#include <cstdio>

// SystemCatalog keeps track of all tables by name
// using our HashMap for O(1) lookup
class SystemCatalog {
    HashMap<Table*> tables;
    int nextId;

public:
    SystemCatalog() : nextId(0) {
        Logger::log("SystemCatalog initialized (O(1) hash map)");
    }

    ~SystemCatalog() {
        // tables themselves are owned by NanoDB, not here
    }

    int nextTableId() { return nextId++; }

    void registerTable(Table* t) {
        tables.set(t->getName(), t);
        Logger::log("SystemCatalog: registered table '%s' (id=%d)", t->getName(), t->getId());
    }

    Table* getTable(const char* name) {
        Table** tp = tables.get(name);
        return tp ? *tp : nullptr;
    }

    void listTables() {
        printf("\n=== System Catalog ===\n");
        tables.forEach([](const char* key, Table* const& t) {
            printf("  Table: %-20s Rows: %d\n", key, t->numRows());
        });
        printf("======================\n\n");
    }
};
