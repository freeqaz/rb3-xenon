/* =============================================================================
 * crt.c  --  Strategy B, Lane C : freestanding CRT for RB3Enhanced.dll (Xbox360/PPCBE)
 *
 * Single translation unit -> ONE object crt.obj providing every Lane-C symbol
 * that the from-source 51-obj link leaves unresolved (see UNRESOLVED-LEDGER.md):
 *
 *   PPC prolog/epilog thunks  __savegprlr_14..31 / __restgprlr_14..31
 *                             (byte-exact vs stock RB3Enhanced.dll @0x8401b820)
 *   _fltused                  (float-used marker, value 0)
 *   _DllMainCRTStartup        (XEX DLL entry -> forwards to RB3E DllMain, returns TRUE)
 *   libc subset               memset memcpy strncpy strchr strrchr strstr
 *                             sprintf sscanf atof atoi isspace isxdigit tolower
 *                             wcscat wcstombs malloc
 *
 * NOTE (intentional non-definitions): strlen/strcmp/strcpy/strcat/strncmp/free/
 * memmove are ALREADY resolved elsewhere in the link (not in the ledger's
 * category-C list) -- defining them here would collide, so they are omitted.
 *
 * Built with the Lane-K CFLAGS baseline (cl.exe X360 16.00.11886.00 under wibo),
 * machine 0x01F2. va_arg uses the toolchain's own <stdarg.h>, so caller-side
 * variadic passing and callee-side va_arg are two halves of the same cl.exe ABI
 * contract (self-consistent, incl. FP doubles homed to the GPR save area).
 * ===========================================================================*/

#include <stdarg.h>

typedef unsigned int   size_t_;   /* 32-bit addresses on Xbox360 */
typedef unsigned long  ulong_;
typedef unsigned short wchar16;   /* wchar_t is 16-bit under -Zc:wchar_t */

/* ---- category D: PPC register save/restore thunks + entry marker ----------*/
#include "_thunks.inc"

int _fltused = 0;

/* RB3E's DllMain (defined in source/xbox360.c). APIENTRY == __stdcall, which is
 * a no-op on PPC; a plain forward is ABI-correct. */
extern int DllMain(void *hInstDLL, unsigned long reason, void *lpReserved);

int _DllMainCRTStartup(void *hInstDLL, unsigned long reason, void *lpReserved)
{
    /* No C++ static-init / atexit machinery for a first boot. */
    return DllMain(hInstDLL, reason, lpReserved);
}

/* ---- category C: freestanding libc ---------------------------------------*/

/* memset/memcpy are compiler intrinsics; force real out-of-line definitions. */
#pragma function(memset, memcpy)

void *memset(void *dst, int c, size_t_ n)
{
    unsigned char *p = (unsigned char *)dst;
    unsigned char v = (unsigned char)c;
    while (n--) *p++ = v;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t_ n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* MSVC-PPC inline-memcpy helper: -Os lowers counted contiguous copies to a
 * 'bl _blkmov' with (dest=r3, src=r4, count=r5) -- confirmed from the call-site
 * register setup. Standard (void*,const void*,size_t) forward copy; a plain byte
 * loop does not self-lower, so this definition is safe and closes the dep. */
void _blkmov(void *dst, const void *src, size_t_ n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
}

char *strncpy(char *dst, const char *src, size_t_ n)
{
    char *d = dst;
    while (n && *src) { *d++ = *src++; n--; }
    while (n--) *d++ = '\0';
    return dst;
}

char *strchr(const char *s, int c)
{
    char ch = (char)c;
    for (;; s++) {
        if (*s == ch) return (char *)s;
        if (*s == '\0') return (char *)0;
    }
}

char *strrchr(const char *s, int c)
{
    char ch = (char)c;
    const char *last = (const char *)0;
    for (;; s++) {
        if (*s == ch) last = s;
        if (*s == '\0') return (char *)last;
    }
}

char *strstr(const char *hay, const char *needle)
{
    if (*needle == '\0') return (char *)hay;
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (*n == '\0') return (char *)hay;
    }
    return (char *)0;
}

int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

int isxdigit(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}

