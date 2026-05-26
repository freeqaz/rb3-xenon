// Forwarder to the real vectorintrinsics.h in xdk/LIBCMT
// This exists because we removed xdk/LIBCMT from the include path
// (it shadows system headers), but we still need vectorintrinsics.h
#pragma once
#include "../src/xdk/LIBCMT/vectorintrinsics.h"
