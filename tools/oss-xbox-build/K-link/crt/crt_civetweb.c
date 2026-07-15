/* crt_civetweb.c  --  CRT additions the vendored civetweb TU pulls in beyond the
 * base OSS crt.c. Kept out of crt.c (which the 51 game TUs share) per task note.
 * Compiled -TC with the same recipe as crt.c; linked as crt/crt_civetweb.obj.
 *
 * Provides: bounded printf (_vsnprintf/vsnprintf/snprintf), number parsers
 * (strtol/strtoul/strtod/_strtoi64/_strtoui64), ctype, mem/str helpers, minimal
 * time (time/gmtime/strftime for HTTP Date), stdio + stat stubs (dead under
 * NO_FILESYSTEMS), a heap-consistent calloc/realloc/free over crt.c's malloc,
 * and the __u64tod compiler helper. See 04-phase0-findings.md Class C/D.
 */
#include <stddef.h>
#include <stdarg.h>
#include <time.h>     /* LIBCMT: time_t (long long), struct tm, decls */

/* crt.c leaf helpers (same link unit set) */
extern void *malloc(size_t n);
extern void *memcpy(void *dst, const void *src, size_t n);
extern void *memset(void *dst, int c, size_t n);
extern size_t strlen(const char *s);

/* kernel time primitive (xboxkrnl.def @132) */
typedef union { struct { unsigned int LowPart; long HighPart; } u; long long QuadPart; } CV_LI;
void KeQuerySystemTime(CV_LI *t);

/* ============================ ctype ============================ */
int isdigit(int c)  { return (c >= '0' && c <= '9'); }
int isalpha(int c)  { return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')); }
int isalnum(int c)  { return isalpha(c) || isdigit(c); }
int iscntrl(int c)  { return (c >= 0 && c < 0x20) || c == 0x7f; }
int isprint(int c)  { return c >= 0x20 && c < 0x7f; }
int isgraph(int c)  { return c > 0x20 && c < 0x7f; }
int toupper(int c)  { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

/* ============================ mem / str ============================ */
void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;
    while (n--) { if (*p == (unsigned char)c) return (void *)p; p++; }
    return 0;
}
void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dst;
    if (d < s) { while (n--) *d++ = *s++; }
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}
int strncmp(const char *a, const char *b, size_t n)
{
    while (n--) {
        unsigned char ca = (unsigned char)*a++, cb = (unsigned char)*b++;
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == 0) break;
    }
    return 0;
}
size_t strcspn(const char *s, const char *reject)
{
    const char *p, *r;
    for (p = s; *p; p++) { for (r = reject; *r; r++) if (*p == *r) return (size_t)(p - s); }
    return (size_t)(p - s);
}
char *strerror(int errnum) { (void)errnum; return (char *)"error"; }

/* ============================ errno ============================ */
static int g_cv_errno;
int *_errno(void) { return &g_cv_errno; }