int atoi(const char *s)
{
    int sign = 1, v = 0;
    while (isspace((int)(unsigned char)*s)) s++;
    if (*s == '+') s++;
    else if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v * sign;
}

double atof(const char *s)
{
    double v = 0.0, frac = 0.0, scale = 1.0;
    int sign = 1, esign = 1, exp = 0, i;
    while (isspace((int)(unsigned char)*s)) s++;
    if (*s == '+') s++;
    else if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10.0 + (*s - '0'); s++; }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') { frac = frac * 10.0 + (*s - '0'); scale *= 10.0; s++; }
        v += frac / scale;
    }
    if (*s == 'e' || *s == 'E') {
        s++;
        if (*s == '+') s++;
        else if (*s == '-') { esign = -1; s++; }
        while (*s >= '0' && *s <= '9') { exp = exp * 10 + (*s - '0'); s++; }
        for (i = 0; i < exp; i++) { if (esign > 0) v *= 10.0; else v /= 10.0; }
    }
    return v * sign;
}

wchar16 *wcscat(wchar16 *dst, const wchar16 *src)
{
    wchar16 *d = dst;
    while (*d) d++;
    while (*src) *d++ = *src++;
    *d = 0;
    return dst;
}

/* wide -> multibyte: low-byte truncation is enough for the ASCII paths RB3E uses. */
size_t_ wcstombs(char *dst, const wchar16 *src, size_t_ n)
{
    size_t_ i = 0;
    if (dst == (char *)0) { while (src[i]) i++; return i; }
    for (; i < n; i++) {
        wchar16 w = src[i];
        dst[i] = (char)(w & 0xFF);
        if (w == 0) return i;
    }
    return i;
}

/* ---- malloc: static bump arena (free lives elsewhere; no reclaim) ---------
 * 8 MB is enough for RB3E's first-boot allocations (MiloSceneHooks.c, inih).
 * Note in findings: no free/realloc reclaim -- revisit only if a fault traces
 * back to exhaustion.                                                        */
#define CRT_ARENA_BYTES (8u * 1024u * 1024u)
static char  g_arena[CRT_ARENA_BYTES];
static size_t_ g_arena_off = 0;

void *malloc(size_t_ n)
{
    size_t_ off, aligned = (n + 15u) & ~15u;   /* 16-byte align */
    if (aligned == 0) aligned = 16;
    off = g_arena_off;
    if (off + aligned > CRT_ARENA_BYTES) return (void *)0;
    g_arena_off = off + aligned;
    return (void *)&g_arena[off];
}

/* ===========================================================================
 * Minimal printf/scanf engine (subset of C99 used by RB3E format strings):
 *   conversions d i u x X o p c s %   length hh h l ll   flags - + space 0 #
 *   width (incl. '*'), precision (incl. '*'), plus f/F for %.2f style.
 * ===========================================================================*/

static char *ull_to_str(unsigned long long v, int base, int upper, char *end)
{
    const char *digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char *p = end;
    *--p = '\0';
    if (v == 0) *--p = '0';
    while (v) { *--p = digs[v % base]; v /= base; }
    return p;
}

static int emit_ch(char *buf, int pos, char c)
{
    buf[pos] = c;
    return pos + 1;
}

static int emit_str(char *buf, int pos, const char *s, int len)
{
    int i;
    for (i = 0; i < len; i++) buf[pos + i] = s[i];
    return pos + len;
}

