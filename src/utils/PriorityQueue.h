#pragma once
#include "CustomArray.h"
#include <cstring>

// min-heap based priority queue
// lower priority number = higher urgency (0 = admin, 1 = normal user)
struct PQEntry {
    int  priority;
    char query[512];
    int  id;
};

class PriorityQueue {
    CustomArray<PQEntry> heap;

    int parent(int i) { return (i - 1) / 2; }
    int left(int i)   { return 2*i + 1; }
    int right(int i)  { return 2*i + 2; }

    void swap(int a, int b) {
        PQEntry tmp = heap[a];
        heap[a] = heap[b];
        heap[b] = tmp;
    }

    void bubbleUp(int i) {
        while (i > 0 && heap[parent(i)].priority > heap[i].priority) {
            swap(i, parent(i));
            i = parent(i);
        }
    }

    void bubbleDown(int i) {
        int n = heap.size();
        while (true) {
            int smallest = i;
            int l = left(i), r = right(i);
            if (l < n && heap[l].priority < heap[smallest].priority) smallest = l;
            if (r < n && heap[r].priority < heap[smallest].priority) smallest = r;
            if (smallest == i) break;
            swap(i, smallest);
            i = smallest;
        }
    }

public:
    void enqueue(const char* query, int priority, int id) {
        PQEntry e;
        e.priority = priority;
        e.id       = id;
        strncpy(e.query, query, 511);
        e.query[511] = '\0';
        heap.push_back(e);
        bubbleUp(heap.size() - 1);
    }

    PQEntry dequeue() {
        PQEntry top = heap[0];
        int last = heap.size() - 1;
        heap[0] = heap[last];
        heap.removeAt(last);
        if (!heap.empty()) bubbleDown(0);
        return top;
    }

    bool empty() const { return heap.empty(); }
    int  size()  const { return heap.size(); }
};