/* ============================ number parsers ============================ */
static int cv_digit(int c, int base)
{
    int v;
    if (c >= '0' && c <= '9') v = c - '0';
    else if (c >= 'a' && c <= 'z') v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z') v = c - 'A' + 10;
    else return -1;
    return (v < base) ? v : -1;
}
static unsigned long long cv_strtoull(const char *s, char **end, int base, int *neg)
{
    unsigned long long acc = 0;
    const char *nptr = s;   /* original arg: C strtol sets endptr==nptr on no conversion */
    const char *start;
    int d;
    *neg = 0;
    while (*s == ' ' || (*s >= 9 && *s <= 13)) s++;
    if (*s == '+') s++;
    else if (*s == '-') { *neg = 1; s++; }
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; base = 16; }
    else if (base == 0 && s[0] == '0') base = 8;
    else if (base == 0) base = 10;
    start = s;
    while ((d = cv_digit((unsigned char)*s, base)) >= 0) { acc = acc * (unsigned)base + (unsigned)d; s++; }
    /* No digits consumed => point endptr back at the original nptr (not past the
     * skipped whitespace/sign), matching C semantics. civetweb's Content-Length
     * validator relies on endptr==header to reject malformed values (e.g. "+"). */
    if (end) *end = (char *)((s == start) ? nptr : s);
    return acc;
}
long strtol(const char *s, char **end, int base)
{
    int neg; unsigned long long v = cv_strtoull(s, end, base, &neg);
    return neg ? -(long)v : (long)v;
}
unsigned long strtoul(const char *s, char **end, int base)
{
    int neg; unsigned long long v = cv_strtoull(s, end, base, &neg);
    return neg ? (unsigned long)(-(long long)v) : (unsigned long)v;
}
long long _strtoi64(const char *s, char **end, int base)
{
    int neg; unsigned long long v = cv_strtoull(s, end, base, &neg);
    return neg ? -(long long)v : (long long)v;
}
unsigned long long _strtoui64(const char *s, char **end, int base)
{
    int neg; unsigned long long v = cv_strtoull(s, end, base, &neg);
    (void)neg; return v;
}
double strtod(const char *s, char **end)
{
    double val = 0.0, sign = 1.0, frac = 0.1;
    const char *start = s;
    while (*s == ' ' || (*s >= 9 && *s <= 13)) s++;
    if (*s == '+') s++; else if (*s == '-') { sign = -1.0; s++; }
    while (*s >= '0' && *s <= '9') { val = val * 10.0 + (*s - '0'); s++; }
    if (*s == '.') { s++; while (*s >= '0' && *s <= '9') { val += (*s - '0') * frac; frac *= 0.1; s++; } }
    if (*s == 'e' || *s == 'E') {
        int esign = 1, e = 0; s++;
        if (*s == '+') s++; else if (*s == '-') { esign = -1; s++; }
        while (*s >= '0' && *s <= '9') { e = e * 10 + (*s - '0'); s++; }
        while (e-- > 0) { if (esign > 0) val *= 10.0; else val *= 0.1; }
    }
    if (end) *end = (char *)((s == start) ? start : s);
    return sign * val;
}

/* u64 -> double compiler helper (cl emits calls to it; avoid re-triggering it by
 * using only signed i64->double conversions). */
double __u64tod(unsigned long long v)
{
    if ((long long)v >= 0) return (double)(long long)v;
    return (double)(long long)(v >> 1) * 2.0 + (double)(long long)(v & 1);
}

/* ============================ heap (consistent w/ crt.c malloc) ============ */
/* crt.c malloc is a bump arena with no reclamation; keep free/realloc/calloc on
 * the SAME arena so raw civetweb malloc<->free stay paired (the mg_* wrappers use
 * the game heap separately). */
void  free(void *p) { (void)p; }
void *calloc(size_t count, size_t size)
{
    size_t n = count * size;
    void *p = malloc(n);
    if (p) memset(p, 0, n);
    return p;
}
void *realloc(void *old, size_t n)
{
    void *p;
    if (n == 0) { free(old); return 0; }
    p = malloc(n);
    if (p && old) memcpy(p, old, n);   /* over-read bounded by the arena buffer */
    return p;
}

/* ============================ time (minimal, GMT) ============================ */
#define CV_EPOCH_1601_TO_1970 11644473600LL
time_t time(time_t *t)
{
    CV_LI now; long long secs;
    KeQuerySystemTime(&now);
    secs = now.QuadPart / 10000000LL - CV_EPOCH_1601_TO_1970;
    if (t) *t = secs;
    return secs;
}
double difftime(time_t a, time_t b) { return (double)(a - b); }

