// Native only (lane A6, 2026-11-22): force-included (native/CMakeLists.txt) into
// TUs that use MILO_ASSERT as an EXPRESSION, e.g. the handler condition
//     HANDLE_ACTION_IF(set_leaderboard_mode, (MILO_ASSERT(mProvider, 0xD9), true), ...)
// in CampaignGoalsLeaderboardPanel.cpp / CampaignCareerLeaderboardPanel.cpp.
//
// The X360 arm of os/Debug.h is already an expression, ((void)(cond)), so MSVC
// accepts the comma form. The HX_NATIVE arm is a do { } while (0) STATEMENT, so
// clang stops with "expected expression". This header includes os/Debug.h first
// (it is #pragma once, so the TU's own include of it is then a no-op) and
// replaces the native MILO_ASSERT with an expression of the same behaviour: the
// same condition test and the same TheDebugFailer << MakeString(kAssertStr,
// __FILE__, line, #cond) failure path. As a statement (MILO_ASSERT(x, n);) it
// behaves exactly as the do/while form. The X360 compile never sees this file.
#pragma once
#include "os/Debug.h"
#include "utl/MakeString.h"

#ifdef HX_NATIVE
#undef MILO_ASSERT
#define MILO_ASSERT(cond, line)                                                          \
    ((cond) ? (void)0                                                                    \
            : (void)(TheDebugFailer << MakeString(kAssertStr, __FILE__, line, #cond)))
#endif
