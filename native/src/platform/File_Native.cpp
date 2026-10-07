// DC3 Native Port - File I/O Implementation
// Replaces File_Win.cpp - POSIX file operations

#include <cstdio>
#include <cstring>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>

#include "os/Archive.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"

void NativeFileLedger(const char *fmt, ...); // FileLedger_Native.cpp

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

// Does host path `path` lie inside the data dir (the disc image) once
// symlinks and ".." are resolved? Both sides go through realpath, so a data
// dir that is itself a symlink, or holds symlinked parts, still counts.
static bool NativePathInsideDataDir(const char *path) {
    char root[PATH_MAX], real[PATH_MAX];
    if (!realpath(gNativeDataDir, root) || !realpath(path, real)) return false;
    size_t n = strlen(root);
    if (n == 1 && root[0] == '/') return true;
    return strncmp(real, root, n) == 0 && (real[n] == '/' || real[n] == '\0');
}

// A DEVICE path ("devkit:/x", "cache:/y"): retail's FileIsLocal is exactly
// "the drive in front of the colon is longer than one character". "d:" (the
// disc) is one character, so it is not a device and goes to the archive.
static bool NativeIsDevicePath(const char *file) {
    if (!file || file[0] == '/') return false;
    char drive[256];
    FileGetDriveBuf(file, drive);
    return strlen(drive) > 1;
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
    // ⛔ A .dta is never in the archive under its own name. CachedDataFile
    // (obj/DataFile.cpp) asks FileIsLocal with the .dta name and, when that
    // says "not local", reads <dir>/gen/<base>.dtb from the ark. So the check
    // above always missed a .dta, and any .dta that also existed on the host
    // filesystem was read as a host TEXT file instead of the shipped .dtb.
    // That happened for real (W16-UA): with the data dir at
    // ~/code/milohax/rb3/orig-assets/xbox-zip, "../../system/run/..." lands in
    // the rb3 Wii repo's system/run/, and 16 of the 27 files the preinit config
    // reads (default, macros, joypad, objects, every *_objects.dta, ...) came
    // from there. The ark owns a .dta exactly when it owns that .dtb.
    size_t n = strlen(norm);
    if (n > 4 && strcasecmp(norm + n - 4, ".dta") == 0) {
        const char *slash = strrchr(norm, '/');
        char dtb[300];
        if (slash)
            snprintf(dtb, sizeof(dtb), "%.*s/gen/%.*s.dtb", (int)(slash - norm), norm,
                     (int)(norm + n - 4 - (slash + 1)), slash + 1);
        else
            snprintf(dtb, sizeof(dtb), "gen/%.*s.dtb", (int)(n - 4), norm);
        if (TheArchive->GetFileInfo(dtb, arkNum, byteOff, fileSize, ucSize))
            return false;
    }
    // Qualify exactly as every other read here does, so NativeSetDataDir()
    // and the overlay directory are both honoured.
    char qualified[256];
    FileQualifiedFilename(qualified, 0x100, file);
    struct stat st;
    if (stat(qualified, &st) != 0 || !S_ISREG(st.st_mode)) return false;
    // ⛔ A loose file must be ON THE DISC IMAGE, i.e. inside the data dir once
    // symlinks and ".." are resolved (lane W16-UC). The .dta rule above only
    // closed W16-UA's instance; the hole was general. Every config path under
    // "../../system/run" climbs two levels out of the data dir, so any such
    // name the ark lacks was read from whatever host tree sat there -- and a
    // run doing so printed ALL GATES PASSED (measured: rb3-milo loaded a milo
    // planted at <data>/../../system/run/ui/gen/ from the host). Retail cannot
    // do this at all: a plain path is never local there.
    return NativePathInsideDataDir(qualified);
}

// On Xbox, FileIsLocal checks for drive letters (d: = disc = not local).
// On native, files without absolute paths are "not local" when UsingCD,
// so they get routed through the archive system (ArkFile).
// Files that exist in the overlay directory are treated as local so they
// bypass the archive and load from disk.
bool NativeFileIsDevicePath(const char *file) { return NativeIsDevicePath(file); }

bool FileIsLocal(const char *file) {
    if (!file || !*file) return true;
    // Absolute paths are always local
    if (file[0] == '/') {
        NativeFileLedger("LOCAL\t%s\tabsolute", file);
        return true;
    }
    // Retail's rule, and the only one retail has: a device path is local.
    // On a console "devkit:" does not exist, so the open fails on the host
    // door; here AsyncFileNative refuses it the same way (no host fallback).
    // Before W16-UC native sent "devkit:/locale_keep.dta" to the archive.
    if (NativeIsDevicePath(file)) {
        NativeFileLedger("LOCAL\t%s\tdevice", file);
        return true;
    }
    // Files in overlay directory are local (bypass archive)
    if (NativeOverlayExists(file)) {
        NativeFileLedger("LOCAL\t%s\toverlay", file);
        return true;
    }
    // Loose files (DLC/mods) are local too -- see NativeLooseFileExists.
    // Gated on UsingCD() because that is the only mode in which the caller
    // (os/File.cpp NewFile) would otherwise build an ArkFile.
    if (UsingCD() && NativeLooseFileExists(file)) {
        NativeFileLedger("LOCAL\t%s\tloose", file);
        return true;
    }
    // When using CD (archive), relative paths are archive files, not local
    return false;
}

int FileGetStat(const char *iFilename, FileStat *iBuffer) {
    if (NativeIsDevicePath(iFilename)) return -1; // no such device on this host
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
    if (NativeIsDevicePath(iFilename)) return -1;
    String str;
    FileQualifiedFilename(str, iFilename);
    return unlink(str.c_str()) == 0 ? 0 : -1;
}

int FileMkDir(const char *iDirname) {
    if (NativeIsDevicePath(iDirname)) return 0;
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
    // Retail (File_Win.cpp): a non-local dir under UsingCD is enumerated in
    // the archive; a local one on the device. A device dir has no host here.
    if (NativeIsDevicePath(dir)) {
        MILO_LOG("FileEnumerate: no device for %s\n", dir);
        return;
    }
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
