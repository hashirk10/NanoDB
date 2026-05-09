#pragma once
#include <cstdio>
#include <cstring>
#include "Page.h"
#include "../utils/Logger.h"

static const int MAX_TABLES = 32;

class DiskManager {
    struct FileEntry {
        char  name[64];   // table name
        FILE* fp;
        int   tableId;
        bool  open;
        FileEntry() : fp(nullptr), tableId(-1), open(false) { name[0] = '\0'; }
    };

    FileEntry files[MAX_TABLES];
    int       numFiles;

    char dataDir[256];

    int findFile(const char* tableName) const {
        for (int i = 0; i < numFiles; i++)
            if (files[i].open && strcmp(files[i].name, tableName) == 0) return i;
        return -1;
    }

public:
    DiskManager(const char* dir = "data") : numFiles(0) {
        strncpy(dataDir, dir, 255);
        dataDir[255] = '\0';
    }

    ~DiskManager() {
        for (int i = 0; i < numFiles; i++)
            if (files[i].open && files[i].fp) fclose(files[i].fp);
    }

    // Open or create a table file; returns tableId
    int openTable(const char* tableName) {
        int idx = findFile(tableName);
        if (idx != -1) return files[idx].tableId;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s.db", dataDir, tableName);

        FILE* fp = fopen(path, "r+b");
        if (!fp) fp = fopen(path, "w+b");
        if (!fp) { Logger::log("DiskManager: failed to open file %s", path); return -1; }

        idx = numFiles++;
        strncpy(files[idx].name, tableName, 63);
        files[idx].fp      = fp;
        files[idx].tableId = idx;
        files[idx].open    = true;
        Logger::log("DiskManager: opened table file %s (id=%d)", path, idx);
        return idx;
    }

    // Write a page to disk at its slot
    void writePage(int tableId, Page* page) {
        if (tableId < 0 || tableId >= numFiles || !files[tableId].open) return;
        long offset = (long)page->pageId() * PAGE_SIZE;
        fseek(files[tableId].fp, offset, SEEK_SET);
        fwrite(page->data, 1, PAGE_SIZE, files[tableId].fp);
        fflush(files[tableId].fp);
        page->markClean();
    }

    // Read a page from disk; returns false if page doesn't exist yet
    bool readPage(int tableId, int pageId, Page* page) {
        if (tableId < 0 || tableId >= numFiles || !files[tableId].open) return false;
        long offset = (long)pageId * PAGE_SIZE;
        fseek(files[tableId].fp, 0, SEEK_END);
        long fileSize = ftell(files[tableId].fp);
        if (offset >= fileSize) return false;
        fseek(files[tableId].fp, offset, SEEK_SET);
        size_t read = fread(page->data, 1, PAGE_SIZE, files[tableId].fp);
        if (read < PAGE_SIZE) {
            // Partial page (new page at end of file)
            memset(page->data + read, 0, PAGE_SIZE - read);
        }
        page->inUse = true;
        return true;
    }

    // Returns number of pages (pages) written for a table
    int getPageCount(int tableId) {
        if (tableId < 0 || tableId >= numFiles || !files[tableId].open) return 0;
        fseek(files[tableId].fp, 0, SEEK_END);
        long fileSize = ftell(files[tableId].fp);
        return (int)(fileSize / PAGE_SIZE);
    }
};
