#pragma once
#include <cstring>
#include <cstdint>

static const int PAGE_SIZE        = 4096;
static const int PAGE_HEADER_SIZE = 32;
static const int RECORD_SIZE      = 256;
static const int RECORDS_PER_PAGE = (PAGE_SIZE - PAGE_HEADER_SIZE) / RECORD_SIZE; // = 15

struct PageHeader {
    int  pageId;
    int  tableId;
    int  numRecords;
    bool isDirty;
    char padding[15];
};

class Page {
public:
    char       data[PAGE_SIZE];
    bool       inUse;
    int        frameId;

    // Accessors into header
    PageHeader* header() { return reinterpret_cast<PageHeader*>(data); }

    void init(int pageId, int tableId) {
        memset(data, 0, PAGE_SIZE);
        header()->pageId     = pageId;
        header()->tableId    = tableId;
        header()->numRecords = 0;
        header()->isDirty    = false;
        inUse = true;
    }

    char* recordSlot(int i) {
        return data + PAGE_HEADER_SIZE + i * RECORD_SIZE;
    }

    bool addRecord(const char* rec) {
        int n = header()->numRecords;
        if (n >= RECORDS_PER_PAGE) return false;
        memcpy(recordSlot(n), rec, RECORD_SIZE);
        header()->numRecords++;
        header()->isDirty = true;
        return true;
    }

    // Read record i into out
    bool readRecord(int i, char* out) {
        if (i >= header()->numRecords) return false;
        memcpy(out, recordSlot(i), RECORD_SIZE);
        return true;
    }

    int  numRecords() const { return reinterpret_cast<const PageHeader*>(data)->numRecords; }
    int  pageId()     const { return reinterpret_cast<const PageHeader*>(data)->pageId; }
    bool isDirty()    const { return reinterpret_cast<const PageHeader*>(data)->isDirty; }

    void markClean() { header()->isDirty = false; }

    Page() : inUse(false), frameId(-1) { memset(data, 0, PAGE_SIZE); }
};
