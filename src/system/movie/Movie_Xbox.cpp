#ifndef HX_NATIVE
#include "movie/Movie_Xbox.h"

// Retail's body (0x82533618) is `li r3,1; blr`, ICF-folded with
// ObjDirPtr<ObjectDir>::IsDirPtr: the Xbox player never needs a file cached
// before opening it, so Begin always proceeds. It is defined outside Movie.cpp
// because retail's Begin calls it out of line and schedules the mPreloaded store
// before the call's argument moves, which only happens when the callee's body is
// not visible to Begin's TU (with it visible, even under auto_inline(off), MSVC
// sinks that store past the argument moves).
bool Movie::Impl::PlatformCacheFile(const char *) { return true; }
#endif
