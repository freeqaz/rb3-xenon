// Quazal NetZ - .\Core\StringStream.cpp
//
// Retail TU: .text 0x82AB34C0..0x82AB3DC8, built /Od /Oi- /Ob1 /GR- /EHs-c-
// (see objects.json). Its .rdata is the __FILE__ string followed by the
// format strings of the insertion operators ("%x"/"%u", "%x"/"%d", "0x",
// "(null)", "true"/"false", "%f", "%I64x"/"%I64u"/"%I64d", "\n"); the TU has
// no vtable and no EH tables.
//
// The buffer starts as the 256-byte member array and grows by half its size
// until the requested room fits; a grown buffer is a qNewArray<char> block.

#include "Platform/StringStream.h"

#define SS_FILE ".\\Core\\StringStream.cpp"

extern "C" {
void *memcpy(void *, const void *, unsigned int);
unsigned int strlen(const char *);
char *strcpy(char *, const char *);
int sprintf(char *, const char *, ...);
int _snprintf(char *, unsigned int, const char *, ...);
}

namespace Quazal {

    // Array new/delete with an element-count header; defined in another TU.
    template <class T>
    T *qNewArray(unsigned int count, const char *file, int line);
    template <class T>
    void qDeleteArray(T *arr);

    StringStream::StringStream() {
        m_szBuffer = m_szInitialBuffer;
        m_uiSize = 0x100;
        m_szCurrentPosition = m_szBuffer;
        m_szInitialBuffer[0] = 0;
        m_bHex = false;
        m_bShowBase = true;
        m_bBoolAlpha = true;
    }

    StringStream::~StringStream() { FreeBuffer(); }

    unsigned int StringStream::GetLength() const {
        return m_szCurrentPosition - m_szBuffer;
    }

    void StringStream::Reset() {
        FreeBuffer();
        m_szBuffer = m_szInitialBuffer;
        m_uiSize = 0x100;
        m_szCurrentPosition = m_szBuffer;
        m_szInitialBuffer[0] = 0;
    }

    void StringStream::FreeBuffer() { ReleaseBuffer(m_szBuffer); }

    void StringStream::ReleaseBuffer(const char *szBuffer) {
        if (szBuffer == m_szInitialBuffer) {
        } else {
            qDeleteArray((char *)szBuffer);
        }
    }

    void StringStream::Resize(unsigned int uiNewSize) {
        unsigned int uiCurrentLength = GetLength();
        const char *szPreviousBuffer = m_szBuffer;
#line 107
        m_szBuffer = qNewArray<char>(uiNewSize, SS_FILE, __LINE__);
        m_uiSize = uiNewSize;
        memcpy((void *)m_szBuffer, szPreviousBuffer, uiCurrentLength + 1);
        m_szCurrentPosition = m_szBuffer + uiCurrentLength;
        ReleaseBuffer(szPreviousBuffer);
    }

    void StringStream::TestFreeRoom(unsigned int uiRoom) {
        unsigned int uiSize = m_uiSize;
        unsigned int uiNewSize;
        if (GetLength() + uiRoom > uiSize) {
            uiNewSize = m_uiSize;
            while (uiNewSize < GetLength() + uiRoom) {
                uiNewSize += uiNewSize / 2;
            }
            Resize(uiNewSize);
        }
    }

    StringStream &StringStream::operator<<(unsigned long ul) {
        TestFreeRoom(0x20);
        const char *szFormat;
        if (m_bHex) {
            ShowBase();
            szFormat = "%x";
        } else {
            szFormat = "%u";
        }
        m_szCurrentPosition += _snprintf((char *)m_szCurrentPosition, 0x20, szFormat, ul);
        return *this;
    }

    StringStream &StringStream::operator<<(long l) {
        TestFreeRoom(0x20);
        const char *szFormat;
        if (m_bHex) {
            ShowBase();
            szFormat = "%x";
        } else {
            szFormat = "%d";
        }
        m_szCurrentPosition += _snprintf((char *)m_szCurrentPosition, 0x20, szFormat, l);
        return *this;
    }

    void StringStream::ShowBase() {
        if (m_bShowBase) {
            strcpy((char *)m_szCurrentPosition, "0x");
            m_szCurrentPosition += 2;
        }
    }

    StringStream &StringStream::operator<<(const char *sz) {
        if (sz == 0) {
            return *this << "(null)";
        }
        unsigned int uiLength = strlen(sz);
        TestFreeRoom(uiLength + 1);
        strcpy((char *)m_szCurrentPosition, sz);
        m_szCurrentPosition += uiLength;
        return *this;
    }

    StringStream &StringStream::operator<<(bool b) {
        if (m_bBoolAlpha) {
            if (b) {
                return *this << "true";
            } else {
                return *this << "false";
            }
        } else {
            return *this << (long)b;
        }
    }

    StringStream &StringStream::operator<<(double d) {
        TestFreeRoom(0x20);
        m_szCurrentPosition += _snprintf((char *)m_szCurrentPosition, 0x20, "%f", d);
        return *this;
    }

    StringStream &StringStream::operator<<(float f) {
        TestFreeRoom(0x20);
        m_szCurrentPosition += _snprintf((char *)m_szCurrentPosition, 0x20, "%f", f);
        return *this;
    }

    StringStream &StringStream::operator<<(const void *p) {
        TestFreeRoom(0x20);
        ShowBase();
        m_szCurrentPosition += sprintf((char *)m_szCurrentPosition, "%x", p);
        return *this;
    }

    StringStream &StringStream::operator<<(unsigned long long ull) {
        TestFreeRoom(0x20);
        const char *szFormat;
        if (m_bHex) {
            ShowBase();
            szFormat = "%I64x";
        } else {
            szFormat = "%I64u";
        }
        m_szCurrentPosition += _snprintf((char *)m_szCurrentPosition, 0x20, szFormat, ull);
        return *this;
    }

    StringStream &StringStream::operator<<(long long ll) {
        TestFreeRoom(0x20);
        const char *szFormat;
        if (m_bHex) {
            ShowBase();
            szFormat = "%I64x";
        } else {
            szFormat = "%I64d";
        }
        m_szCurrentPosition += _snprintf((char *)m_szCurrentPosition, 0x20, szFormat, ll);
        return *this;
    }

    StringStream &StringStream::operator<<(const StringStream &ss) {
        return *this << ss.m_szBuffer;
    }

    StringStream &hex(StringStream &ss) {
        ss.m_bHex = true;
        return ss;
    }

    StringStream &dec(StringStream &ss) {
        ss.m_bHex = false;
        return ss;
    }

    StringStream &endl(StringStream &ss) { return ss << "\n"; }

    StringStream &showbase(StringStream &ss) {
        ss.m_bShowBase = true;
        return ss;
    }

    StringStream &noshowbase(StringStream &ss) {
        ss.m_bShowBase = false;
        return ss;
    }

    StringStream &boolalpha(StringStream &ss) {
        ss.m_bBoolAlpha = true;
        return ss;
    }

    StringStream &noboolalpha(StringStream &ss) {
        ss.m_bBoolAlpha = false;
        return ss;
    }
}
