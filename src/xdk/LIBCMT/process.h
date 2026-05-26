#pragma once
#include "../win_types.h"

#ifdef __cplusplus
extern "C" {
#endif

HANDLE _beginthreadex( // NATIVE CODE
    void *security,
    unsigned stack_size,
    unsigned (*start_address)(void *),
    void *arglist,
    unsigned initflag,
    unsigned *thrdaddr
);

#ifdef __cplusplus
}
#endif
