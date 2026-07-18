/* setjmp.h -- OSS X360 build shim for source/puff.c (raw DEFLATE inflate).
 * The reconstructed CRT lacks setjmp/longjmp; they are provided in crt/crt.c
 * as __declspec(naked) PPC64 functions. jmp_buf must be >= 176 bytes. */
#ifndef _OSS_SETJMP_H
#define _OSS_SETJMP_H
typedef unsigned long jmp_buf[64];   /* 4-byte long on X360 -> 256 bytes */
int  setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);
#endif /* _OSS_SETJMP_H */
