// thunk_stubs.cpp - Proper C++ method definitions for functions that need
// non-virtual thunks (Itanium ABI). The asm-label stubs in engine_stubs_generated.cpp
// provide the mangled symbol but don't generate the vtable thunks that GCC/Clang need
// for classes with multiple inheritance.
//
// All stubs have been moved to their respective decomp source files under
// #ifdef HX_NATIVE guards. This file is kept for any future thunk stubs that
// can't be placed in decomp source.

// DingoJob.cpp references this label-named string constant from .rodata
const char *lbl_82066608 = "";

// SkeletonChooser stubs — now in SkeletonChooser.cpp
