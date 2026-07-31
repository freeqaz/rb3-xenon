// rb3-xenon native — mount the real .ark archive.
//
// The engine's ArkFile::Read (src/system/os/ArkFile.cpp, HX_NATIVE branch)
// deliberately delegates the raw byte fetch to this ONE function so the native
// port can bypass BlockMgr's overlapped/async disc pipeline. Everything else in
// the ark stack is real, unmodified engine code:
//
//   Archive::Read        — .hdr open + Rand2 stream decryption + v6 parse
//   ArkHash              — the string heap / hash table
//   Archive::GetFileInfo — the cumulative multi-ark offset walk (u64!)
//   ArkFile              — File subclass, Seek/Tell/Size/Eof
//
// ⚠ Unlike dc3-decomp's native port, we must NOT also define CDRead /
// CDReadExternal / CDReadDone here. rb3-xenon compiles the REAL
// src/system/os/CDReader.cpp natively (it is picked up by the ENGINE_OS glob in
// native/CMakeLists.txt and is not filtered out), so a second definition is a
// duplicate-symbol link error. dc3's CDReader_Native.cpp replaces a file that
// dc3 does not compile; ours would collide. NativeArkRead is the only genuinely
// missing symbol — it is weak-stubbed in native/src/dta_link_stubs.s.
//
// Format facts this relies on (verified against RB3 retail main_xbox.hdr):
//   * archive version 6
//   * every file entry is stored UNCOMPRESSED, so a straight byte copy is correct
//   * no file entry straddles an ark-part boundary, so one pread range suffices
// If a future archive violates either, this function is NOT sufficient on its
// own — Archive::GetFileInfo would still hand us a single (ark, offset) pair and
// the tail of the file would silently be wrong. See the boundary check below.

#include "os/Archive.h"
#include "os/Debug.h"
#include "os/File.h"
#include "utl/Str.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
    // Lazily opened, indexed by ark part number. Empty == not yet initialized.
    std::vector<int> gArkFds;
    bool gArkInitFailed = false;

    void CloseAll() {
        for (size_t i = 0; i < gArkFds.size(); i++) {
            if (gArkFds[i] >= 0) close(gArkFds[i]);
        }
        gArkFds.clear();
    }

    bool ArkFdsInit() {
        if (!gArkFds.empty()) return true;
        if (gArkInitFailed) return false;

        if (!TheArchive) {
            MILO_LOG("NativeArkRead: TheArchive is null — ArchiveInit()/"
                     "NativeArchiveInit() has not run\n");
            gArkInitFailed = true;
            return false;
        }

        int numArks = TheArchive->NumArkFiles();
        if (numArks <= 0) {
            MILO_LOG("NativeArkRead: archive reports %d ark files\n", numArks);
            gArkInitFailed = true;
            return false;
        }

        gArkFds.assign((size_t)numArks, -1);
        for (int i = 0; i < numArks; i++) {
            const char *arkFileName = TheArchive->GetArkfileName(i);
            String fullPath;
            // Route through the engine's path qualifier so the native data-dir
            // prefix (File_Native.cpp's gNativeDataDir) is applied exactly as it
            // is for every other file open.
            FileQualifiedFilename(fullPath, arkFileName);
            int fd = open(fullPath.c_str(), O_RDONLY);
            if (fd < 0) {
                MILO_LOG(
                    "NativeArkRead: failed to open ark part %d '%s': %s\n", i,
                    fullPath.c_str(), strerror(errno)
                );
                CloseAll();
                gArkInitFailed = true;
                return false;
            }
            gArkFds[i] = fd;
        }
        return true;
    }
}

// Read `bytes` bytes at `byteOffset` (an offset WITHIN ark part `arkFile` —
// Archive::GetFileInfo has already subtracted the cumulative size of the
// preceding parts). Returns false on any short read; the caller (ArkFile::Read)
// latches mFail, so a silent truncation cannot masquerade as success.
bool NativeArkRead(int arkFile, long long byteOffset, void *buffer, int bytes) {
    if (bytes < 0 || !buffer) return false;
    if (bytes == 0) return true;
    if (byteOffset < 0) return false;
    if (!ArkFdsInit()) return false;

    if (arkFile < 0 || arkFile >= (int)gArkFds.size()) {
        MILO_LOG("NativeArkRead: ark index %d out of range (%d parts)\n", arkFile,
                 (int)gArkFds.size());
        return false;
    }
    int fd = gArkFds[arkFile];
    if (fd < 0) return false;

    // pread, not lseek+read: no shared file-position state, so this stays correct
    // if the engine ever reads two ArkFiles in the same part concurrently.
    char *out = (char *)buffer;
    long long pos = byteOffset;
    long long remaining = bytes;
    while (remaining > 0) {
        ssize_t got = pread(fd, out, (size_t)remaining, (off_t)pos);
        if (got < 0) {
            if (errno == EINTR) continue;
            MILO_LOG("NativeArkRead: pread failed ark=%d off=%lld: %s\n", arkFile,
                     pos, strerror(errno));
            return false;
        }
        if (got == 0) {
            // EOF before satisfying the request. Either the entry straddles an
            // ark-part boundary (which this single-range read cannot serve) or
            // the ark file on disk is truncated. Both are real errors, not
            // something to paper over.
            MILO_LOG(
                "NativeArkRead: short read ark=%d off=%lld want=%d missing=%lld"
                " (entry may span an ark boundary, or the .ark is truncated)\n",
                arkFile, byteOffset, bytes, remaining
            );
            return false;
        }
        out += got;
        pos += got;
        remaining -= got;
    }
    return true;
}
