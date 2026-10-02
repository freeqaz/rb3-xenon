#include "StringConversion.h"
#include "types.h"

// The CRT and XDK headers are deliberately not included:
// - T2Char8 calls strncpy with only (out, in) -- retail loads no count
//   register before the call -- which the standard prototype cannot express.
// - swprintf below needs the variadic __va_start intrinsic; the shared
//   va_list_def.h (reached from stringapiset.h via wchar.h too) declares it
//   with a fixed parameter list.
typedef char *va_list;
extern "C" {
int MultiByteToWideChar(unsigned int, unsigned long, const char *, int, wchar_t *, int);
int WideCharToMultiByte(
    unsigned int, unsigned long, const wchar_t *, int, char *, int, const char *, int *
);
char *strcpy(char *, const char *);
unsigned int strlen(const char *);
char *strncpy(char *, const char *);
int sprintf(char *, const char *, ...);
int _vswprintf(wchar_t *, const wchar_t *, va_list);
void __va_start(va_list *, ...);
}

// The non-conforming two-argument swprintf. Retail emits it out of line at the
// end of this TU (0x82AE7338) and calls it from the wide BufferToHex.
inline int swprintf(wchar_t *_String, const wchar_t *_Format, ...) {
    va_list _Arglist;
    int _Ret;
    __va_start(&_Arglist, _Format);
    _Ret = _vswprintf(_String, _Format, _Arglist);
    _Arglist = (va_list)0;
    return _Ret;
}

namespace Quazal {
    // Array new/delete with an element-count header; defined in another TU.
    template <class T>
    T *qNewArray(unsigned int count, const char *file, int line);
    template <class T>
    void qDeleteArray(T *arr);
}

// Code pages: [0] = CP_ACP (the T / Char8 encoding), [1] = CP_UTF8.
unsigned int s_uiCodePages[2] = { 0, 65001 };

// This TU is built /Od. Its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES below determine the stack offsets;
// they were chosen to reproduce retail's frame layout.

namespace {
    void Latin1ToUtf8(const char *in, char *out, unsigned int len) {
        const unsigned char *src = (const unsigned char *)in;
        unsigned char *dst = (unsigned char *)out;
        len--;
        int c = *src;
        while (c != 0 && len > 0) {
            if (c >= 0x80) {
                *dst = ((c & 0x7C0) >> 6) | 0xC0;
                dst++;
                len--;
                *dst = (c & 0x3F) | 0x80;
                dst++;
                len--;
            } else {
                *dst = c;
                dst++;
                len--;
            }
            src++;
            c = *src;
        }
        *dst = 0;
    }

    void Utf8ToLatin1(const char *in, char *out, unsigned int len) {
        const unsigned char *src = (const unsigned char *)in;
        unsigned char *dst = (unsigned char *)out;
        len--;
        int c = *src;
        int count = 0;
        bool fail = false; // hit a character outside Latin-1
        while (c != 0 && len > 0 && !fail) {
            if (c >= 0 && c <= 0x7F) {
                *dst = c;
                dst++;
                len--;
            } else if (c >= 0xC0 && c <= 0xDF) {
                unsigned char c0 = *src;
                src++;
                unsigned char c1 = *src;
                *dst = ((c0 - 0xC0) << 6) + (c1 - 0x80);
                dst++;
                len--;
            } else {
                fail = true;
            }
            src++;
            c = *src;
            count++;
        }
        *dst = 0;
    }
}

namespace Quazal {
    namespace StringConversion {
        int GetUtf16ToUtf8BufferSize(const wchar_t *str) {
            int size = 1;
            const wchar_t *p;
            for (p = str; *p != 0; p++) {
                int c = *p;
                if (c >= 0 && c < 0x80) {
                    size++;
                } else if (c < 0x800) {
                    size += 2;
                } else if (c < 0x10000) {
                    size += 3;
                }
            }
            return size;
        }

