// xexp-apply: Standalone CLI that applies a .xexp delta patch to a base .xex.
//
// Wraps XenonRecomp's XexPatcher::apply (ported from xenia). One-shot use:
//   xexp-apply <base.xex> <patch.xexp> <out.xex>
//
// We treat the produced .xex as a build artifact and stash it under
// orig/<GAMEID>/ for jeff to consume.

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "xex_patcher.h"

static const char *kResultNames[] = {
    "Success",
    "FileOpenFailed",
    "FileWriteFailed",
    "XexFileUnsupported",
    "XexFileInvalid",
    "PatchFileInvalid",
    "PatchIncompatible",
    "PatchFailed",
    "PatchUnsupported",
};

int main(int argc, char **argv) {
    if (argc != 4) {
        std::fprintf(stderr,
                     "usage: %s <base.xex> <patch.xexp> <out.xex>\n",
                     argv[0]);
        return 2;
    }

    std::filesystem::path base = argv[1];
    std::filesystem::path patch = argv[2];
    std::filesystem::path out = argv[3];

    auto result = XexPatcher::apply(base, patch, out);
    auto idx = static_cast<std::size_t>(result);
    const char *name =
        idx < (sizeof(kResultNames) / sizeof(kResultNames[0]))
            ? kResultNames[idx]
            : "Unknown";

    if (result != XexPatcher::Result::Success) {
        std::fprintf(stderr, "xexp-apply: %s\n", name);
        return 1;
    }

    std::printf("xexp-apply: wrote %s\n", out.string().c_str());
    return 0;
}
