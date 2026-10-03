// Quazal NetZ - .\Core\String.cpp
// Retail TU: .text 0x82A86ED0..0x82A87F20, /Od /Oi- /Ob1 /EHs-c-. See the end
// of the file for the extent and flag evidence.
#include "Platform/String.h"

typedef char *va_list;
#define va_start(ap, v) __va_start(&ap, v)
#define va_end(ap) (ap = (va_list)0)

extern "C" {
void __va_start(va_list *, ...);
unsigned int strlen(const char *);
char *strcpy(char *, const char *);
int strcmp(const char *, const char *);
char *strstr(const char *, const char *);
void *memcpy(void *, const void *, unsigned int);
int vsnprintf(char *, unsigned int, const char *, va_list);
int toupper(int);
int tolower(int);
unsigned int wcslen(const wchar_t *);
int MultiByteToWideChar(unsigned int, unsigned long, const char *, int, wchar_t *, int);
int WideCharToMultiByte(
    unsigned int, unsigned long, const wchar_t *, int, char *, int, const char *, int *
);
}

inline void *operator new(unsigned int, void *place) { return place; }

namespace Quazal {
    // The allocator entry points this TU calls (Platform/MemoryManager.h).
    // The header is not included: it pulls in the CRT's fixed-argument
    // __va_start declaration, and Format needs the variadic intrinsic.
    class MemoryManager {
    public:
        enum _InstructionType {
            _InstType9 = 9
        };
        static MemoryManager *GetDefaultMemoryManager();
        static void *
        Allocate(MemoryManager *, unsigned int, const char *, unsigned int, _InstructionType);
        static void Free(MemoryManager *, void *, _InstructionType);
    };

    template <class T>
    T *qNewArray(unsigned int count, const char *file, int line);
    template <class T>
    void qDeleteArray(T *arr);
}

#line 43
static void CopyString(char **pszDest, const char *szSource) {
    if (szSource == 0) {
        *pszDest = 0;
    } else {
        *pszDest = Quazal::qNewArray<char>(strlen(szSource) + 1, __FILE__, __LINE__);
        strcpy(*pszDest, szSource);
    }
}

#line 53
static void CopyString(wchar_t **pszDest, const char *szSource) {
    if (szSource == 0) {
        *pszDest = 0;
    } else {
        unsigned int uiSize = strlen(szSource) + 1;
        *pszDest = Quazal::qNewArray<wchar_t>(uiSize, __FILE__, __LINE__);
        MultiByteToWideChar(0, 0, szSource, -1, *pszDest, uiSize);
    }
}

#line 67
static void CopyString(char **pszDest, const wchar_t *szSource) {
    if (szSource == 0) {
        *pszDest = 0;
    } else {
        unsigned int uiSize = wcslen(szSource) + 1;
        *pszDest = Quazal::qNewArray<char>(uiSize, __FILE__, __LINE__);
        WideCharToMultiByte(0, 0, szSource, -1, *pszDest, uiSize, 0, 0);
    }
}

namespace Quazal {
    String::String() { CopyString(&m_szContent, ""); }

    String::String(const char *szContent) { CopyString(&m_szContent, szContent); }

    String::String(const wchar_t *szContent) { CopyString(&m_szContent, szContent); }

    String::String(const String &oString) { CopyString(&m_szContent, oString.m_szContent); }

    String::~String() {
        if (m_szContent)
            qDeleteArray(m_szContent);
    }

    String &String::operator=(const char *szContent) {
        if (m_szContent == szContent)
            return *this;
        if (m_szContent)
            qDeleteArray(m_szContent);
        CopyString(&m_szContent, szContent);
        return *this;
    }

    String &String::operator=(const wchar_t *szContent) {
        if (m_szContent)
            qDeleteArray(m_szContent);
        CopyString(&m_szContent, szContent);
        return *this;
    }

    String &String::operator=(const String &oString) {
        if (m_szContent != oString.m_szContent) {
            if (m_szContent)
                qDeleteArray(m_szContent);
            CopyString(&m_szContent, oString.m_szContent);
        }
        return *this;
    }

