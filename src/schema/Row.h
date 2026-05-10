#pragma once
#include "Value.h"
#include "../utils/CustomArray.h"
#include "../memory/Page.h"
#include <cstring>
#include <cstdio>

// column metadata struct
struct Column {
    char      name[64];
    ValueType type;
    bool      isPrimaryKey;

    Column() : type(ValueType::INT), isPrimaryKey(false) { name[0] = '\0'; }
    Column(const char* n, ValueType t, bool pk = false) : type(t), isPrimaryKey(pk) {
        strncpy(name, n, 63); name[63] = '\0';
    }
};

// Row stores an array of Value* (heterogeneous via polymorphism)
class Row {
    Value** values;
    int     numCols;

public:
    Row() : values(nullptr), numCols(0) {}

    Row(int cols) : numCols(cols) {
        values = new Value*[numCols];
        for (int i = 0; i < numCols; i++) values[i] = nullptr;
    }

    ~Row() { clear(); }

    Row(const Row& o) : numCols(o.numCols) {
        values = new Value*[numCols];
        for (int i = 0; i < numCols; i++)
            values[i] = o.values[i] ? o.values[i]->clone() : nullptr;
    }

    Row& operator=(const Row& o) {
        if (this == &o) return *this;
        clear();
        numCols = o.numCols;
        values  = new Value*[numCols];
        for (int i = 0; i < numCols; i++)
            values[i] = o.values[i] ? o.values[i]->clone() : nullptr;
        return *this;
    }

    void clear() {
        if (values) {
            for (int i = 0; i < numCols; i++) delete values[i];
            delete[] values;
            values = nullptr;
        }
        numCols = 0;
    }

    void set(int i, Value* v) {
        if (i < numCols) { delete values[i]; values[i] = v; }
    }

    Value* get(int i) const {
        if (i < 0 || i >= numCols) return nullptr;
        return values[i];
    }

    int cols() const { return numCols; }

    // pack this row into a fixed RECORD_SIZE buffer
    void serialize(char* buf) const {
        memset(buf, 0, RECORD_SIZE);
        int off = 0;
        memcpy(buf + off, &numCols, 4); off += 4;
        for (int i = 0; i < numCols && off < RECORD_SIZE - 16; i++) {
            if (values[i]) {
                off += values[i]->serialize(buf + off);
            } else {
                buf[off++] = (char)ValueType::INT;
                int zero = 0; memcpy(buf+off, &zero, 4); off += 4;
            }
        }
    }

    void deserialize(const char* buf, const CustomArray<Column>& cols) {
        clear();
        numCols = cols.size();
        values  = new Value*[numCols];
        int off = 4; // skip numCols int
        for (int i = 0; i < numCols && off < RECORD_SIZE; i++) {
            values[i] = Value::deserialize(buf + off);
            ValueType t = (ValueType)buf[off];
            if (t == ValueType::INT || t == ValueType::FLOAT) off += 5;
            else {
                short len; memcpy(&len, buf+off+1, 2);
                off += 3 + len;
            }
        }
    }

    void print(const CustomArray<Column>& cols) const {
        (void)cols;
        for (int i = 0; i < numCols; i++) {
            if (values[i]) {
                char* s = values[i]->toString();
                printf("%-20s", s);
                delete[] s;
            } else {
                printf("%-20s", "NULL");
            }
            if (i < numCols - 1) printf(" | ");
        }
        printf("\n");
    }

    Value* getByName(const char* colName, const CustomArray<Column>& cols) const {
        for (int i = 0; i < cols.size(); i++)
            if (strcmp(cols[i].name, colName) == 0) return values[i];
        return nullptr;
    }
};
