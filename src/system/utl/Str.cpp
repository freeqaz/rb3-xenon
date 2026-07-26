#include "utl/Str.h"
#include "Str.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include <cctype>

#ifdef HX_NATIVE
const unsigned int FixedString::npos = (unsigned int)-1;
const unsigned int String::npos = (unsigned int)-1;
#else
const unsigned int String::npos = (unsigned int)-1;
#endif

// FixedString (for StackString) uses gEmpty+4 with capacity stored at gEmpty[0..3].
char gEmpty[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// A default-constructed String's mStr points at this shared empty-buffer sentinel
// (a zero-initialized .bss char). Retail bakes its *address* directly into every
// String ctor (lis/addi of a fixed .bss VA) — mStr = &gNullStrBuf — NOT a load of a
// `const char*` variable's value. Mirrors rb3-Wii's `char gEmpty; mStr(&gEmpty)`.
char gNullStrBuf = 0;

// gNullStr ("" in .rdata) backs Symbol/comparison use; distinct from gNullStrBuf.
extern const char *gNullStr;

// Retail's NUL-terminated copy loop is NOT MSVC's /Oi `strcpy` intrinsic: it keeps
// the copied character in a 32-bit *unsigned* temp, so the loop test compiles to
//     lbzu rC,1(rS) / cmplwi rC,0 / stbu rC,1(rD) / bne
// whereas `strcpy(d, s)` (== `while ((*d++ = *s++) != 0);`, the char-typed form)
// emits `extsb. rT,rC` in the compare slot instead. Verified by compiling both
// shapes with the retail cl.exe (X360/16.00.11886.00, /O1 /Oi): only the
// unsigned-int temp reproduces `cmplwi`. Keep this file-local + static so it
// inlines away and emits no COMDAT of its own.
static void CopyStrZ(char *dest, const char *src) {
    --src;
    --dest;
    unsigned int c;
    do {
        c = (unsigned char)*++src;
        *++dest = (char)c;
    } while (c != 0);
}

void RemoveSpaces(char *out, int len, const char *in) {
    MILO_ASSERT(out, 0x2C0);
    MILO_ASSERT(in, 0x2C1);
    MILO_ASSERT(len > 0, 0x2C2);

    char *max = out + len - 1;
    char *orig = out;
    bool wasSpace = true;
    char c = *in;

    while (c != '\0') {
        if (out < max) {
            bool isSpace = (c == ' ');
            if (!isSpace || !wasSpace) {
                *out++ = c;
            }
            wasSpace = isSpace;
        } else {
            break;
        }
        c = *++in;
    }
    if (out > orig && *(out - 1) == ' ') {
        out--;
    }
    *out = '\0';
}

// searches for occurrences of substring substr_old within string src, and replaces each
// occurrence with substr_new. the result goes in dest. if a change was made, this fn
// returns true. if no changes to the original string were made, return false
bool SearchReplace(
    const char *src, const char *substr_old, const char *substr_new, char *dest
) {
    bool changed;
    int matchOffset;
    char *matchPtr;

    *dest = 0;
    changed = false;

    while (true) {
        matchPtr = (char *)strstr(src, substr_old);
        if (matchPtr == 0)
            break;
        matchOffset = matchPtr - src;
        strncat(dest, src, matchOffset);
        strcat(dest, substr_new);
        src = strlen(substr_old) + (src + matchOffset);
        changed = true;
    }

    strcat(dest, src);
    return changed;
}

// sorta like strncpy, except for the return value
// returns true if the copy operation was terminated because of reaching the maximum
// length or encountering the end of src, and 0 otherwise.
bool StrNCopy(char *dest, const char *src, int n) {
    MILO_ASSERT(n, 0x2F7);

    for (n = n - 1; *src != '\0' && n != 0; n--) {
        *dest++ = *src++;
    }
    *dest = '\0';

    return (n != 0 || *src == '\0');
}

#pragma region FixedString

FixedString::FixedString() : mStr((char *)(gEmpty + 4)) {
    *(int *)(mStr - 4) = 0;
    mStr[0] = '\0';
}

FixedString::FixedString(char *str, int bufferSize) {
    mStr = str + 4;
    MILO_ASSERT(bufferSize >= 5, 0x1C);
    *(int *)(mStr - 4) = bufferSize - 5;
    mStr[0] = '\0';
}

FixedString &FixedString::operator+=(const char *str) {
    if (str && *str) {
        char *p;
        char *max = mStr + capacity();
        for (p = mStr + strlen(mStr); p < max && *str; str++) {
            *p++ = *str;
        }
        *p = '\0';
    }
    return *this;
}

bool FixedString::operator<(const FixedString &str) const {
    return strcmp(mStr, str.c_str()) < 0;
}

unsigned int FixedString::find_last_of(char c) const {
    char *found = strrchr(mStr, c);
    if (found)
        return found - mStr;
    else
        return -1;
}

unsigned int FixedString::find_last_of(const char *str) const {
    if (!str)
        return -1;
    int a = -1;
    for (char *tmp = (char *)str; *tmp != '\0'; tmp++) {
        int lastIdx = find_last_of(*tmp);
        if (lastIdx != -1 && lastIdx > a) {
            a = lastIdx;
        }
    }
    if (a == -1)
        return -1;
    else
        return a;
}

// Retail ICF-folds FixedString::{ToLower,ToUpper,ReplaceAll} with their String
// counterparts (the FixedString-mangled symbol survived). The retained body reads
// the buffer at String's mStr@8, so for the X360 match operate on the String-layout
// pointer. The native build keeps true FixedString semantics (mStr@0).
#ifdef HX_NATIVE
#define FIXEDSTRING_BUF mStr
#else
#define FIXEDSTRING_BUF reinterpret_cast<String *>(this)->mStr
#endif

void FixedString::ToLower() {
    char *p;
    for (p = FIXEDSTRING_BUF; *p != '\0'; p++) {
        *p = tolower(*p);
    }
}

void FixedString::ToUpper() {
    char *p;
    for (p = FIXEDSTRING_BUF; *p != '\0'; p++) {
        *p = toupper(*p);
    }
}

void FixedString::ReplaceAll(char old_char, char new_char) {
    char *p;
    for (p = FIXEDSTRING_BUF; *p != '\0'; p++) {
        if (*p == old_char)
            *p = new_char;
    }
}

#undef FIXEDSTRING_BUF

unsigned int FixedString::find(char c, unsigned int pos) const {
    MILO_ASSERT(pos <= capacity(), 0x6C);
    char *p = &mStr[pos];
    while ((*p != '\0') && (*p != c))
        p++;
    if (*p != '\0')
        return p - mStr;
    else
        return -1;
}

unsigned int FixedString::find(char c) const { return find(c, 0); }

unsigned int FixedString::find(const char *str, unsigned int pos) const {
    MILO_ASSERT(pos <= capacity(), 0x83);
    char *found = strstr(&mStr[pos], str);
    if (found != 0)
        return found - mStr;
    else
        return -1;
}

unsigned int FixedString::find_first_of(const char *str, unsigned int pos) const {
    char *p1;
    char *p2;
    if (str == 0)
        return -1;
    MILO_ASSERT(pos <= capacity(), 0x8E);
    for (p1 = &mStr[pos]; *p1 != '\0'; p1++) {
        for (p2 = (char *)str; *p2 != '\0'; p2++) {
            if (*p1 == *p2)
                return p1 - mStr;
        }
    }
    return -1;
}

int FixedString::compare(unsigned int pos, unsigned int i2, const char *str) const {
    if (!str)
        return -1;
    else {
        MILO_ASSERT(pos <= capacity(), 0xDD);
        return strncmp(mStr + pos, str, i2);
    }
}

char &FixedString::operator[](unsigned int i) {
    MILO_ASSERT(i < capacity(), 0xFC);
    return *(mStr + i);
}

unsigned int FixedString::find(const char *cc) const { return find(cc, 0); }

bool FixedString::contains(const char *str) const { return find(str) != -1; }

#pragma endregion
#pragma region String

// Retail String layout: {vptr@0, mCap@4, mStr@8} = 0xC bytes.
// mCap is the allocated buffer size (number of chars, excluding null terminator).
// mStr points to the start of the char buffer (gNullStr when mCap == 0).
// Free: MemOrPoolFree(mCap + 1, mStr) — allocations are mCap+1 bytes, no header prefix.

String::String() : mCap(0), mStr(&gNullStrBuf) {}

String::String(const char *str) : mCap(0), mStr(&gNullStrBuf) { *this = str; }

String::String(Symbol s) : mCap(0), mStr(&gNullStrBuf) { *this = s; }

String::String(unsigned int len, char c) : mCap(0), mStr(&gNullStrBuf) {
    reserve(len);
    for (unsigned int i = 0; i < len; i++)
        mStr[i] = c;
    mStr[len] = '\0';
}

String::String(const String &str) : mCap(0), mStr(&gNullStrBuf) { *this = str.c_str(); }

String::~String() {
    if (mCap != 0) {
        MemOrPoolFree(mCap + 1, mStr);
    }
}

bool String::operator!=(const char *str) const {
    if (str == 0)
        return true;
    else
        return strcmp(str, mStr);
}

bool String::operator==(const char *str) const {
    if (str == 0)
        return false;
    else
        return strcmp(str, mStr) == 0;
}

bool String::operator!=(const String &str) const { return strcmp(str.mStr, mStr); }

bool String::operator==(const String &str) const { return strcmp(str.mStr, mStr) == 0; }

String &String::erase() {
    *mStr = 0;
    return *this;
}

bool String::operator!=(const FixedString &str) const {
    return strcmp(str.c_str(), mStr);
}

bool String::operator==(const FixedString &str) const {
    return strcmp(str.c_str(), mStr) == 0;
}

bool String::operator==(Symbol s) const { return strcmp(s.Str(), mStr) == 0; }

void String::reserve(unsigned int len) {
    if (len > mCap) {
#ifdef HX_NATIVE
        void *dst = MemOrPoolAlloc(len + 1, __FILE__, 0x13B, "StringBuf");
#else
        // Retail/match: no __FILE__/line/"StringBuf" — see MemMgr.h ABI note.
        void *dst = MemOrPoolAlloc(len + 1);
#endif
        memcpy(dst, mStr, mCap + 1);
        *((char *)dst + len) = 0;
        if (mCap != 0) {
            MemOrPoolFree(mCap + 1, mStr);
        }
        mCap = len;
        mStr = (char *)dst;
    }
}

String &String::operator+=(const char *str) {
    if (str == 0 || *str == '\0')
        return *this;
    int len = length();
    reserve(len + strlen(str));
    unsigned char *d = (unsigned char *)&mStr[len] - 1;
    const unsigned char *s = (const unsigned char *)str - 1;
    unsigned int c;
    do {
        c = *++s;
        *++d = (unsigned char)c;
    } while (c != 0);
    return *this;
}

String &String::operator+=(Symbol s) { return *this += s.Str(); }

String &String::operator+=(const FixedString &str) { return *this += str.c_str(); }

String &String::operator+=(char c) {
    int len = length();
    reserve(len + 1);
    mStr[len] = c;
    mStr[len + 1] = '\0';
    return *this;
}

// Retail ICF-folds operator=(const String&) into the operator=(const FixedString&)
// body (the FixedString-mangled symbol won the COMDAT tie). Both bodies are
// byte-identical: they read the source's mCap@4 and mStr@8 (String layout), so for
// the X360 match the FixedString overload reinterprets its arg as a String. The
// native build keeps true FixedString semantics (its only member, mStr@0).
String &String::operator=(const FixedString &fstr) {
#ifdef HX_NATIVE
    reserve(fstr.capacity());
    if (mStr != fstr.c_str()) // avoid ASan strcpy-param-overlap on self-assignment
        strcpy(mStr, fstr.c_str());
#else
    const String &str = reinterpret_cast<const String &>(fstr);
    reserve(str.mCap);
    CopyStrZ(mStr, str.mStr);
#endif
    return *this;
}

String &String::operator=(const String &str) {
    reserve(str.mCap);
#ifdef HX_NATIVE
    if (mStr != str.mStr) // avoid ASan strcpy-param-overlap on self-assignment
#endif
    strcpy(mStr, str.mStr);
    return *this;
}

void String::resize(unsigned int arg) {
    reserve(arg);
    mStr[arg] = 0;
}

// replaces this->text with the contents of buffer, at this->text index length
// pos: the starting index of the text you want to replace
// length: how many chars you want the replacement to be
// buffer: the replacement chars
String &String::replace(unsigned int pos, unsigned int n, const char *buffer) {
    char *destPtr;
    char *srcPtr;
    unsigned int bufferLength, end;
    MILO_ASSERT(pos <= capacity(), 0x241);
    end = pos + n;
    if (end > capacity()) {
        n = capacity() - pos;
    }
    bufferLength = strlen(buffer);
    if (bufferLength > n) {
        String str_tmp;
        str_tmp.reserve(bufferLength + (length() - n));
        strncpy(str_tmp.mStr, mStr, pos);
        strncpy(str_tmp.mStr + pos, buffer, bufferLength);
        strcpy(str_tmp.mStr + (bufferLength + pos), mStr + (n + pos));
        swap(str_tmp);
    } else {
        strncpy(mStr + pos, buffer, bufferLength);
        destPtr = mStr + pos + bufferLength;
        srcPtr = mStr + pos + n;
        while (*srcPtr != '\0') {
            *destPtr++ = *srcPtr++;
        }
        *destPtr = *srcPtr;
    }
    return *this;
}

String &String::erase(unsigned int pos) {
    if (pos < capacity()) {
        mStr[pos] = '\0';
    }
    return *this;
}

String &String::erase(unsigned int start, unsigned int count) {
    return replace(start, count, "");
}

int String::split(const char *token, std::vector<String> &subStrings) const {
    MILO_ASSERT(subStrings.empty(), 0x1FD);

    int lastIndex = 0;
    int splitIndex = find_first_of(token, 0);

    while (splitIndex != -1U) {
        if (splitIndex > lastIndex) {
            String split = substr(lastIndex, splitIndex - lastIndex);
            subStrings.push_back(split);
        }
        lastIndex = splitIndex + 1;
        splitIndex = find_first_of(token, lastIndex);
    }

    if (lastIndex < length()) {
        String split = substr(lastIndex, length() - lastIndex);
        subStrings.push_back(split);
    }

    return subStrings.size();
}

String &String::operator=(const char *str) {
    if (str == mStr)
        return *this;
    if (str == 0 || *str == '\0') {
        resize(0);
    } else {
        reserve(strlen(str));
        unsigned char *d = (unsigned char *)mStr - 1;
        const unsigned char *s = (const unsigned char *)str - 1;
        unsigned int c;
        do {
            c = *++s;
            *++d = (unsigned char)c;
        } while (c != 0);
    }
    return *this;
}

String &String::operator=(Symbol s) { return *this = s.Str(); }

String String::operator+(const char *chrstr) const {
    String ret(*this);
    ret += chrstr;
    return ret;
}

String String::operator+(char c) const {
    String ret(*this);
    ret += c;
    return ret;
}

String String::operator+(const FixedString &fstr) const {
    String ret(*this);
    ret += fstr.c_str();
    return ret;
}

String String::substr(unsigned int pos) const {
    MILO_ASSERT(pos <= capacity(), 0x220);
    return String(mStr + pos);
}

String String::substr(unsigned int pos, unsigned int len) const {
    MILO_ASSERT(pos <= capacity(), 0x226);
    MILO_ASSERT(len > 0, 0x227);
    char buf[512];
    if (pos + len >= capacity()) {
        return String(mStr + pos);
    } else {
        MILO_ASSERT(len < 512, 0x22B);
        strncpy(buf, mStr + pos, len);
        buf[len] = '\0';
        return String(buf);
    }
}

String &String::insert(unsigned int pos, unsigned int count, char c) {
    MILO_ASSERT(pos <= capacity(), 0x27B);
    String tmp;
    tmp.reserve(length() + count);
    strncpy(tmp.mStr, mStr, pos);
    for (unsigned int i = 0; i < count; i++) {
        tmp.mStr[pos + i] = c;
    }
    CopyStrZ(tmp.mStr + pos + count, mStr + pos);
    swap(tmp);
    return *this;
}

String &String::insert(unsigned int pos, const char *str) { return replace(pos, 0, str); }

bool String::operator<(const String &str) const {
    return strcmp(mStr, str.mStr) < 0;
}

unsigned int String::find(char c, unsigned int pos) const {
    MILO_ASSERT(pos <= mCap, 0x6C);
    char *p = &mStr[pos];
    while ((*p != '\0') && (*p != c))
        p++;
    if (*p != '\0')
        return p - mStr;
    else
        return -1;
}

unsigned int String::find(char c) const { return find(c, 0); }

unsigned int String::find(const char *str, unsigned int pos) const {
    MILO_ASSERT(pos <= mCap, 0x83);
    char *found = strstr(&mStr[pos], str);
    if (found != 0)
        return found - mStr;
    else
        return -1;
}

unsigned int String::find_first_of(const char *str, unsigned int pos) const {
    char *p1;
    char *p2;
    if (str == 0)
        return -1;
    MILO_ASSERT(pos <= mCap, 0x8E);
    for (p1 = &mStr[pos]; *p1 != '\0'; p1++) {
        for (p2 = (char *)str; *p2 != '\0'; p2++) {
            if (*p1 == *p2)
                return p1 - mStr;
        }
    }
    return -1;
}

unsigned int String::find_last_of(char c) const {
    char *found = strrchr(mStr, c);
    if (found)
        return found - mStr;
    else
        return -1;
}

unsigned int String::find_last_of(const char *str) const {
    if (!str)
        return -1;
    int a = -1;
    for (char *tmp = (char *)str; *tmp != '\0'; tmp++) {
        int lastIdx = find_last_of(*tmp);
        if (lastIdx != -1 && lastIdx > a) {
            a = lastIdx;
        }
    }
    if (a == -1)
        return -1;
    else
        return a;
}

void String::ToLower() {
    char *p;
    for (p = mStr; *p != '\0'; p++) {
        *p = tolower(*p);
    }
}

void String::ToUpper() {
    char *p;
    for (p = mStr; *p != '\0'; p++) {
        *p = toupper(*p);
    }
}

void String::ReplaceAll(char old_char, char new_char) {
    char *p;
    for (p = mStr; *p != '\0'; p++) {
        if (*p == old_char)
            *p = new_char;
    }
}

bool String::contains(const char *str) const { return find(str, 0) != -1; }

int String::compare(unsigned int pos, unsigned int i2, const char *str) const {
    if (!str)
        return -1;
    else {
        MILO_ASSERT(pos <= mCap, 0xDD);
        return strncmp(mStr + pos, str, i2);
    }
}

char &String::operator[](unsigned int i) {
    MILO_ASSERT(i < mCap, 0xFC);
    return *(mStr + i);
}

char String::rindex(int i) const {
    MILO_ASSERT(i < 0 && uint(-i) <= mCap, 0xBC);
    return mStr[mCap + i];
}

char &String::rindex(int i) {
    MILO_ASSERT(i < 0 && uint(-i) <= mCap, 0xC2);
    return mStr[mCap + i];
}

unsigned int String::find(const char *cc) const { return find(cc, 0); }

void String::swap(String &s) {
    char *temp_text;
    unsigned int temp_len;

    temp_text = mStr;
    temp_len = mCap;
    mStr = s.mStr;
    mCap = s.mCap;
    s.mStr = temp_text;
    s.mCap = temp_len;
}
