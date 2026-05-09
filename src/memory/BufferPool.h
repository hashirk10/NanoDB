#pragma once
#include "Page.h"
#include "DiskManager.h"
#include "../utils/DoublyLinkedList.h"
#include "../utils/HashMap.h"
#include "../utils/Logger.h"
#include <cstdio>
#include <cstring>

static const int DEFAULT_POOL_SIZE = 1000;

class BufferPool {
    Page*             frames;
    int               poolSize;
    int               evictions;
    int               hits;
    int               misses;
    int               pageFaults;

    DoublyLinkedList<int> lruList;   // front = MRU, back = LRU
    DLLNode<int>**    frameNode;     // maps frame index -> DLL node
    bool*             frameInUse;
    HashMap<int>      pageMap;       // "tid:pid" -> frame index

    DiskManager* disk;

    // evict the LRU frame and return its index
    int evictLRU() {
        if (!lruList.tail) return -1;
        int victim = lruList.tail->val;
        Page* vp = &frames[victim];

        char evictKey[64];
        snprintf(evictKey, sizeof(evictKey), "%d:%d", vp->header()->tableId, vp->header()->pageId);

        if (vp->isDirty()) {
            disk->writePage(vp->header()->tableId, vp);
            Logger::log("Page %d evicted via LRU, written to disk (table=%d)", vp->pageId(), vp->header()->tableId);
        } else {
            Logger::log("Page %d evicted via LRU (clean, table=%d)", vp->pageId(), vp->header()->tableId);
        }
        evictions++;

        pageMap.remove(evictKey);
        // remove from lruList by moving to front then popping — actually just remove the node
        if (frameNode[victim]) {
            // manually unlink
            DLLNode<int>* nd = frameNode[victim];
            if (nd->prev) nd->prev->next = nd->next;
            else          lruList.head = nd->next;
            if (nd->next) nd->next->prev = nd->prev;
            else          lruList.tail = nd->prev;
            nd->prev = nd->next = nullptr;
            lruList.sz--;
            delete nd;
            frameNode[victim] = nullptr;
        }
        frameInUse[victim] = false;
        return victim;
    }

    int getFreeFrame() {
        for (int i = 0; i < poolSize; i++)
            if (!frameInUse[i]) return i;
        return evictLRU();
    }

public:
    BufferPool(DiskManager* dm, int size = DEFAULT_POOL_SIZE)
        : poolSize(size), evictions(0), hits(0), misses(0), pageFaults(0), disk(dm)
    {
        frames     = new Page[poolSize];
        frameNode  = new DLLNode<int>*[poolSize];
        frameInUse = new bool[poolSize];
        for (int i = 0; i < poolSize; i++) {
            frames[i].frameId = i;
            frameNode[i]  = nullptr;
            frameInUse[i] = false;
        }
        Logger::log("BufferPool initialized with %d page frames", poolSize);
    }

    ~BufferPool() {
        flushAll();
        delete[] frames;
        delete[] frameNode;
        delete[] frameInUse;
    }

    Page* fetchPage(int tableId, int pageId) {
        char key[64];
        snprintf(key, sizeof(key), "%d:%d", tableId, pageId);

        int* fp = pageMap.get(key);
        if (fp) {
            hits++;
            lruList.moveToFront(frameNode[*fp]);
            return &frames[*fp];
        }

        misses++;
        pageFaults++;
        int fi = getFreeFrame();
        if (fi < 0) { printf("[ERROR] BufferPool: no frame available\n"); return &frames[0]; }

        Page* p = &frames[fi];
        p->init(pageId, tableId);

        if (!disk->readPage(tableId, pageId, p))
            p->init(pageId, tableId);

        frameInUse[fi] = true;
        pageMap.set(key, fi);
        frameNode[fi] = lruList.pushFront(fi);
        return p;
    }

    void markDirty(Page* p) {
        p->header()->isDirty = true;
    }

    void flushAll() {
        for (int i = 0; i < poolSize; i++) {
            if (frameInUse[i] && frames[i].isDirty())
                disk->writePage(frames[i].header()->tableId, &frames[i]);
        }
    }

    int getEvictions()  const { return evictions; }
    int getPoolSize()   const { return poolSize; }

    void printStats() {
        Logger::log("BufferPool stats: hits=%d  misses=%d  evictions=%d  pool_size=%d",
                    hits, misses, evictions, poolSize);
    }
};
