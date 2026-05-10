#pragma once
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

enum class ValueType : char { INT = 1, FLOAT = 2, STRING = 3 };

// abstract base so we can store int/float/string in the same Row array
class Value {
public:
    virtual ~Value() {}
    virtual ValueType getType()    const = 0;
    virtual double    toDouble()   const = 0;
    virtual char*     toString()   const = 0; // caller must delete[]
    virtual Value*    clone()      const = 0;

    // arithmetic ops - return new heap Value
    virtual Value* add(const Value& o) const = 0;
    virtual Value* sub(const Value& o) const = 0;
    virtual Value* mul(const Value& o) const = 0;
    virtual Value* div(const Value& o) const = 0;
    virtual Value* mod(const Value& o) const = 0;

    // comparison
    virtual bool eq(const Value& o)  const { return toDouble() == o.toDouble(); }
    virtual bool neq(const Value& o) const { return !eq(o); }
    virtual bool lt(const Value& o)  const { return toDouble() <  o.toDouble(); }
    virtual bool lte(const Value& o) const { return toDouble() <= o.toDouble(); }
    virtual bool gt(const Value& o)  const { return toDouble() >  o.toDouble(); }
    virtual bool gte(const Value& o) const { return toDouble() >= o.toDouble(); }

    // write to buffer, return bytes written
    virtual int serialize(char* buf) const = 0;

    // read from buffer, returns new heap Value
    static Value* deserialize(const char* buf);
};

class IntValue : public Value {
    int v;
public:
    explicit IntValue(int x) : v(x) {}
    ValueType getType()  const override { return ValueType::INT; }
    double    toDouble() const override { return (double)v; }
    char* toString()     const override {
        char* s = new char[32];
        snprintf(s, 32, "%d", v);
        return s;
    }
    Value* clone() const override { return new IntValue(v); }

    Value* add(const Value& o) const override { return new IntValue((int)(v + o.toDouble())); }
    Value* sub(const Value& o) const override { return new IntValue((int)(v - o.toDouble())); }
    Value* mul(const Value& o) const override { return new IntValue((int)(v * o.toDouble())); }
    Value* div(const Value& o) const override {
        double d = o.toDouble();
        if (d == 0) return new IntValue(0);
        return new IntValue((int)(v / d));
    }
    Value* mod(const Value& o) const override {
        int d = (int)o.toDouble();
        if (d == 0) return new IntValue(0);
        return new IntValue(v % d);
    }

    int serialize(char* buf) const override {
        buf[0] = (char)ValueType::INT;
        memcpy(buf+1, &v, 4);
        return 5;
    }
};

class FloatValue : public Value {
    float v;
public:
    explicit FloatValue(float x) : v(x) {}
    ValueType getType()  const override { return ValueType::FLOAT; }
    double    toDouble() const override { return (double)v; }
    char* toString()     const override {
        char* s = new char[32];
        snprintf(s, 32, "%.2f", v);
        return s;
    }
    Value* clone() const override { return new FloatValue(v); }

    Value* add(const Value& o) const override { return new FloatValue((float)(v + o.toDouble())); }
    Value* sub(const Value& o) const override { return new FloatValue((float)(v - o.toDouble())); }
    Value* mul(const Value& o) const override { return new FloatValue((float)(v * o.toDouble())); }
    Value* div(const Value& o) const override {
        double d = o.toDouble();
        if (d == 0.0) return new FloatValue(0.0f);
        return new FloatValue((float)(v / d));
    }
    Value* mod(const Value& o) const override {
        // float mod -> use fmod
        return new FloatValue((float)fmod(v, o.toDouble()));
    }

    int serialize(char* buf) const override {
        buf[0] = (char)ValueType::FLOAT;
        memcpy(buf+1, &v, 4);
        return 5;
    }
};

class StringValue : public Value {
    char v[128];
public:
    explicit StringValue(const char* s) { strncpy(v, s, 127); v[127] = '\0'; }
    ValueType getType()  const override { return ValueType::STRING; }
    double    toDouble() const override { return atof(v); }
    char* toString()     const override {
        int n = strlen(v);
        char* s = new char[n+1];
        memcpy(s, v, n+1);
        return s;
    }
    Value* clone() const override { return new StringValue(v); }
    const char* raw() const { return v; }

    // string doesn't do arithmetic, just return copy
    Value* add(const Value&) const override { return clone(); }
    Value* sub(const Value&) const override { return clone(); }
    Value* mul(const Value&) const override { return clone(); }
    Value* div(const Value&) const override { return clone(); }
    Value* mod(const Value&) const override { return clone(); }

    // string comparison is lexicographic
    bool eq(const Value& o)  const override { return strcmp(v, o.toString()) == 0; }
    bool neq(const Value& o) const override { return !eq(o); }
    bool lt(const Value& o)  const override { char* s = o.toString(); bool r = strcmp(v,s)<0; delete[] s; return r; }
    bool lte(const Value& o) const override { char* s = o.toString(); bool r = strcmp(v,s)<=0; delete[] s; return r; }
    bool gt(const Value& o)  const override { char* s = o.toString(); bool r = strcmp(v,s)>0; delete[] s; return r; }
    bool gte(const Value& o) const override { char* s = o.toString(); bool r = strcmp(v,s)>=0; delete[] s; return r; }

    int serialize(char* buf) const override {
        short len = (short)strlen(v);
        buf[0] = (char)ValueType::STRING;
        memcpy(buf+1, &len, 2);
        memcpy(buf+3, v, len);
        return 3 + len;
    }
};

// needs to be after all three subclasses
inline Value* Value::deserialize(const char* buf) {
    ValueType t = (ValueType)buf[0];
    if (t == ValueType::INT) {
        int x; memcpy(&x, buf+1, 4);
        return new IntValue(x);
    } else if (t == ValueType::FLOAT) {
        float x; memcpy(&x, buf+1, 4);
        return new FloatValue(x);
    } else {
        short len; memcpy(&len, buf+1, 2);
        char tmp[129] = {};
        int copy = len < 128 ? len : 127;
        memcpy(tmp, buf+3, copy);
        tmp[copy] = '\0';
        return new StringValue(tmp);
    }
}
