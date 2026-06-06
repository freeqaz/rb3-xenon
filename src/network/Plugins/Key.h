#pragma once
#include "Platform/RefCountedObject.h"
#include "Platform/qStd.h"

namespace Quazal {
    class Key : public RefCountedObject {
    public:
        Key();
        Key(unsigned char *, unsigned int);
        Key(const Key &);
        virtual ~Key();

        Key &operator=(const Key &);
        int GetLength() const;
        const unsigned char *GetContentPtr() const;
        unsigned char *PrepareContentPtr(unsigned int);

        qVector<unsigned char> mData; // 0x8, size 0xc (STLport vector with EBO: 3 ptrs)
        int _unusedPad; // 0x14 - binary has Key=0x18; unknown field (MSVC secure-scl or alloc EBO diff)
    };
}