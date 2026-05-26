#include <stdarg.h>
#include <stdio.h>
#include "stddef.h"
#include "errno.h"

extern "C" {

typedef unsigned int uintptr_t;
typedef int (*output_fn_t)(void);

int _vsnprintf_helper(output_fn_t output_fn, char *buffer, size_t sizeInBytes, const char *format, void *locale, va_list argptr);
void _invalid_parameter_noinfo(void);

extern "C" int _output_s_l(void);

#define EINVAL 22
#define ERANGE 34

int _vsprintf_s_l(char *buffer, size_t sizeInBytes, const char *format, void *locale, va_list argptr) {
    int result;
    int *err_ptr;
    int err_val;

    if (format == NULL || buffer == NULL || sizeInBytes == 0) {
        err_ptr = _errno();
        err_val = EINVAL;
    } else {
        result = _vsnprintf_helper(_output_s_l, buffer, sizeInBytes, format, locale, argptr);

        if (result < 0) {
            buffer[0] = '\0';
        }

        if (result != -2) {
            return result;
        }

        err_ptr = _errno();
        err_val = ERANGE;
    }

    *err_ptr = err_val;
    _invalid_parameter_noinfo();
    return -1;
}

int vsprintf_s(char *buffer, size_t sizeInBytes, const char *format, va_list argptr) {
    return _vsprintf_s_l(buffer, sizeInBytes, format, 0, argptr);
}

}
