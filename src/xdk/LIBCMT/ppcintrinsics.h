#pragma once
#include "vectorintrinsics.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HX_NATIVE
unsigned long long __mftb();
#endif
double __fsel(double fComparand, double fValGE, double fValLT);
double __frsqrte(double);
void __dcbst(int, void *);

#ifdef __cplusplus
}
#endif
