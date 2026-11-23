#pragma once

// Clang uses verbatim names from explicit asm labels on both object formats.
#ifdef __APPLE__
#define RB3_ASM_LABEL(name) __asm__("_" name)
#else
#define RB3_ASM_LABEL(name) __asm__(name)
#endif
