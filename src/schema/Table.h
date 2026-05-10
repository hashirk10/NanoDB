#pragma once
#include "Row.h"
#include "Value.h"
#include "../utils/CustomArray.h"
#include "../utils/Logger.h"
#include "../index/AVLTree.h"
#include "../memory/BufferPool.h"
#include <cstring>
#include <cstdio>
#include <ctime>

static const int MAX_COL_NAME = 64;

class Table {
    char               name[64];
    int                tableId;
    CustomArray<Column> columns;
    CustomArray<Row*>   rows;
    AVLTree             index;      // primary key index (int col 0)
    BufferPool*         pool;
    int                 nextPageId;
    bool                hasPKIndex;

    // write row to next available page slot - O(1) amortized
    void persistRow(const Row& row) {
        char buf[RECORD_SIZE] = {};
        row.serialize(buf);

        Page* pg = pool->fetchPage(tableId, nextPageId);
        if (pg->numRecords() < RECORDS_PER_PAGE) {
            pg->addRecord(buf);
            pool->markDirty(pg);
            return;
        }
        // current page full, move to next
        nextPageId++;
        Page* newPg = pool->fetchPage(tableId, nextPageId);
        newPg->init(nextPageId, tableId);
        newPg->addRecord(buf);
        pool->markDirty(newPg);
    }

public:
    Table() : tableId(-1), pool(nullptr), nextPageId(0), hasPKIndex(false) {
        name[0] = '\0';
    }

    Table(const char* n, int tid, BufferPool* bp) : tableId(tid), pool(bp), nextPageId(0), hasPKIndex(false) {
        strncpy(name, n, 63); name[63] = '\0';
    }

    ~Table() {
        for (int i = 0; i < rows.size(); i++) delete rows[i];
    }

    void addColumn(const Column& c) {
        columns.push_back(c);
        if (c.isPrimaryKey && c.type == ValueType::INT) hasPKIndex = true;
    }

    void insertRow(Row* row) {
        int idx = rows.size();
        rows.push_back(row);

        if (hasPKIndex && columns.size() > 0) {
            Value* pk = row->get(0);
            if (pk && pk->getType() == ValueType::INT)
                index.insert((int)pk->toDouble(), idx);
        }

        if (pool) persistRow(*row);
    }

    // full scan, returns array of indices
    int* seqScan(int& count) const {
        count = rows.size();
        int* out = new int[count];
        for (int i = 0; i < count; i++) out[i] = i;
        return out;
    }

    int indexScan(int key) const { return index.search(key); }

    Row*  getRow(int i) const { return (i >= 0 && i < rows.size()) ? rows[i] : nullptr; }
    int   numRows()     const { return rows.size(); }
    int   numCols()     const { return columns.size(); }
    const char* getName() const { return name; }
    int   getId()        const { return tableId; }
    const CustomArray<Column>& getCols() const { return columns; }
    AVLTree& getIndex() { return index; }

    void printHeader() const {
        for (int i = 0; i < columns.size(); i++) {
            printf("%-20s", columns[i].name);
            if (i < columns.size() - 1) printf(" | ");
        }
        printf("\n");
        for (int i = 0; i < columns.size() * 23; i++) printf("-");
        printf("\n");
    }

    void deleteRow(int idx) {
        if (idx < 0 || idx >= rows.size()) return;
        Row* r = rows[idx];
        if (hasPKIndex && r->get(0))
            index.remove((int)r->get(0)->toDouble());
        delete r;
        rows[idx] = nullptr;
    }

    void updateRow(int idx, const char* colName, Value* newVal) {
        Row* r = rows[idx];
        if (!r) return;
        for (int i = 0; i < columns.size(); i++) {
            if (strcmp(columns[i].name, colName) == 0) {
                r->set(i, newVal);
                if (pool) persistRow(*r);
                return;
            }
        }
        delete newVal;
    }

    int colIndex(const char* colName) const {
        for (int i = 0; i < columns.size(); i++)
            if (strcmp(columns[i].name, colName) == 0) return i;
        return -1;
    }

    void flush() {
        if (pool) pool->flushAll();
    }
};
