// DC3 Native Port - File I/O Implementation
// Replaces File_Win.cpp - POSIX file operations

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "os/Archive.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"

// Configurable data directory for native port (where gen/, config/ etc. live)
static char gNativeDataDir[512] = ".";

// Optional overlay directory — files here shadow the archive/data dir.
// Used for native-only DTA patches (e.g. adding settings UI toggles).
static char gNativeOverlayDir[512] = "";

void NativeSetDataDir(const char *dir) {
    strncpy(gNativeDataDir, dir, sizeof(gNativeDataDir) - 1);
    gNativeDataDir[sizeof(gNativeDataDir) - 1] = '\0';
}

const char *NativeGetDataDir() { return gNativeDataDir; }

void NativeSetOverlayDir(const char *dir) {
    strncpy(gNativeOverlayDir, dir, sizeof(gNativeOverlayDir) - 1);
    gNativeOverlayDir[sizeof(gNativeOverlayDir) - 1] = '\0';
}

const char *NativeGetOverlayDir() { return gNativeOverlayDir; }

// Check if a file exists in the overlay directory
static bool NativeOverlayExists(const char *file) {
    if (!gNativeOverlayDir[0] || !file || !*file) return false;
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/%s", gNativeOverlayDir, file);
    struct stat st;
    return stat(buf, &st) == 0;
}

// Is `file` a LOOSE file -- present on the filesystem but absent from the
// mounted archive index? That is how real DLC ships (loose files inside an
// STFS container, never in the .ark), and how mods and hand-authored assets
// arrive too.
//
// ★ ORDER IS LOAD-BEARING AND IS THE WHOLE SAFETY ARGUMENT: the archive is
// consulted FIRST and WINS. So every path the ark contains keeps resolving
// through ArkFile and reading exactly the bytes it read before -- this
// function can only ever return true for a path the ark does NOT have, and
// for such a path the pre-existing behaviour is a GUARANTEED HARD FAILURE
// (NewFile -> ArkFile -> Fail() -> delete -> null). There is no case where
// this converts a working load into a different working load.
//
// The stat therefore lands only on what is otherwise an error path, so the
// cost is not paid by ordinary disc content.
static bool NativeLooseFileExists(const char *file) {
    if (!TheArchive) return false;
    // Normalize the way the archive index is keyed (drops "./", collapses
    // "..", etc). FileMakePathBuf, not FileMakePath: the latter returns a
    // shared static buffer and asserts MainThread().
    char norm[256];
    FileMakePathBuf(".", file, norm);
    int arkNum = 0, fileSize = 0, ucSize = 0;
    unsigned long long byteOff = 0;
    if (TheArchive->GetFileInfo(norm, arkNum, byteOff, fileSize, ucSize))
        return false; // the archive owns it -- not loose, and the ark wins
    // Qualify exactly as every other read here does, so NativeSetDataDir()
    // and the overlay directory are both honoured.
    char qualified[256];
    FileQualifiedFilename(qualified, 0x100, file);
    struct stat st;
    return stat(qualified, &st) == 0 && S_ISREG(st.st_mode);
}

// On Xbox, FileIsLocal checks for drive letters (d: = disc = not local).
// On native, files without absolute paths are "not local" when UsingCD,
// so they get routed through the archive system (ArkFile).
// Files that exist in the overlay directory are treated as local so they
// bypass the archive and load from disk.
bool FileIsLocal(const char *file) {
    if (!file || !*file) return true;
    // Absolute paths are always local
    if (file[0] == '/') return true;
    // Files in overlay directory are local (bypass archive)
    if (NativeOverlayExists(file)) return true;
    // Loose files (DLC/mods) are local too -- see NativeLooseFileExists.
    // Gated on UsingCD() because that is the only mode in which the caller
    // (os/File.cpp NewFile) would otherwise build an ArkFile.
    if (UsingCD() && NativeLooseFileExists(file)) return true;
    // When using CD (archive), relative paths are archive files, not local
    return false;
}

int FileGetStat(const char *iFilename, FileStat *iBuffer) {
    String fullName;
    FileQualifiedFilename(fullName, iFilename);
    struct stat st;
    if (stat(fullName.c_str(), &st) != 0) return -1;
    iBuffer->st_mode = st.st_mode;
    iBuffer->st_size = st.st_size;
#ifdef __APPLE__
    iBuffer->st_ctime = st.st_ctimespec.tv_sec;
    iBuffer->st_atime = st.st_atimespec.tv_sec;
    iBuffer->st_mtime = st.st_mtimespec.tv_sec;
#else
    iBuffer->st_ctime = st.st_ctim.tv_sec;
    iBuffer->st_atime = st.st_atim.tv_sec;
    iBuffer->st_mtime = st.st_mtim.tv_sec;
#endif
    return 0;
}

int FileDelete(const char *iFilename) {
    String str;
    FileQualifiedFilename(str, iFilename);
    return unlink(str.c_str()) == 0 ? 0 : -1;
}

int FileMkDir(const char *iDirname) {
    String str;
    FileQualifiedFilename(str, iDirname);
    return mkdir(str.c_str(), 0755) == 0 ? 1 : 0;
}

void FileQualifiedFilename(char *out, int, const char *in) {
    MILO_ASSERT(in && out, 0x121);
    // On native, prepend the data directory (like Xbox prepends "d:")
    // If the file exists in the overlay directory, use that instead.
    String str(in);
    const char *inStr = str.c_str();
    char buf[256];
    const char *baseDir = NativeOverlayExists(inStr) ? gNativeOverlayDir : gNativeDataDir;
    const char *path = FileMakePathBuf(baseDir, inStr, buf);
    strcpy(out, path);
}

void FileEnumerate(
    const char *dir,
    void (*cb)(const char *, const char *),
    bool recurse,
    const char *pattern,
    bool b2
) {
    if (UsingCD() && TheArchive) {
        TheArchive->Enumerate(dir, cb, recurse, pattern);
        return;
    }

    char qualified[256];
    FileQualifiedFilename(qualified, 0x100, dir);

    DIR *d = opendir(qualified);
    if (!d) {
        MILO_LOG("FileEnumerate: cannot open %s\n", qualified);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char buf[512];
        snprintf(buf, sizeof(buf), "%s/%s", qualified, entry->d_name);

        struct stat st;
        if (stat(buf, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (b2 && (!pattern || FileMatch(buf, pattern))) {
                cb(qualified, entry->d_name);
            }
            if (recurse) {
                FileEnumerate(buf, cb, recurse, pattern, b2);
            }
        } else {
            if (!b2 && (!pattern || FileMatch(buf, pattern))) {
                cb(qualified, entry->d_name);
            }
        }
    }
    closedir(d);
}
