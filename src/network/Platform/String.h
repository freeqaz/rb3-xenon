#pragma once
#include "Platform/RootObject.h"
#include "Platform/StringStream.h"
#include "types.h"

namespace Quazal {
    class DebugString {
    public:
        DebugString() {}
    };

    class String : public RootObject {
    public:
        String();
        String(const char *);
        String(const String &);
        ~String();
        Quazal::String &operator=(const char *);
        Quazal::String &operator=(const Quazal::String &);
        bool operator<(const Quazal::String &) const;
        void Reserve(int);
        uint GetLength() const;
        void CreateCopy(char **) const;
        void Format(const char *, ...);
        String(const wchar_t *);
        Quazal::String &operator=(const wchar_t *);
        Quazal::String &operator+=(const Quazal::String &);
        Quazal::String Left(unsigned int) const;
        void CreateCopy(wchar_t **) const;
        void ToUpper();
        void ToLower();
        int Find(const char *) const;
        int FindNoCase(const char *) const;
        operator const char *() const { return m_szContent; }
#ifdef RB3_QUAZAL_STRING_OPEQ
        bool operator==(const String &s) const { return IsEqual(m_szContent, s.m_szContent); }
#endif

        char *m_szContent;

        static bool IsEqual(const char *, const char *);
        static void ReleaseCopy(char *);
        static void ReleaseCopy(wchar_t *);
        static uint s_uiDefaultStringEncoding;
    };

    String operator+(const Quazal::String &, const Quazal::String &);
    String operator+(const Quazal::String &, const char *);
    String operator+(const char *, const Quazal::String &);
    StringStream &operator<<(Quazal::StringStream &, const Quazal::String &);
}