    bool String::IsEqual(const char *szString1, const char *szString2) {
        if (szString1 == szString2)
            return true;
        if (szString1 == 0)
            return IsEqual("", szString2);
        if (szString2 == 0)
            return IsEqual(szString1, "");
        return strcmp(szString1, szString2) == 0;
    }

    bool String::operator<(const String &oString) const {
        if (m_szContent == oString.m_szContent)
            return false;
        if (m_szContent == 0)
            return String("") < oString;
        if (oString.m_szContent == 0)
            return *this < String("");
        return strcmp(*this, oString) < 0;
    }

    String &String::operator+=(const String &oString) {
        *this = *this + oString;
        return *this;
    }

    String String::Left(unsigned int uiCount) const {
        String strResult;
        unsigned int uiCurrentLength = GetLength();
        unsigned int uiCopySize = uiCurrentLength < uiCount ? uiCurrentLength : uiCount;
        strResult.Reserve(uiCopySize + 1);
        memcpy(strResult.m_szContent, m_szContent, uiCopySize);
        strResult.m_szContent[uiCopySize] = 0;
        return strResult;
    }

#line 200
    void String::Reserve(int iSize) {
        if (m_szContent)
            qDeleteArray(m_szContent);
        m_szContent = qNewArray<char>(iSize, __FILE__, __LINE__);
        *m_szContent = 0;
    }

    uint String::GetLength() const {
        if (m_szContent == 0) {
            return 0;
        } else {
            return strlen(m_szContent);
        }
    }

    void String::CreateCopy(char **pszCopy) const { CopyString(pszCopy, m_szContent); }

    void String::ReleaseCopy(char *szCopy) {
        if (szCopy)
            qDeleteArray(szCopy);
    }

    void String::Format(const char *szFormat, ...) {
        va_list args;
        va_start(args, szFormat);
        const unsigned int uiBufferSize = 0x1000;
        char szBuffer[uiBufferSize];
        vsnprintf(szBuffer, uiBufferSize, szFormat, args);
        va_end(args);
        *this = szBuffer;
    }

    void String::CreateCopy(wchar_t **pszCopy) const { CopyString(pszCopy, m_szContent); }

    void String::ReleaseCopy(wchar_t *szCopy) {
        if (szCopy)
            qDeleteArray(szCopy);
    }

    void String::ToUpper() {
        if (m_szContent) {
            for (char *p = m_szContent; *p; p++) {
                *p = toupper(*p);
            }
        }
    }

    void String::ToLower() {
        if (m_szContent) {
            for (char *p = m_szContent; *p; p++) {
                *p = tolower(*p);
            }
        }
    }

    int String::Find(const char *szSubString) const {
        const int iNotFound = -1;
        int iIndex = -1;
        if (m_szContent && szSubString && *m_szContent && *szSubString) {
            char *szFound = strstr(*this, szSubString);
            if (szFound)
                iIndex = szFound - m_szContent;
        }
        return iIndex;
    }

    int String::FindNoCase(const char *szSubString) const {
        const int iNotFound = -1;
        int iIndex = -1;
        if (m_szContent && szSubString && *m_szContent && *szSubString) {
            String strCopy(m_szContent);
            strCopy.ToUpper();
            String strSubString(szSubString);
            strSubString.ToUpper();
            char *szFound = strstr(strCopy, strSubString);
            if (szFound)
                iIndex = szFound - strCopy.m_szContent;
        }
        return iIndex;
    }
}

namespace {
    Quazal::String _Copy(
        const char *szLeft, unsigned int uiSizeLeft, const char *szRight, unsigned int uiSizeRight
    ) {
        Quazal::String str;
        str.Reserve(uiSizeLeft + uiSizeRight + 1);
        char *szBuffer = str.m_szContent;
        strcpy(szBuffer, szLeft);
        strcpy(szBuffer + uiSizeLeft, szRight);
        return str;
    }
}