/* core: writes into buf (caller guarantees space, as with C sprintf). */
static int crt_vsprintf(char *buf, const char *fmt, va_list ap)
{
    int pos = 0;

    while (*fmt) {
        if (*fmt != '%') { pos = emit_ch(buf, pos, *fmt++); continue; }
        fmt++;  /* past '%' */

        {
            int flag_minus = 0, flag_zero = 0, flag_plus = 0, flag_space = 0, flag_hash = 0;
            int width = 0, prec = -1, lenmod = 0; /* lenmod: 1=h 2=hh 3=l 4=ll */
            char conv;

            /* flags */
            for (;;) {
                char f = *fmt;
                if (f == '-') flag_minus = 1;
                else if (f == '0') flag_zero = 1;
                else if (f == '+') flag_plus = 1;
                else if (f == ' ') flag_space = 1;
                else if (f == '#') flag_hash = 1;
                else break;
                fmt++;
            }
            /* width */
            if (*fmt == '*') { width = va_arg(ap, int); fmt++;
                               if (width < 0) { flag_minus = 1; width = -width; } }
            else while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt++ - '0'); }
            /* precision */
            if (*fmt == '.') {
                fmt++;
                prec = 0;
                if (*fmt == '*') { prec = va_arg(ap, int); fmt++; if (prec < 0) prec = -1; }
                else while (*fmt >= '0' && *fmt <= '9') { prec = prec * 10 + (*fmt++ - '0'); }
            }
            /* length modifiers */
            if (*fmt == 'h') { fmt++; if (*fmt == 'h') { lenmod = 2; fmt++; } else lenmod = 1; }
            else if (*fmt == 'l') { fmt++; if (*fmt == 'l') { lenmod = 4; fmt++; } else lenmod = 3; }
            else if (*fmt == 'L') { fmt++; lenmod = 4; }

            conv = *fmt ? *fmt++ : '\0';

            if (conv == '%') { pos = emit_ch(buf, pos, '%'); continue; }

            if (conv == 'c') {
                char c = (char)va_arg(ap, int);
                int pad = width - 1, i;
                if (!flag_minus) for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, ' ');
                pos = emit_ch(buf, pos, c);
                if (flag_minus) for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, ' ');
                continue;
            }

            if (conv == 's') {
                const char *s = va_arg(ap, const char *);
                int len = 0, pad, i;
                if (s == (const char *)0) s = "(null)";
                while (s[len] && (prec < 0 || len < prec)) len++;
                pad = width - len;
                if (!flag_minus) for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, ' ');
                pos = emit_str(buf, pos, s, len);
                if (flag_minus) for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, ' ');
                continue;
            }

            if (conv == 'd' || conv == 'i' || conv == 'u' ||
                conv == 'x' || conv == 'X' || conv == 'o' || conv == 'p') {
                char tmp[24];
                char *digits;
                int base = 10, upper = 0, is_signed = (conv == 'd' || conv == 'i');
                unsigned long long uv;
                long long sv = 0;
                int neg = 0, dlen, i, pad, zpad, numlen;
                char signch = 0;
                const char *prefix = "";

                if (conv == 'x') base = 16;
                else if (conv == 'X') { base = 16; upper = 1; }
                else if (conv == 'o') base = 8;
                else if (conv == 'p') { base = 16; }

                if (conv == 'p') {
                    uv = (unsigned long long)(ulong_)va_arg(ap, void *);
                } else if (is_signed) {
                    if (lenmod == 4) sv = va_arg(ap, long long);
                    else sv = (long long)va_arg(ap, int);
                    if (sv < 0) { neg = 1; uv = (unsigned long long)(-sv); }
                    else uv = (unsigned long long)sv;
                } else {
                    if (lenmod == 4) uv = va_arg(ap, unsigned long long);
                    else uv = (unsigned long long)(unsigned int)va_arg(ap, unsigned int);
                }

                digits = ull_to_str(uv, base, upper, tmp + sizeof(tmp));
                dlen = 0; while (digits[dlen]) dlen++;

                if (neg) signch = '-';
                else if (is_signed && flag_plus) signch = '+';
                else if (is_signed && flag_space) signch = ' ';

                if (flag_hash && base == 16 && uv != 0) prefix = upper ? "0X" : "0x";

                /* precision => min digits (zero-pad); disables 0-flag */
                numlen = dlen;
                if (prec >= 0 && numlen < prec) numlen = prec;
                {
                    int prefixlen = 0;
                    const char *pp = prefix;
                    int total;
                    while (*pp) { prefixlen++; pp++; }
                    total = numlen + (signch ? 1 : 0) + prefixlen;
                    pad = width - total;
                    zpad = numlen - dlen;

                    if (!flag_minus && flag_zero && prec < 0) {
                        /* zero-pad after sign/prefix */
                        if (signch) pos = emit_ch(buf, pos, signch);
                        pos = emit_str(buf, pos, prefix, prefixlen);
                        for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, '0');
                        for (i = 0; i < zpad; i++) pos = emit_ch(buf, pos, '0');
                        pos = emit_str(buf, pos, digits, dlen);
                    } else if (!flag_minus) {
                        for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, ' ');
                        if (signch) pos = emit_ch(buf, pos, signch);
                        pos = emit_str(buf, pos, prefix, prefixlen);
                        for (i = 0; i < zpad; i++) pos = emit_ch(buf, pos, '0');
                        pos = emit_str(buf, pos, digits, dlen);
                    } else {
                        if (signch) pos = emit_ch(buf, pos, signch);
                        pos = emit_str(buf, pos, prefix, prefixlen);
                        for (i = 0; i < zpad; i++) pos = emit_ch(buf, pos, '0');
                        pos = emit_str(buf, pos, digits, dlen);
                        for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, ' ');
                    }
                }
                continue;
            }

            if (conv == 'f' || conv == 'F') {
                /* 32-bit integer/fraction parts only: avoids the u64->double
                 * helper (__u64tod). p clamped so 'scale' stays inside 32 bits.
                 * u32<->double and double<->u32 casts lower inline (fcfid/fctiwz). */
                double d = va_arg(ap, double);
                int neg = 0, i, ip_len, pad, total;
                unsigned int ip, scale = 1, fs;
                double frac;
                int p = (prec < 0) ? 6 : prec;
                char tmp[24]; char *ipd; int fdig[20];

                if (p > 9) p = 9;
                if (d < 0) { neg = 1; d = -d; }
                for (i = 0; i < p; i++) scale *= 10;
                ip = (unsigned int)d;
                frac = d - (double)ip;
                fs = (unsigned int)(frac * (double)scale + 0.5);
                if (fs >= scale) { ip += 1; fs -= scale; }
                for (i = p - 1; i >= 0; i--) { fdig[i] = (int)(fs % 10); fs /= 10; }
                ipd = ull_to_str((unsigned long long)ip, 10, 0, tmp + sizeof(tmp));
                ip_len = 0; while (ipd[ip_len]) ip_len++;
                total = ip_len + (neg ? 1 : 0) + (p > 0 ? 1 + p : 0);
                pad = width - total;
                if (!flag_minus && flag_zero) {
                    if (neg) pos = emit_ch(buf, pos, '-');
                    for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, '0');
                } else if (!flag_minus) {
                    for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, ' ');
                    if (neg) pos = emit_ch(buf, pos, '-');
                } else {
                    if (neg) pos = emit_ch(buf, pos, '-');
                }
                pos = emit_str(buf, pos, ipd, ip_len);
                if (p > 0) {
                    pos = emit_ch(buf, pos, '.');
                    for (i = 0; i < p; i++) pos = emit_ch(buf, pos, (char)('0' + fdig[i]));
                }
                if (flag_minus) for (i = 0; i < pad; i++) pos = emit_ch(buf, pos, ' ');
                continue;
            }

            /* unknown conversion: emit literally */
            pos = emit_ch(buf, pos, '%');
            if (conv) pos = emit_ch(buf, pos, conv);
        }
    }

    buf[pos] = '\0';
    return pos;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = crt_vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