static const int cv_mdays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
struct tm *gmtime(const time_t *timer)
{
    static struct tm tmv;
    long long days, secs, y;
    int mon;
    if (!timer) return 0;
    secs = *timer;
    days = secs / 86400; secs %= 86400;
    if (secs < 0) { secs += 86400; days--; }
    tmv.tm_hour = (int)(secs / 3600);
    tmv.tm_min  = (int)((secs % 3600) / 60);
    tmv.tm_sec  = (int)(secs % 60);
    tmv.tm_wday = (int)(((days % 7) + 4 + 7) % 7);   /* 1970-01-01 = Thursday */
    y = 1970;
    for (;;) {
        int leap = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
        long long ylen = leap ? 366 : 365;
        if (days >= ylen) { days -= ylen; y++; } else break;
    }
    tmv.tm_year = (int)(y - 1900);
    tmv.tm_yday = (int)days;
    for (mon = 0; mon < 12; mon++) {
        int leap = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
        int dm = cv_mdays[mon] + ((mon == 1 && leap) ? 1 : 0);
        if (days >= dm) days -= dm; else break;
    }
    tmv.tm_mon = mon;
    tmv.tm_mday = (int)days + 1;
    tmv.tm_isdst = 0;
    return &tmv;
}
struct tm *localtime(const time_t *timer) { return gmtime(timer); }
size_t strftime(char *out, size_t max, const char *fmt, const struct tm *tm)
{
    static const char *wday[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
    static const char *mon[]  = { "Jan","Feb","Mar","Apr","May","Jun",
                                  "Jul","Aug","Sep","Oct","Nov","Dec" };
    size_t o = 0;
    char nb[8];
    while (*fmt && o + 1 < max) {
        if (*fmt != '%') { out[o++] = *fmt++; continue; }
        fmt++;
        {
            const char *s = 0; int num = -1, w = 0, i, l;
            switch (*fmt) {
                case 'a': s = (tm->tm_wday >= 0 && tm->tm_wday < 7) ? wday[tm->tm_wday] : "?"; break;
                case 'b': case 'h': s = (tm->tm_mon >= 0 && tm->tm_mon < 12) ? mon[tm->tm_mon] : "?"; break;
                case 'd': num = tm->tm_mday; w = 2; break;
                case 'H': num = tm->tm_hour; w = 2; break;
                case 'M': num = tm->tm_min;  w = 2; break;
                case 'S': num = tm->tm_sec;  w = 2; break;
                case 'Y': num = tm->tm_year + 1900; w = 4; break;
                case 'y': num = (tm->tm_year + 1900) % 100; w = 2; break;
                case 'm': num = tm->tm_mon + 1; w = 2; break;
                case '%': out[o++] = '%'; fmt++; continue;
                default:  if (o + 1 < max) out[o++] = *fmt; if (*fmt) fmt++; continue;
            }
            fmt++;
            if (num >= 0) {
                int v = num, d = 0; char tmp[8];
                if (v == 0) tmp[d++] = '0';
                while (v > 0 && d < 7) { tmp[d++] = (char)('0' + v % 10); v /= 10; }
                while (d < w) tmp[d++] = '0';
                for (i = 0; i < d; i++) nb[i] = tmp[d - 1 - i];
                nb[d] = 0; s = nb;
            }
            l = (int)strlen(s);
            for (i = 0; i < l && o + 1 < max; i++) out[o++] = s[i];
        }
    }
    if (max) out[o] = 0;
    return o;
}
time_t mktime(struct tm *tm) { (void)tm; return 0; }

/* ============================ stdio / stat stubs (dead: NO_FILESYSTEMS) ==== */
void *fopen(const char *p, const char *m) { (void)p; (void)m; return 0; }
int   fclose(void *f) { (void)f; return 0; }
int   ferror(void *f) { (void)f; return 0; }
char *fgets(char *b, int n, void *f) { (void)b; (void)n; (void)f; return 0; }
size_t fwrite(const void *b, size_t s, size_t n, void *f) { (void)b; (void)s; (void)n; (void)f; return 0; }
int   fprintf(void *f, const char *fmt, ...) { (void)f; (void)fmt; return 0; }
int   _fileno(void *f) { (void)f; return -1; }
int   _read(int fd, void *b, unsigned n) { (void)fd; (void)b; (void)n; return -1; }
int   _write(int fd, const void *b, unsigned n) { (void)fd; (void)b; (void)n; return -1; }
int   _close(int fd) { (void)fd; return -1; }
long long _lseeki64(int fd, long long off, int origin) { (void)fd; (void)off; (void)origin; return -1; }
int   stat(const char *p, void *buf) { (void)p; (void)buf; return -1; }

/* wide-char FS stubs referenced before civetweb's NO_FILESYSTEMS guard (Class E) */
unsigned long GetLongPathNameW(const unsigned short *a, unsigned short *b, unsigned long c) { (void)a; (void)b; (void)c; return 0; }
int  DeleteFileW(const unsigned short *a) { (void)a; return 0; }
int  CreateDirectoryW(const unsigned short *a, void *b) { (void)a; (void)b; return 0; }
unsigned long GetFileAttributesW(const unsigned short *a) { (void)a; return (unsigned long)-1; }
void *FindFirstFileW(const unsigned short *a, void *b) { (void)a; (void)b; return (void *)-1; }
int  FindNextFileW(void *a, void *b) { (void)a; (void)b; return 0; }

/* ============================ bounded printf ============================ */
struct cv_out { char *buf; size_t cap; size_t len; };
static void cv_put(struct cv_out *o, char c) { if (o->len + 1 < o->cap) o->buf[o->len] = c; o->len++; }
static void cv_puts(struct cv_out *o, const char *s, int n) { int i; for (i = 0; i < n; i++) cv_put(o, s[i]); }

static int cv_vformat(char *buf, size_t cap, const char *fmt, va_list ap)
{
    struct cv_out o; o.buf = buf; o.cap = cap; o.len = 0;
    while (*fmt) {
        if (*fmt != '%') { cv_put(&o, *fmt++); continue; }
        fmt++;
        {
            int fminus = 0, fzero = 0, fplus = 0, fspace = 0, fhash = 0;
            int width = 0, prec = -1, lenmod = 0; char conv;
            for (;;) { char f = *fmt;
                if (f == '-') fminus = 1; else if (f == '0') fzero = 1;
                else if (f == '+') fplus = 1; else if (f == ' ') fspace = 1;
                else if (f == '#') fhash = 1; else break; fmt++; }
            if (*fmt == '*') { width = va_arg(ap, int); fmt++; if (width < 0) { fminus = 1; width = -width; } }
            else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
            if (*fmt == '.') { fmt++; prec = 0;
                if (*fmt == '*') { prec = va_arg(ap, int); fmt++; if (prec < 0) prec = -1; }
                else while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0'); }
            if (*fmt == 'h') { fmt++; if (*fmt == 'h') { lenmod = 2; fmt++; } else lenmod = 1; }
            else if (*fmt == 'l') { fmt++; if (*fmt == 'l') { lenmod = 4; fmt++; } else lenmod = 3; }
            else if (*fmt == 'z' || *fmt == 'j' || *fmt == 't') { fmt++; lenmod = 3; }
            else if (*fmt == 'L') { fmt++; lenmod = 4; }
            conv = *fmt ? *fmt++ : 0;

            if (conv == '%') { cv_put(&o, '%'); continue; }
            if (conv == 'c') { char c = (char)va_arg(ap, int);
                int pad = width - 1, i;
                if (!fminus) for (i = 0; i < pad; i++) cv_put(&o, ' ');
                cv_put(&o, c);
                if (fminus) for (i = 0; i < pad; i++) cv_put(&o, ' '); continue; }
            if (conv == 's') { const char *s = va_arg(ap, const char *);
                int len = 0, pad, i; if (!s) s = "(null)";
                while (s[len] && (prec < 0 || len < prec)) len++;
                pad = width - len;
                if (!fminus) for (i = 0; i < pad; i++) cv_put(&o, ' ');
                cv_puts(&o, s, len);
                if (fminus) for (i = 0; i < pad; i++) cv_put(&o, ' '); continue; }
            if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'x' ||
                conv == 'X' || conv == 'o' || conv == 'p') {
                char tmp[24]; const char *digs;
                int base = 10, upper = 0, is_signed = (conv == 'd' || conv == 'i');
                unsigned long long uv; long long sv; int neg = 0, dlen = 0, i, pad, zpad, numlen, prefixlen = 0;
                char signch = 0; const char *prefix = ""; char *dp;
                if (conv == 'x') base = 16; else if (conv == 'X') { base = 16; upper = 1; }
                else if (conv == 'o') base = 8; else if (conv == 'p') base = 16;
                if (conv == 'p') uv = (unsigned long long)(unsigned long)va_arg(ap, void *);
                else if (is_signed) { sv = (lenmod == 4) ? va_arg(ap, long long) : (long long)va_arg(ap, int);
                    if (sv < 0) { neg = 1; uv = (unsigned long long)(-sv); } else uv = (unsigned long long)sv; }
                else uv = (lenmod == 4) ? va_arg(ap, unsigned long long)
                                        : (unsigned long long)va_arg(ap, unsigned int);
                digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
                dp = tmp + sizeof(tmp); *--dp = 0;
                if (uv == 0) *--dp = '0';
                while (uv) { *--dp = digs[uv % (unsigned)base]; uv /= (unsigned)base; }
                dlen = 0; while (dp[dlen]) dlen++;
                if (neg) signch = '-'; else if (is_signed && fplus) signch = '+';
                else if (is_signed && fspace) signch = ' ';
                if (fhash && base == 16 && dp[0] != '0') { prefix = upper ? "0X" : "0x"; prefixlen = 2; }
                numlen = dlen; if (prec >= 0 && numlen < prec) numlen = prec;
                zpad = numlen - dlen;
                pad = width - (numlen + (signch ? 1 : 0) + prefixlen);
                if (!fminus && fzero && prec < 0) {
                    if (signch) cv_put(&o, signch); cv_puts(&o, prefix, prefixlen);
                    for (i = 0; i < pad; i++) cv_put(&o, '0');
                    for (i = 0; i < zpad; i++) cv_put(&o, '0'); cv_puts(&o, dp, dlen);
                } else if (!fminus) {
                    for (i = 0; i < pad; i++) cv_put(&o, ' ');
                    if (signch) cv_put(&o, signch); cv_puts(&o, prefix, prefixlen);
                    for (i = 0; i < zpad; i++) cv_put(&o, '0'); cv_puts(&o, dp, dlen);
                } else {
                    if (signch) cv_put(&o, signch); cv_puts(&o, prefix, prefixlen);
                    for (i = 0; i < zpad; i++) cv_put(&o, '0'); cv_puts(&o, dp, dlen);
                    for (i = 0; i < pad; i++) cv_put(&o, ' ');
                }
                continue;
            }
            if (conv == 'f' || conv == 'F' || conv == 'g' || conv == 'G' || conv == 'e') {
                double d = va_arg(ap, double); int neg = 0, i, p = (prec < 0) ? 6 : prec;
                unsigned int ip, scale = 1, fs; double frac; char tmp[24]; char *ipd; int fdig[10], ipl;
                if (p > 9) p = 9;
                if (d < 0) { neg = 1; d = -d; }
                for (i = 0; i < p; i++) scale *= 10;
                ip = (unsigned int)d; frac = d - (double)ip;
                fs = (unsigned int)(frac * (double)scale + 0.5);
                if (fs >= scale) { ip++; fs -= scale; }
                for (i = p - 1; i >= 0; i--) { fdig[i] = (int)(fs % 10); fs /= 10; }
                ipd = tmp + sizeof(tmp); *--ipd = 0;
                if (ip == 0) *--ipd = '0'; while (ip) { *--ipd = (char)('0' + ip % 10); ip /= 10; }
                ipl = 0; while (ipd[ipl]) ipl++;
                if (neg) cv_put(&o, '-'); cv_puts(&o, ipd, ipl);
                if (p > 0) { cv_put(&o, '.'); for (i = 0; i < p; i++) cv_put(&o, (char)('0' + fdig[i])); }
                continue;
            }
            cv_put(&o, '%'); if (conv) cv_put(&o, conv);
        }
    }
    if (o.cap) o.buf[(o.len < o.cap) ? o.len : o.cap - 1] = 0;
    return (int)o.len;
}
int _vsnprintf(char *buf, size_t count, const char *fmt, va_list ap) { return cv_vformat(buf, count, fmt, ap); }
int vsnprintf(char *buf, size_t count, const char *fmt, va_list ap) { return cv_vformat(buf, count, fmt, ap); }
int snprintf(char *buf, size_t count, const char *fmt, ...)
{ va_list ap; int n; va_start(ap, fmt); n = cv_vformat(buf, count, fmt, ap); va_end(ap); return n; }
int _snprintf(char *buf, size_t count, const char *fmt, ...)
{ va_list ap; int n; va_start(ap, fmt); n = cv_vformat(buf, count, fmt, ap); va_end(ap); return n; }
