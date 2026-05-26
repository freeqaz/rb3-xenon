#pragma once
#include <cstring>
#ifdef HX_NATIVE
#include <cstdint>
#endif

extern const char *gNullStr;

#define STR_TO_SYM(str) *reinterpret_cast<Symbol *>(const_cast<char **>(&str))

class Symbol {
private:
    const char *mStr;

public:
    Symbol() : mStr(gNullStr) {}
    Symbol(const char *);
    // Symbol(const Symbol &rhs) : mStr(rhs.mStr) {}
    // Symbol& operator=(const Symbol& rhs){
    //     mStr = rhs.mStr;
    //     return *this;
    // }

    const char *Str() const { return mStr; }
    bool operator<(const Symbol &s) const { return mStr < s.mStr; }
    bool Null() const { return mStr == gNullStr; }
    bool operator==(const Symbol &s) const { return mStr == s.mStr; }
    bool operator!=(const Symbol &s) const { return mStr != s.mStr; }
    bool operator!=(const char *cc) const { return !(*this == cc); }
    bool operator==(const char *cc) const {
        if (cc)
            return strcmp(mStr, cc) == 0;
        else
            return Null();
    }
#ifdef HX_NATIVE
    // LP64 WARNING: This truncates an 8-byte pointer to 4 bytes!
    // Do NOT use for comparison — use Symbol::operator== or Symbol::Str() instead.
    // Kept for PPC compatibility paths only.
    [[deprecated("Symbol→int truncates on LP64; use Symbol::operator== or Str()")]]
    operator int() { return (int)(intptr_t)mStr; }
#else
    operator int() { return (int)mStr; }
#endif

    static void PreInit(int, int);
    static void Init(void);
    static void Terminate(void);
};

inline void Interp(const Symbol &s1, const Symbol &s2, float f, Symbol &s3) {
    s3 = (f < 1.0f) ? s1 : s2;
}
