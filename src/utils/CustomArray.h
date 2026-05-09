#pragma once
#include <cstdlib>
#include <cstring>

// dynamic array, doubles capacity when full
// using raw heap instead of std::vector since STL is not allowed
template<typename T>
class CustomArray {
    T*  data;
    int cap;
    int len;

    void grow() {
        int newCap = cap * 2;
        T* newData = new T[newCap];
        for (int i = 0; i < len; i++) newData[i] = data[i];
        delete[] data;
        data = newData;
        cap  = newCap;
    }

public:
    CustomArray() : cap(8), len(0) {
        data = new T[cap];
    }

    ~CustomArray() { delete[] data; }

    // copy ctor
    CustomArray(const CustomArray& o) : cap(o.cap), len(o.len) {
        data = new T[cap];
        for (int i = 0; i < len; i++) data[i] = o.data[i];
    }

    CustomArray& operator=(const CustomArray& o) {
        if (this == &o) return *this;
        delete[] data;
        cap  = o.cap;
        len  = o.len;
        data = new T[cap];
        for (int i = 0; i < len; i++) data[i] = o.data[i];
        return *this;
    }

    void push_back(const T& val) {
        if (len == cap) grow();
        data[len++] = val;
    }

    T& operator[](int i)       { return data[i]; }
    const T& operator[](int i) const { return data[i]; }

    int  size()  const { return len; }
    bool empty() const { return len == 0; }

    void clear() { len = 0; }

    // remove element at index i, shifts everything left
    void removeAt(int i) {
        if (i < 0 || i >= len) return;
        for (int j = i; j < len-1; j++) data[j] = data[j+1];
        len--;
    }
};
