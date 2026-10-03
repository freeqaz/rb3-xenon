#ifndef PLATFORM_STRINGSTREAM_H
#define PLATFORM_STRINGSTREAM_H
#include "Platform/RootObject.h"

namespace Quazal {
    class StringStream : public RootObject {
    public:
        StringStream();
        ~StringStream();
        void TestFreeRoom(unsigned int);

        StringStream &operator<<(const char *);
        StringStream &operator<<(unsigned long long);
        StringStream &operator<<(long long);
        StringStream &operator<<(unsigned long);
        StringStream &operator<<(long);
        StringStream &operator<<(bool);
        StringStream &operator<<(double);
        StringStream &operator<<(float);
        StringStream &operator<<(const void *);
        StringStream &operator<<(const StringStream &);
        // Retail has no int overloads: an int is inserted as a long (the "%d"
        // operator), an unsigned int as an unsigned long (the "%u" operator).
        StringStream &operator<<(int i) { return *this << (long)i; }
        StringStream &operator<<(unsigned int ui) { return *this << (unsigned long)ui; }

        unsigned int GetLength() const;
        void Reset();
        void FreeBuffer();
        void ReleaseBuffer(const char *);
        void Resize(unsigned int);
        void ShowBase();

        const char *m_szBuffer;
        unsigned int m_uiSize;
        const char *m_szCurrentPosition;
        char m_szInitialBuffer[256];
        bool m_bHex;
        bool m_bShowBase;
        bool m_bBoolAlpha;
    };

    // Manipulators: each sets a flag (endl appends "\n") and returns the stream.
    StringStream &hex(StringStream &);
    StringStream &dec(StringStream &);
    StringStream &endl(StringStream &);
    StringStream &showbase(StringStream &);
    StringStream &noshowbase(StringStream &);
    StringStream &boolalpha(StringStream &);
    StringStream &noboolalpha(StringStream &);
}

#endif
