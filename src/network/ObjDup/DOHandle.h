#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOID.h"

namespace Quazal {
    class ByteStream;
    class DOHandle : public RootObject {
    public:
        DOHandle(unsigned int val = 0) : mValue(val) {}
        DOHandle(const DOHandle &h) : mValue(h.mValue) {}
        ~DOHandle() {}
        // User-declared and inline: retail's assignments go through it with the
        // inline's `this` homed in a temporary (DORef(DOHandle) at 0x82A80540,
        // MatchOperation's ctor at 0x82B481D0).
        DOHandle &operator=(const DOHandle &h) {
            mValue = h.mValue;
            return *this;
        }

        unsigned int GetValue() const { return mValue; }
        unsigned int GetDOClassID() const { return (mValue & 0xFFC00000) >> 22; }
        unsigned int GetID() const {
            unsigned int uiID = mValue & 0x3FFFFF;
            return uiID;
        }
        bool IsA(unsigned int id) const { return (mValue & 0xFFC00000) >> 22 == id; }

        DOID GetDOID() const { return DOID(GetID()); }
        bool operator<(const DOHandle &h) const { return mValue < h.mValue; }
        bool operator==(const DOHandle &h) const { return mValue == h.mValue; }
        bool operator!=(const DOHandle &h) const { return mValue != h.mValue; }

        bool IsAWKHandle() const;

        void SetDOClassID(unsigned int);
        void SetDOID(DOID);
        const char *GetClassNameString() const;

        unsigned int mValue; // 0x0
    };

    ByteStream &operator>>(ByteStream &, DOHandle &);
}
