// rb3-xenon native -- the file-source LEDGER (lane W16-UC).
//
// Every file the engine opens goes through one of two doors:
//
//   ARK   ArkFile (os/ArkFile.cpp, HX_NATIVE ctor): the name is looked up in
//         the mounted main_xbox.hdr index, and the bytes come out of a
//         main_xbox_N.ark part. This is the ONLY door retail uses for a read
//         of a relative path (retail FileIsLocal is "drive letter longer than
//         one character", so every plain path is "not local" -> ArkFile).
//   HOST  AsyncFileNative::_OpenAsync: a host fopen of an already-qualified
//         path (data dir, overlay dir, or an absolute path as given).
//
// With RB3_FILE_LEDGER=<path> set, each open appends one tab-separated line:
//
//   ARK   <ark key>            ok|miss
//   HOST  r|w <host path>      ok|fail   <why FileIsLocal said local>
//
// tools/native_file_audit.py reads it next to an strace of the same run: the
// strace is the authority on what the PROCESS touched (it sees opens no
// engine code made), and the ledger says which engine door each one came
// through, and which archive names were asked for. Unset, this costs one
// getenv per process and nothing per open.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace {
    std::mutex gLedgerLock;
    FILE *gLedger = nullptr;
    bool gLedgerInit = false;

    FILE *Ledger() {
        if (!gLedgerInit) {
            gLedgerInit = true;
            const char *path = getenv("RB3_FILE_LEDGER");
            if (path && *path) {
                gLedger = fopen(path, "a");
                if (gLedger) setvbuf(gLedger, nullptr, _IOLBF, 0);
            }
        }
        return gLedger;
    }
}

void NativeFileLedger(const char *fmt, ...) {
    std::lock_guard<std::mutex> lock(gLedgerLock);
    FILE *f = Ledger();
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
}
