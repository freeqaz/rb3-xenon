#pragma once

namespace Quazal {
    class VirtualRootObject {
    public:
        virtual ~VirtualRootObject() {}
        static void *operator new(size_t, char *, unsigned int); // size_t not unsigned long (MSVC requires)
        static void operator delete(void *);
    };
}
