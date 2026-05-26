#include "wchar.h"
#include "errno.h"
#include "cstdarg"

extern "C" {

typedef int (*woutput_func_t)(wchar_t *, size_t, size_t, const wchar_t *, void *, va_list);

extern int _vswprintf_helper(woutput_func_t, wchar_t *, size_t, size_t, const wchar_t *, void *, va_list);
extern int _woutput_s_l(wchar_t *, size_t, size_t, const wchar_t *, void *, va_list);
extern void _invalid_parameter_noinfo(void);

int _vswprintf_s_l(wchar_t *buffer, size_t sizeInWords, size_t count, const wchar_t *format, void *locale, va_list arglist) {
    int result;

    if (count == 0 || buffer == 0 || sizeInWords == 0) {
        *_errno() = 0x16;
        _invalid_parameter_noinfo();
        return -1;
    }

    result = _vswprintf_helper(_woutput_s_l, buffer, sizeInWords, count, format, locale, arglist);

    if (result < 0) {
        *buffer = 0;
    }

    if (result != -2) {
        return result;
    }

    *_errno() = 0x22;
    _invalid_parameter_noinfo();
    return -1;
}

}
