#pragma once
#include "CustomArray.h"

// just a stack built on top of CustomArray
template<typename T>
class CustomStack {
    CustomArray<T> arr;
public:
    void   push(const T& v) { arr.push_back(v); }
    T      pop()            { T v = arr[arr.size()-1]; arr.removeAt(arr.size()-1); return v; }
    T&     top()            { return arr[arr.size()-1]; }
    bool   empty()   const  { return arr.empty(); }
    int    size()    const  { return arr.size(); }
    void   clear()          { arr.clear(); }
};