        int Utf16ToUtf8(const wchar_t *in, char *out, unsigned int len) {
            int written = 0;
            len--;
            const wchar_t *cur = in;
            char *dst = out;
            bool stop = false;
            for (; *cur != 0 && len > 0 && !stop; cur++) {
                int c = *cur;
                if (c >= 0 && c < 0x80) {
                    *dst = c;
                    dst++;
                    len--;
                    written++;
                } else if (c < 0x800) {
                    *dst = (c >> 6) | 0xC0;
                    dst++;
                    len--;
                    *dst = (c & 0x3F) | 0x80;
                    dst++;
                    len--;
                    written += 2;
                } else if (c < 0x10000) {
                    *dst = (c >> 12) | 0xE0;
                    dst++;
                    len--;
                    *dst = ((c >> 6) & 0x3F) | 0x80;
                    dst++;
                    len--;
                    *dst = (c & 0x3F) | 0x80;
                    dst++;
                    len--;
                    written += 3;
                } else {
                    stop = true;
                }
            }
            *dst = 0;
            return written;
        }

        int GetUtf8ToUtf16BufferSize(const char *str) {
            int size = 1;
            const char *p;
            for (p = str; *p != 0; size++, p++) {
                int c = *p & 0xFF;
                if (c >= 0xC0 && c <= 0xDF) {
                    p++;
                } else if (c >= 0xE0 && c <= 0xEF) {
                    p += 2;
                }
            }
            return size;
        }

        int Utf8ToUtf16(const char *in, wchar_t *out, unsigned int len) {
            int numChars = 0;
            len--;
            const char *cur = in;
            wchar_t *dst = out;
            bool stop = false;
            for (; *cur != 0 && len > 0 && !stop; cur++) {
                int c = *cur & 0xFF;
                if (c >= 0 && c < 0x80) {
                    *dst = (char)c;
                    dst++;
                    len--;
                } else if (c >= 0xC0 && c <= 0xDF) {
                    wchar_t c0 = *cur & 0xFF;
                    cur++;
                    wchar_t c1 = *cur & 0xFF;
                    *dst = ((c0 - 0xC0) << 6) + (c1 - 0x80);
                    dst++;
                    len--;
                } else if (c >= 0xE0 && c <= 0xEF) {
                    wchar_t c0 = *cur & 0xFF;
                    cur++;
                    wchar_t c1 = *cur & 0xFF;
                    cur++;
                    wchar_t c2 = *cur & 0xFF;
                    *dst = ((c0 - 0xE0) << 12) + ((c1 - 0x80) << 6) + c2 - 0x80;
                    dst++;
                    len--;
                } else if (c >= 0xF0 && c <= 0xF7) {
                    stop = true; // 4-byte sequences are not supported
                }
                if (!stop) {
                    numChars++;
                }
            }
            *dst = 0;
            return numChars;
        }

        void Char8_2T(const char *in, char *out, unsigned int len) {
            strcpy(out, in); // BUG: should be strncpy(out, in, len);
        }

        void T2Char8(const char *in, char *out, unsigned int len) {
            strncpy(out, in); // no count is passed (see the declaration above)
        }

        void Char8_2T(const char *in, char **out) {
            *out = qNewArray<char>(strlen(in) + 1, __FILE__, 0x1AF);
            strcpy(*out, in);
        }

        void Utf8ToT(const char *in, char *out, unsigned int len) {
            Utf8ToLatin1(in, out, len);
        }

        void TToUtf8(const char *in, char *out, unsigned int len) {
            Latin1ToUtf8(in, out, len);
        }

        int GetTToUtf8BufferSize(const char *str) { return strlen(str) * 2 + 1; }

        void Char8ToWide(const char *in, wchar_t *out, unsigned int len) {
            MultiByteToWideChar(s_uiCodePages[0], 0, in, -1, out, len);
        }

        void WideToChar8(const wchar_t *in, char *out, unsigned int len) {
            WideCharToMultiByte(s_uiCodePages[0], 0, in, -1, out, len, 0, 0);
        }

        void Char8ToUtf8(const char *in, char **out) {
            int cchWide = MultiByteToWideChar(s_uiCodePages[0], 0, in, -1, 0, 0);
            wchar_t *wide = qNewArray<wchar_t>(cchWide + 1, __FILE__, 0x1E3);
            MultiByteToWideChar(s_uiCodePages[0], 0, in, -1, wide, cchWide);
            int cbUtf8 = WideCharToMultiByte(s_uiCodePages[1], 0, wide, -1, 0, 0, 0, 0);
            *out = qNewArray<char>(cbUtf8 + 1, __FILE__, 0x1EA);
            char *str = *out;
            WideCharToMultiByte(s_uiCodePages[1], 0, wide, -1, str, cbUtf8, 0, 0);
            qDeleteArray(wide);
        }

