#pragma once
#include "utl/MemMgr.h"
#include "utl/TextStream.h"
#include "utl/Symbol.h"
#include <cstring>
#include <vector>

// i can't think of a better place to put this
inline bool IsAsciiNum(char c) { return c >= 0x30 && c <= 0x39; }

// ditto
inline bool streq(const char *s1, const char *s2) { return strcmp(s1, s2) == 0; }

inline bool strneq(const char *s1, const char *s2, int n) {
    return strncmp(s1, s2, n) == 0;
}

#ifdef HX_NATIVE
inline bool strieq(const char *s1, const char *s2) { return strcasecmp(s1, s2) == 0; }
#else
inline bool strieq(const char *s1, const char *s2) { return stricmp(s1, s2) == 0; }
#endif

class FixedString {
protected:
    char *mStr; // 0x0
public:
    FixedString();
    FixedString(char *, int);

    unsigned int length() const { return strlen(mStr); }
    unsigned int size() const { return strlen(mStr); }
    unsigned int capacity() const { return *(unsigned int *)(mStr - 4); }
    const char *c_str() const { return mStr; }
    bool empty() const { return *mStr == '\0'; }

    bool operator<(const FixedString &) const;
    FixedString &operator+=(const char *);
    bool contains(const char *) const;

    unsigned int find(const char *) const;
    unsigned int find(char, unsigned int) const;
    unsigned int find(char) const;
    unsigned int find_last_of(char) const;
    unsigned int find_last_of(const char *) const;
    unsigned int find(const char *, unsigned int) const;
    unsigned int find_first_of(const char *, unsigned int) const;

    char &operator[](unsigned int);

    void ToLower();
    void ToUpper();
    void ReplaceAll(char, char);
    int compare(unsigned int, unsigned int, const char *) const;

    static const unsigned int npos;
};

// Retail String layout (X360): {vptr@0, mCap@4, mStr@8} = 0xC bytes.
// Verified from retail ctor fn_82798E18: param_1[0]=vptr, param_1[1]=0 (mCap),
// param_1[2]=gNullStr (mStr). resize fn_82798E68 uses iVar1+4 for mCap,
// iVar1+8 for mStr.
// String inherits only from TextStream (brings vptr@0), with mCap and mStr
// as explicit members. FixedString is kept only for StackString.
class String : public TextStream {
public:
    unsigned int mCap; // 0x4
    char *mStr;        // 0x8

    virtual ~String();
    virtual void Print(const char *str) { *this += str; }

    String();
    String(const char *);
    String(Symbol);
    String(const String &);
    String(unsigned int, char);

    unsigned int length() const { return strlen(mStr); }
    unsigned int size() const { return strlen(mStr); }
    unsigned int capacity() const { return mCap; }
    const char *c_str() const { return mStr; }
    bool empty() const { return *mStr == '\0'; }

    bool operator<(const String &) const;
    bool operator==(const FixedString &) const;
    bool operator==(const String &s) const { return strcmp(mStr, s.mStr) == 0; }
    bool operator!=(const String &s) const { return strcmp(mStr, s.mStr) != 0; }

    void reserve(unsigned int);

    String operator+(const char *) const;
    String operator+(char) const;
    String operator+(const FixedString &) const;
    String operator+(const String &s) const { String r(*this); r += s.mStr; return r; }
    String &operator+=(const char *);
    String &operator+=(Symbol);
    String &operator+=(const FixedString &);
    String &operator+=(const String &s) { return *this += s.mStr; }
    String &operator+=(char);
    String &operator=(const char *);
    String &operator=(Symbol);
    String &operator=(const FixedString &);
    String &operator=(const String &);

    // char rindex(int) const;
    // char &rindex(int);

    bool operator!=(const char *) const;
    bool operator!=(const FixedString &) const;
    bool operator==(const char *) const;
    bool operator==(Symbol) const;

    void resize(unsigned int);
    // unsigned int rfind(const char *) const;

    unsigned int find(const char *) const;
    unsigned int find(char, unsigned int) const;
    unsigned int find(char) const;
    unsigned int find_last_of(char) const;
    unsigned int find_last_of(const char *) const;
    unsigned int find(const char *, unsigned int) const;
    unsigned int find_first_of(const char *, unsigned int) const;

    char &operator[](unsigned int);
    void ToLower();
    void ToUpper();
    void ReplaceAll(char, char);
    bool contains(const char *) const;
    int compare(unsigned int, unsigned int, const char *) const;

    static const unsigned int npos;

    int split(const char *token, std::vector<String> &subStrings) const;

    String substr(unsigned int) const;
    String substr(unsigned int, unsigned int) const;

    void swap(String &);
    String &replace(unsigned int, unsigned int, const char *);
    String &erase();
    String &erase(unsigned int);
    String &erase(unsigned int, unsigned int);
    String &insert(unsigned int, unsigned int, char);
    String &insert(unsigned int, const char *);
    // String &insert(unsigned int, const String &);
};

bool SearchReplace(const char *, const char *, const char *, char *);
bool StrNCopy(char *, const char *, int);
void RemoveSpaces(char *, int, const char *);

inline TextStream &operator<<(TextStream &ts, const String &str) {
    ts.Print(str.c_str());
    return ts;
}

template <int N>
class StackString : public FixedString, public TextStream {
private:
    char mStack[N + 5];

public:
    StackString() : TextStream(), FixedString((char *)mStack, N + 5) {}
    StackString(const char *str) : TextStream(), FixedString((char *)mStack, N + 5) {
        *this += str;
    }
    // virtual ~StackString() {} // dtor is at 0x8269E480
    virtual void Print(const char *str) { *this += str; }

    StackString &operator=(const StackString &rhs) {
        const FixedString *src = &rhs;
        *mStr = '\0';
        FixedString::operator+=(src->c_str());
        for (int i = 0; i < N + 5; i++) {
            mStack[i] = rhs.mStack[i];
        }
        return *this;
    }
};
