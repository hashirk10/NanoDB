#pragma once

// doubly linked list node
// used by the buffer pool for LRU tracking
template<typename T>
struct DLLNode {
    T         val;
    DLLNode*  prev;
    DLLNode*  next;

    DLLNode(const T& v) : val(v), prev(nullptr), next(nullptr) {}
};

template<typename T>
class DoublyLinkedList {
public:
    DLLNode<T>* head;
    DLLNode<T>* tail;
    int         sz;

    DoublyLinkedList() : head(nullptr), tail(nullptr), sz(0) {}

    ~DoublyLinkedList() {
        DLLNode<T>* cur = head;
        while (cur) {
            DLLNode<T>* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
    }

    // insert new node at front, return the node
    DLLNode<T>* pushFront(const T& val) {
        DLLNode<T>* node = new DLLNode<T>(val);
        node->next = head;
        if (head) head->prev = node;
        head = node;
        if (!tail) tail = node;
        sz++;
        return node;
    }

    // remove a node (caller has the pointer already)
    void remove(DLLNode<T>* node) {
        if (!node) return;
        if (node->prev) node->prev->next = node->next;
        else            head = node->next;
        if (node->next) node->next->prev = node->prev;
        else            tail = node->prev;
        node->prev = node->next = nullptr;
        sz--;
        delete node;
    }

    // move existing node to front (used on cache hit)
    void moveToFront(DLLNode<T>* node) {
        if (node == head) return;
        // detach
        if (node->prev) node->prev->next = node->next;
        if (node->next) node->next->prev = node->prev;
        else            tail = node->prev;
        // reattach at front
        node->prev = nullptr;
        node->next = head;
        if (head) head->prev = node;
        head = node;
    }

    // remove tail and return its value
    T popBack() {
        T val = tail->val;
        DLLNode<T>* old = tail;
        tail = tail->prev;
        if (tail) tail->next = nullptr;
        else      head = nullptr;
        delete old;
        sz--;
        return val;
    }

    int  size()  const { return sz; }
    bool empty() const { return sz == 0; }
};
