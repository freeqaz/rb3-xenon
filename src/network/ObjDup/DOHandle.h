#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOID.h"

namespace Quazal {
    class DOHandle : public RootObject {
    public:
        DOHandle(unsigned int val = 0) : mValue(val) {}
        DOHandle(const DOHandle &h) : mValue(h.mValue) {}
        ~DOHandle() {}

        unsigned int GetDOClassID() const { return (mValue & 0xFFC00000) >> 22; }
        unsigned int GetID() const { return mValue & 0x3FFFFF; }
        DOID GetDOID() const { return mValue & 0x3FFFFF; }
        bool IsA(unsigned int id) const { return (mValue & 0xFFC00000) >> 22 == id; }

        bool operator<(const DOHandle &h) const { return mValue < h.mValue; }
        bool operator==(const DOHandle &h) const { return mValue == h.mValue; }
        bool operator!=(const DOHandle &h) const { return mValue != h.mValue; }

        void SetDOID(DOID);

        void SetDOClassID(unsigned int);
        const char *GetClassNameString() const;

        unsigned int mValue; // 0x0
    };
}