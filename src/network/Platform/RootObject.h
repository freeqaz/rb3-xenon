#pragma once

namespace Quazal {
    class RootObject {
    public:
        ~RootObject() {}
        static void *operator new(size_t);
        static void *operator new(size_t, const char *, unsigned int);
        static void *operator new(size_t, void *p) { return p; }
        static void *operator new[](size_t);
        static void *operator new[](size_t, const char *, unsigned int);
        static void operator delete(void *);
        static void operator delete(void *, const char *, unsigned int);
        static void operator delete[](void *);
    };
}
