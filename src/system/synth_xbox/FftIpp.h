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
        // Retail/match: this allocator passes align 0x10 and guards count==0.
        // Both are read off retail bytes at the only two call sites that survive
        // out-of-line, 0x82B7599C (_Vector_base ctor) and 0x82B75C24
        // (_M_insert_overflow): `cmplwi cr6,r4,0; beq; li r4,0x10; bl MemAlloc`.
        // ⚠ The parenthesized `(MemAlloc)` is LOAD-BEARING. MemMgr.h defines
        // `#define MemAlloc(size, file, line, name, ...) (MemAlloc)((size), 0)`,
        // and MSVC's traditional preprocessor expands that macro even when it is
        // invoked with FEWER arguments than it declares -- so the previous
        // 5-arg debug spelling here had its trailing align swallowed and emitted
        // `li r4,0` where retail has `li r4,0x10`. It compiled, it looked right,
        // and only the immediate in the callee's r4 gave it away. See the W16-BT
        // census doc for the whole-binary sweep of this hazard.
        if (count == 0)
            return 0;
#ifdef HX_NATIVE
        return (pointer)MemAlloc(count * sizeof(T), __FILE__, __LINE__, "unknown", 0x10);
#else
        return (pointer)(MemAlloc)(count * sizeof(T), 0x10);
#endif
    }

    void deallocate(pointer ptr, size_type) {
        MemFree(ptr);
    }

    void construct(pointer ptr, const_reference value) { new (ptr) T(value); }
    void destroy(pointer ptr) { ptr->~T(); }
};

class FftIpp {
public:
    void FftRealCcs(const float *__restrict, float *__restrict);
    void FftReal(const float *__restrict, float *__restrict, float *__restrict);
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
