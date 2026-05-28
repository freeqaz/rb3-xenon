#pragma once

namespace Quazal {
    class RootObject {
    public:
        static void *operator new(size_t);
        static void *operator new(size_t, const char *, unsigned int);
        static void *operator new[](size_t);
        static void *operator new[](size_t, const char *, unsigned int);
        static void operator delete(void *);
        static void operator delete[](void *);
    };
}