        void WideToUtf8(const wchar_t *in, char **out) {
            int cbUtf8 = WideCharToMultiByte(s_uiCodePages[1], 0, in, -1, 0, 0, 0, 0);
            *out = qNewArray<char>(cbUtf8 + 1, __FILE__, 0x200);
            char *str = *out;
            WideCharToMultiByte(s_uiCodePages[1], 0, in, -1, str, cbUtf8, 0, 0);
        }

        void Utf8ToWide(const char *in, wchar_t **out) {
            int cchWide = MultiByteToWideChar(s_uiCodePages[1], 0, in, -1, 0, 0);
            *out = qNewArray<wchar_t>(cchWide + 1, __FILE__, 0x20A);
            wchar_t *wstr = *out;
            MultiByteToWideChar(s_uiCodePages[1], 0, in, -1, wstr, cchWide);
        }

        void FreeWide(wchar_t *str) { qDeleteArray(str); }

        bool BufferToHexString(
            const unsigned char *buf, unsigned int len, char *out, unsigned int outSize
        ) {
            char *dst = out;
            if (outSize >= len * 2 + 1) {
                for (unsigned int i = 0; i < len; i++) {
                    unsigned char hi = buf[i] >> 4;
                    unsigned char lo = buf[i] & 0xF;
                    *dst = hi < 10 ? hi + '0' : hi - 10 + 'A';
                    dst++;
                    *dst = lo < 10 ? lo + '0' : lo - 10 + 'A';
                    dst++;
                }
                *dst = 0;
                return true;
            } else {
                return false;
            }
        }

        bool HexStringToBuffer(const char *str, unsigned char *buf, unsigned int bufSize) {
            const char *cur = str;
            unsigned int length = strlen(str);
            if (bufSize >= length / 2) {
                for (unsigned int i = 0; i < length / 2; i++) {
                    unsigned char hi = *cur <= '9' ? *cur - '0' : *cur - 'A' + 10;
                    cur++;
                    unsigned char lo = *cur <= '9' ? *cur - '0' : *cur - 'A' + 10;
                    cur++;
                    buf[i] = (hi << 4) | lo;
                }
                return true;
            } else {
                return false;
            }
        }

        unsigned char HexCharToNibble(char c) {
            switch (c) {
            case 'A':
                return 10;
            case 'B':
                return 11;
            case 'C':
                return 12;
            case 'D':
                return 13;
            case 'E':
                return 14;
            case 'F':
                return 15;
            default:
                return c - '0';
            }
        }

        void HexToBuffer(const char *str, unsigned char *buf, unsigned int len) {
            for (unsigned int i = 0; i < len; i++) {
                unsigned int j = i * 2;
                buf[i] = (HexCharToNibble(str[j]) << 4) | HexCharToNibble(str[j + 1]);
            }
        }

        int BufferToHex(const unsigned char *buf, char *out, unsigned int len) {
            int written = 0;
            for (unsigned int i = 0; i < len; i++) {
                written += sprintf(out + written, "%02X", buf[i]);
            }
            return written;
        }

        unsigned char HexCharToNibble(wchar_t c) {
            switch (c) {
            case L'A':
                return 10;
            case L'B':
                return 11;
            case L'C':
                return 12;
            case L'D':
                return 13;
            case L'E':
                return 14;
            case L'F':
                return 15;
            default:
                return c - L'0';
            }
        }

        void HexToBuffer(const wchar_t *str, unsigned char *buf, unsigned int len) {
            for (unsigned int i = 0; i < len; i++) {
                unsigned int j = i * 2;
                buf[i] = (HexCharToNibble(str[j]) << 4) | HexCharToNibble(str[j + 1]);
            }
        }

        int BufferToHex(const unsigned char *buf, wchar_t *out, unsigned int len) {
            int written = 0;
            for (unsigned int i = 0; i < len; i++) {
                written += swprintf(out + written, L"%02X", buf[i]);
            }
            return written;
        }
    }
}
