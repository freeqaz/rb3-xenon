/* freestanding stdint.h for the X360 MSVC (cl.exe) XDK-free OSS build (Lane K).
   LIBCMT/stdint.h is a 0-byte stub; RB3E headers need uint32_t etc.
   Shadowed ahead of LIBCMT via -I order. */
#ifndef _K_OSS_STDINT_H
#define _K_OSS_STDINT_H
typedef signed char        int8_t;   typedef unsigned char      uint8_t;
typedef short              int16_t;  typedef unsigned short     uint16_t;
typedef int                int32_t;  typedef unsigned int       uint32_t;
typedef __int64            int64_t;  typedef unsigned __int64   uint64_t;
typedef unsigned int       uintptr_t; typedef int               intptr_t;
typedef signed char        int_least8_t;  typedef unsigned char  uint_least8_t;
typedef short              int_least16_t; typedef unsigned short uint_least16_t;
typedef int                int_least32_t; typedef unsigned int   uint_least32_t;
typedef __int64            int_least64_t; typedef unsigned __int64 uint_least64_t;
#endif
