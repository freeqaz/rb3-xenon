#pragma once

template <class T>
class scoped_ptr {
    T* ptr;
public:
    scoped_ptr() : ptr(0) {}
    ~scoped_ptr() { delete ptr; }
    T* get() const { return ptr; }
    void reset(T* p) { if (ptr != p) { delete ptr; ptr = p; } }
    T& operator*() const { return *ptr; }
    T* operator->() const { return ptr; }
};
