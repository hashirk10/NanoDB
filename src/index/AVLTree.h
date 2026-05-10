#pragma once

struct AVLNode {
    int key;
    int val; // row index in Table::rows
    int height;
    AVLNode* left;
    AVLNode* right;

    AVLNode(int k, int v) : key(k), val(v), height(1), left(nullptr), right(nullptr) {}
};

static int avlH(AVLNode* n) { return n ? n->height : 0; }
static int avlBF(AVLNode* n) { return n ? avlH(n->left) - avlH(n->right) : 0; }
static void avlFix(AVLNode* n) {
    if (!n) return;
    int lh = avlH(n->left), rh = avlH(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

static AVLNode* rotateRight(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* t = x->right;
    x->right = y;
    y->left  = t;
    avlFix(y); avlFix(x);
    return x;
}

static AVLNode* rotateLeft(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* t = y->left;
    y->left  = x;
    x->right = t;
    avlFix(x); avlFix(y);
    return y;
}

static AVLNode* avlBalance(AVLNode* n) {
    avlFix(n);
    int bf = avlBF(n);
    if (bf > 1) {
        if (avlBF(n->left) < 0) n->left = rotateLeft(n->left);
        return rotateRight(n);
    }
    if (bf < -1) {
        if (avlBF(n->right) > 0) n->right = rotateRight(n->right);
        return rotateLeft(n);
    }
    return n;
}

static AVLNode* avlInsert(AVLNode* n, int key, int val) {
    if (!n) return new AVLNode(key, val);
    if      (key < n->key) n->left  = avlInsert(n->left,  key, val);
    else if (key > n->key) n->right = avlInsert(n->right, key, val);
    else                   n->val   = val; // update
    return avlBalance(n);
}

static AVLNode* avlMin(AVLNode* n) {
    while (n->left) n = n->left;
    return n;
}

static AVLNode* avlRemove(AVLNode* n, int key) {
    if (!n) return nullptr;
    if (key < n->key)      n->left  = avlRemove(n->left, key);
    else if (key > n->key) n->right = avlRemove(n->right, key);
    else {
        if (!n->left || !n->right) {
            AVLNode* tmp = n->left ? n->left : n->right;
            delete n;
            return tmp;
        }
        AVLNode* succ = avlMin(n->right);
        n->key = succ->key;
        n->val = succ->val;
        n->right = avlRemove(n->right, succ->key);
    }
    return avlBalance(n);
}

static int avlSearch(AVLNode* n, int key) {
    while (n) {
        if      (key == n->key) return n->val;
        else if (key <  n->key) n = n->left;
        else                    n = n->right;
    }
    return -1;
}

static void avlFree(AVLNode* n) {
    if (!n) return;
    avlFree(n->left);
    avlFree(n->right);
    delete n;
}

static int avlHeight(AVLNode* n) { return avlH(n); }

class AVLTree {
    AVLNode* root;
public:
    AVLTree() : root(nullptr) {}
    ~AVLTree() { avlFree(root); }

    void insert(int key, int val) { root = avlInsert(root, key, val); }
    void remove(int key)          { root = avlRemove(root, key); }
    int  search(int key) const    { return avlSearch(root, key); }
    int  height()        const    { return avlHeight(root); }
};