/* ---- sscanf subset: whitespace, literal match, %d %i %u %x %s %c %f, h/l --*/

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int sscanf(const char *str, const char *fmt, ...)
{
    va_list ap;
    int assigned = 0;
    const char *s = str;
    va_start(ap, fmt);

    while (*fmt) {
        if (isspace((int)(unsigned char)*fmt)) {
            fmt++;
            while (isspace((int)(unsigned char)*s)) s++;
            continue;
        }
        if (*fmt != '%') {
            if (*s != *fmt) break;
            s++; fmt++;
            continue;
        }
        /* conversion */
        fmt++;
        {
            int suppress = 0, lenmod = 0, width = 0, haswidth = 0;
            char conv;
            if (*fmt == '*') { suppress = 1; fmt++; }
            while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt++ - '0'); haswidth = 1; }
            if (*fmt == 'h') { fmt++; if (*fmt == 'h') { lenmod = 2; fmt++; } else lenmod = 1; }
            else if (*fmt == 'l') { fmt++; if (*fmt == 'l') { lenmod = 4; fmt++; } else lenmod = 3; }
            conv = *fmt ? *fmt++ : '\0';

            if (conv == '%') { if (*s == '%') s++; else break; continue; }

            if (conv == 'c') {
                char *out = suppress ? (char *)0 : va_arg(ap, char *);
                if (*s == '\0') break;
                if (out) *out = *s;
                s++;
                if (!suppress) assigned++;
                continue;
            }

            if (conv == 's') {
                char *out = suppress ? (char *)0 : va_arg(ap, char *);
                int cnt = 0;
                while (isspace((int)(unsigned char)*s)) s++;
                if (*s == '\0') break;
                while (*s && !isspace((int)(unsigned char)*s) && (!haswidth || cnt < width)) {
                    if (out) out[cnt] = *s;
                    s++; cnt++;
                }
                if (out) out[cnt] = '\0';
                if (!suppress) assigned++;
                continue;
            }

            /* numeric */
            {
                int base = 10, neg = 0, any = 0;
                unsigned long long uv = 0;
                const char *start;
                while (isspace((int)(unsigned char)*s)) s++;
                start = s;

                if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'x' ||
                    conv == 'X' || conv == 'o' || conv == 'f' || conv == 'F' ||
                    conv == 'e' || conv == 'g') {

                    if (conv == 'f' || conv == 'F' || conv == 'e' || conv == 'g') {
                        /* float: reuse atof on a bounded copy is overkill; parse inline */
                        char numbuf[64]; int k = 0;
                        if (*s == '+' || *s == '-') { if (k < 63) numbuf[k++] = *s; s++; }
                        while (*s >= '0' && *s <= '9') { if (k < 63) numbuf[k++] = *s; s++; any = 1; }
                        if (*s == '.') { if (k < 63) numbuf[k++] = *s; s++;
                            while (*s >= '0' && *s <= '9') { if (k < 63) numbuf[k++] = *s; s++; any = 1; } }
                        if (any && (*s == 'e' || *s == 'E')) { if (k < 63) numbuf[k++] = *s; s++;
                            if (*s == '+' || *s == '-') { if (k < 63) numbuf[k++] = *s; s++; }
                            while (*s >= '0' && *s <= '9') { if (k < 63) numbuf[k++] = *s; s++; } }
                        numbuf[k] = '\0';
                        if (!any) break;
                        if (!suppress) {
                            double dv = atof(numbuf);
                            if (lenmod == 3 || lenmod == 4) *va_arg(ap, double *) = dv;
                            else *va_arg(ap, float *) = (float)dv;
                            assigned++;
                        }
                        continue;
                    }

                    if (conv == 'x' || conv == 'X') base = 16;
                    else if (conv == 'o') base = 8;

                    if (*s == '+') s++;
                    else if (*s == '-') { neg = 1; s++; }

                    if (conv == 'i') {
                        if (*s == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
                        else if (*s == '0') base = 8;
                    } else if ((conv == 'x' || conv == 'X') &&
                               *s == '0' && (s[1] == 'x' || s[1] == 'X')) {
                        s += 2;
                    }

                    if (base == 16) {
                        int hv;
                        while ((hv = hexval((int)(unsigned char)*s)) >= 0) { uv = uv * 16 + hv; s++; any = 1; }
                    } else {
                        while (*s >= '0' && *s < ('0' + base)) { uv = uv * base + (*s - '0'); s++; any = 1; }
                    }
                    if (!any) { s = start; break; }

                    if (!suppress) {
                        unsigned long long outv = neg ? (unsigned long long)(-(long long)uv) : uv;
                        if (lenmod == 2)      *va_arg(ap, char *)  = (char)outv;
                        else if (lenmod == 1) *va_arg(ap, short *) = (short)outv;
                        else if (lenmod == 4) *va_arg(ap, long long *) = (long long)outv;
                        else                  *va_arg(ap, int *)   = (int)outv;
                        assigned++;
                    }
                    continue;
                }
                /* unknown conversion: stop */
                break;
            }
        }
    }

    va_end(ap);
    return assigned;
}
