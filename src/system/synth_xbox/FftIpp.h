#pragma once

#include "utl/MemMgr.h"
#include <vector>

template <class T>
class XboxAllocator {
public:
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef T value_type;
    typedef T *pointer;
    typedef T &reference;
    typedef const T *const_pointer;
    typedef const T &const_reference;

    template <class T2>
    struct rebind {
        typedef XboxAllocator<T2> other;
    };

    XboxAllocator() {}
    XboxAllocator(const XboxAllocator &) {}
    template <class T2>
    XboxAllocator(const XboxAllocator<T2> &) {}
    ~XboxAllocator() {}

    template <class T2>
    XboxAllocator &operator=(const XboxAllocator<T2> &) { return *this; }

    template <class T2>
    bool operator==(const XboxAllocator<T2> &) const { return true; }
    template <class T2>
    bool operator!=(const XboxAllocator<T2> &) const { return false; }

    pointer address(reference value) const { return &value; }
    const_pointer address(const_reference value) const { return &value; }
    size_type max_size() const { return size_type(-1) / sizeof(T); }

    pointer allocate(size_type count, const void *hint = 0) {
        return (pointer)MemAlloc(count * sizeof(T), __FILE__, __LINE__, "unknown", 0);
    }

    void deallocate(pointer ptr, size_type) {
        MemFree(ptr);
    }

    void construct(pointer ptr, const_reference value) { new (ptr) T(value); }
    void destroy(pointer ptr) { ptr->~T(); }
};

class FftIpp {
public:
    void FftRealCcs(unsigned int *, volatile float &, unsigned int *, float &);
    void
    FftReal(unsigned int *, volatile float &, unsigned int *, float &, volatile float &);
    ~FftIpp();
    FftIpp();
    void SetMode(int);

    int mSize;
    int mOrder;
    std::vector<float, XboxAllocator<float> > mBuf1;   // 0x08
    std::vector<float, XboxAllocator<float> > mBuf2;   // 0x14
    std::vector<float, XboxAllocator<float> > mBuf3;   // 0x20
    std::vector<float, XboxAllocator<float> > mBuf4;   // 0x2C
    std::vector<float, XboxAllocator<float> > mSinCos; // 0x38
};