namespace Quazal {
    String operator+(const String &strLeft, const String &strRight) {
        unsigned int uiSizeLeft = strLeft.GetLength();
        if (uiSizeLeft == 0)
            return strRight;
        unsigned int uiSizeRight = strRight.GetLength();
        if (uiSizeRight == 0)
            return strLeft;
        return _Copy(strLeft, uiSizeLeft, strRight, uiSizeRight);
    }

    String operator+(const String &strLeft, const char *szRight) {
        unsigned int uiSizeLeft = strLeft.GetLength();
        if (uiSizeLeft == 0)
            return szRight;
        if (szRight == 0 || *szRight == 0)
            return strLeft;
        unsigned int uiSizeRight = strlen(szRight);
        return _Copy(strLeft, uiSizeLeft, szRight, uiSizeRight);
    }

    String operator+(const char *szLeft, const String &strRight) {
        if (szLeft == 0 || *szLeft == 0)
            return strRight;
        unsigned int uiSizeRight = strRight.GetLength();
        if (uiSizeRight == 0)
            return szLeft;
        unsigned int uiSizeLeft = strlen(szLeft);
        return _Copy(szLeft, uiSizeLeft, strRight, uiSizeRight);
    }

    StringStream &operator<<(StringStream &oStream, const String &strString) {
        return oStream << (const char *)strString;
    }
}

namespace Quazal {
    // Array new/delete with an element-count header in front of the array.
    template <class T>
    T *qNewArray(unsigned int count, const char *file, int line) {
        void *pMemory = MemoryManager::Allocate(
            MemoryManager::GetDefaultMemoryManager(),
            count * sizeof(T) + 4,
            file,
            line,
            MemoryManager::_InstType9
        );
        unsigned int *puiHeader = (unsigned int *)pMemory;
        T *pArray = (T *)(puiHeader + 1);
        *puiHeader = count;
        for (unsigned int i = 0; i < count; i++) {
            new (&pArray[i]) T;
        }
        return pArray;
    }

    template <class T>
    void qDeleteArray(T *arr) {
        unsigned int *puiHeader = ((unsigned int *)arr) - 1;
        void *pMemory = puiHeader;
        T *pArray = arr;
        for (unsigned int i = 0; i < *puiHeader; i++) {
        }
        MemoryManager::Free(
            MemoryManager::GetDefaultMemoryManager(), pMemory, MemoryManager::_InstType9
        );
    }
}

// Retail TU: .text 0x82A86ED0..0x82A87F20, compiled /Od /Oi- /Ob1 /EHs-c-
// (see objects.json). Its .rdata is the TU's basic_string literal
// (0x8217E378), the five "" literals in source order (String(), IsEqual x2,
// operator< x2), then the two __FILE__ strings (Reserve's, then the shared
// copy used by the CopyString helpers). No function has an unwind funclet,
// although operator< and the operator+ family destroy String temporaries, so
// the TU is built without EH. String has no vtable.
//
// The functions before 0x82A86ED0 (getters/setters over the vtable at
// 0x8217D980) are the previous object's COMDAT tail; 0x82A87F20 is the next
// object's constructor (vtable 0x8217E3B8, after this TU's __FILE__ strings).
//
// The COMDAT tail of this object holds the three CopyString helpers and the
// qNewArray<char>, qNewArray<wchar_t> and qDeleteArray instantiations every
// Quazal TU calls. Retail has a single qDeleteArray body (0x82A87E98): the
// char and wchar_t instantiations are byte-identical and folded.
//
// 0x82A87B78 (12 bytes, an empty function of two arguments, no references
// in retail) sits between operator<< and the CopyString helpers. It is not
// written here.
//
// This TU is built /Od: its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES above determine the stack offsets, and
// the allocations pass __FILE__/__LINE__, so #line reproduces retail's lines
// (47, 58 and 72 in the CopyString helpers, 203 in Reserve). The #line
// directives only ever move the line number forward; this file's prologue is
// kept short for that reason.
//
// operator< is one slot pair short of retail: retail stores the two
// operator const char * results with oString's at 0x64 and *this's at 0x60,
// ours the other way round. Spellings of the comparison, named locals and an
// inline wrapper did not reproduce it.

