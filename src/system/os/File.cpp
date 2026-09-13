#include "os/File.h"
#include "os/AsyncFile.h"
#include "os/Block.h"
#include "os/FileCache.h"
#include "os/ArkFile_p.h"
#include "HolmesClient.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "os/Debug.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "types.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Option.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <list>
#ifdef HX_NATIVE
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#undef st_ctime
#undef st_atime
#undef st_mtime
#endif

// DECLARATION ORDER HERE IS CODEGEN-LOAD-BEARING -- do not "tidy" it.
// MSVC lays out non-COMDAT .bss in REVERSE declaration order (dynamically
// initialized objects like gFiles/gDirList are appended last, separately), so
// the scalars below must be declared BEFORE the four path globals in order for
// gSystemRoot to land at .bss offset 0x000, giving:
//     gSystemRoot 0x000 / gExecRoot 0x100 / gRoot 0x200 / gOpenCaptureFile 0x300
// which is retail's exact layout (gSystemRoot = 0x82CC9D90).
//
// It matters beyond layout: when several same-section statics are referenced in
// one function, MSVC hoists ONE of them into a callee-saved register and
// addresses the rest as displacements off it. It anchors on the static at
// section offset 0x000. With the scalars declared first, gSystemRoot is that
// symbol -- matching retail, which keeps &gSystemRoot in r31 (it is needed as a
// VALUE across the FileMakePath call by the inlined strcpy) and reaches
// gOpenCaptureFile for free as 0x300(r31). Declared the other way round the
// anchor becomes gOpenCaptureFile, every offset goes negative, and FileInit
// pays an extra `subi r11,r31,0x300` -- 8 mismatched instructions, 99.0% not
// 100%. Measured, lane NCCC-0803-b2bb/f33: size/position/refcount/scalar-vs-
// array/CFG-shape all fail to move the anchor; only the offset-0x000 slot does.
bool gFakeFileErrors;
bool gNullFiles;
void *kNoHandle;
DataArray *gFrameRateArray;
int gCaptureFileMode;

static File *gOpenCaptureFile;
static char gRoot[256];
static char gExecRoot[256];
static char gSystemRoot[256];

std::vector<File *> gFiles(0x80); // 0x10...?
std::vector<String> gDirList;
const int File::MaxFileNameLen = 0x100;

const char *FileRoot() { return gRoot; }
const char *FileExecRoot() { return gExecRoot; }
const char *FileSystemRoot() { return gSystemRoot; }

#ifdef HX_NATIVE
extern const char *NativeGetDataDir();

static bool NativeDirExists(const char *path) {
    struct stat st;
    return path && *path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void NativeSetCanonicalPath(char *dst, size_t dstSize, const char *path) {
    char resolved[PATH_MAX];
    if (path && *path && realpath(path, resolved)) {
        strncpy(dst, resolved, dstSize - 1);
    } else if (path) {
        strncpy(dst, path, dstSize - 1);
    } else {
        *dst = '\0';
        return;
    }
    dst[dstSize - 1] = '\0';
}

static void NativeInitSystemRoot() {
    char extractedSystemRun[PATH_MAX];
    const char *dataDir = NativeGetDataDir();
    if (dataDir && *dataDir) {
        snprintf(
            extractedSystemRun,
            sizeof(extractedSystemRun),
            "%s/extracted/(..)/(..)/system/run",
            dataDir
        );
        if (NativeDirExists(extractedSystemRun)) {
            NativeSetCanonicalPath(gSystemRoot, sizeof(gSystemRoot), extractedSystemRun);
            return;
        }
    }

    NativeSetCanonicalPath(gSystemRoot, sizeof(gSystemRoot), "../../system/run");
}
#endif

void FileTerminate() {
    RELEASE(gOpenCaptureFile);
    *gRoot = 0;
    *gExecRoot = 0;
    *gSystemRoot = 0;
    TheDebug.StopLog();
    HolmesClientTerminate();
}

void FileQualifiedFilename(String &out, const char *in) {
    char buf[256];
    FileQualifiedFilename(buf, 0x100, in);
    out = buf;
}

void FileNormalizePath(const char *cc) {
    for (char *ptr = (char *)cc; *ptr != '\0'; ptr++) {
        if (*ptr == '\\')
            *ptr = '/';
        else
            *ptr = tolower(*ptr);
    }
}

const char *FileGetDriveBuf(const char *iFilepath, char *oBuf) {
    MILO_ASSERT(iFilepath, 0x437);
    MILO_ASSERT(oBuf, 0x438);
    const char *p = strchr(iFilepath, ':');
    if (p != 0) {
        strncpy(oBuf, iFilepath, p - iFilepath);
        oBuf[p - iFilepath] = '\0';
    } else {
        oBuf[0] = '\0';
    }
    return oBuf;
}

const char *FileGetDrive(const char *file) {
    static char drive[256];
    const char *p = strchr(file, ':');
    if (p != 0) {
        strncpy(drive, file, p - file);
        drive[p - file] = '\0';
    } else
        drive[0] = '\0';
    return drive;
}

const char *FileGetPathBuf(const char *file, char *path) {
    MILO_ASSERT(path, 0x3F6);
    if (file != 0) {
        if (file != path)
            strcpy(path, file);
        char *p2 = path + strlen(path) - 1;
        while (p2 >= path && *p2 != '/' && *p2 != '\\') {
            p2--;
        }
        if (p2 >= path) {
            if ((p2 == path) || (p2[-1] == ':'))
                p2[1] = '\0';
            else
                *p2 = '\0';
            return path;
        }
    }
    path[0] = '.';
    path[1] = '\0';
    return path;
}

const char *FileGetPath(const char *file) {
    static char static_path[256];
    char *p2;
    if (file != 0) {
        strcpy(static_path, file);
        p2 = static_path + strlen(static_path);
        p2--;
        while (p2 >= static_path && *p2 != '/' && *p2 != '\\') {
            p2--;
        }
        if (p2 >= static_path) {
            if ((p2 == static_path) || (p2[-1] == ':'))
                p2[1] = '\0';
            else
                *p2 = '\0';
            return static_path;
        }
    }
    *static_path = '.';
    static_path[1] = '\0';
    return static_path;
}

const char *FileGetBaseBuf(const char *file, char *base) {
    MILO_ASSERT(file, 0x458);
    MILO_ASSERT(base, 0x459);
    const char *dir = strrchr(file, '/');
    if ((dir == 0) && (dir = strrchr(file, '\\'), dir == 0))
        strcpy(base, file);
    else
        strcpy(base, dir + 1);
    char *ext = strrchr(base, '.');
    if (ext != 0)
        *ext = 0;
    return base;
}

const char *FileGetBase(const char *file) {
    static char my_path[256];
    const char *dir;
    char *ext;
    dir = strrchr(file, '/');
    if ((dir != 0) || (dir = strrchr(file, '\\'), (dir != 0)))
        strcpy(my_path, dir + 1);
    else
        strcpy(my_path, file);
    ext = strrchr(my_path, '.');
    if (ext != 0)
        *ext = 0;
    return my_path;
}

const char *FileGetExt(const char *root) {
    const char *end = root + strlen(root);
    for (const char *search = end - 1; search >= root; search--) {
        if (*search == '.') {
            return search + 1;
        } else if (*search == '/' || *search == '\\') {
            return end;
        }
    }
    return end;
}

const char *FileGetName(const char *file) {
    static char path[256];
    const char *dir = strrchr(file, '/');
    if ((dir != 0) || (dir = strrchr(file, '\\'), (dir != 0)))
        strcpy(path, dir + 1);
    else
        strcpy(path, file);
    return path;
}

static bool FileMatchInternal(const char *arg0, const char *arg1, bool arg2) {
    for (; *arg0 != 0; arg0++) {
        if (FileMatch(arg0, arg1))
            return true;
        if (!arg2 && (*arg0 == '/' || *arg0 == '\\'))
            return false;
    }
    return (*arg1 == *arg0);
}

bool FileMatch(const char *param1, const char *param2) {
    if (param2 == 0)
        return false;
    while (*param2 != '\0') {
        if (*param2 == '*')
            return FileMatchInternal(param1, param2 + 1, 0);
        if (*param2 == '&')
            return FileMatchInternal(param1, param2 + 1, 1);
        if (*param1 == '\0')
            break;
        if (*param2 == '?') {
            if ((*param1 == '\\') || (*param1 == '/'))
                return 0;
        } else if ((*param2 == '/') || (*param2 == '\\')) {
            if ((*param1 != '/') && (*param1 != '\\'))
                return 0;
        } else if (*param2 != *param1)
            return 0;
        param2++;
        param1++;
    }
    return (*param2 - *param1) == 0;
}

const char *FrameRateSuffix() {
    return MakeString("_keep_%s.dta", PlatformSymbol(TheLoadMgr.GetPlatform()));
}

// the weird __rs in the debug symbols here, is for a FileStat&
// so BinStream >> FileStat
BinStream &operator>>(BinStream &bs, FileStat &fs) {
    bs >> fs.st_mode >> fs.st_size;
    u64 ctime;
    bs >> ctime;
    fs.st_ctime = ctime;
    u64 atime;
    bs >> atime;
    fs.st_atime = atime;
    u64 mtime;
    bs >> mtime;
    fs.st_mtime = mtime;
    return bs;
}

DataNode OnFileExecRoot(DataArray *da) { return gExecRoot; }
DataNode OnFileRoot(DataArray *da) { return gRoot; }
DataNode OnFileGetExt(DataArray *da) { return FileGetExt(da->Str(1)); }
DataNode OnFileMatch(DataArray *da) { return FileMatch(da->Str(1), da->Str(2)); }

// The guarded do/while is load-bearing for matching -- do NOT fold it back into
// the equivalent `for (i = 2; i < thresh; i++)`. With the `for` form MSVC runs
// its count-down-to-zero induction-variable elimination: it drops `i` entirely,
// synthesizes a decrementing trip counter (`addic.`/`bne`) and precomputes the
// post-loop value of `i` ahead of the loop -- 2 extra preheader instructions to
// save 1 on the back edge. Retail keeps a true loop-carried `i` (`addi`/`cmpw`/
// `blt`) alongside the byte-offset accumulator. Writing the rotation by hand
// suppresses the transform and reproduces retail exactly.
DataNode OnWithFileRoot(DataArray *da) {
    FilePathTracker fpt(da->Str(1));
    int thresh = da->Size() - 1;
    int i = 2;
    if (i < thresh) {
        do {
            da->Command(i)->Execute();
            i++;
        } while (i < thresh);
    }
    return da->Evaluate(i);
}

DataNode OnSynchProc(DataArray *) {
    MILO_FAIL("calling synchproc on non-pc platform");
    return "";
}

void OnFrameRateRecurseCB(const char *cc1, const char *cc2) {
    MILO_ASSERT(gFrameRateArray, 0x120);
    String str(cc2);
    str = str.substr(0, str.length() - strlen(FrameRateSuffix()));
    gFrameRateArray->Insert(gFrameRateArray->Size(), str);
}

// Retail re-materializes &tmp (`addi r4, r31, 0x50`) for the push_back arg instead
// of reusing the String ctor's returned `this` in r3 -- i.e. a *named* local, not
// a temporary bound directly to the call.
void DirListCB(const char *, const char *cc2) {
    String tmp(cc2);
    gDirList.push_back(tmp);
}

bool FileExists(const char *iFilename, int iMode) {
    MILO_ASSERT((iMode & ~FILE_OPEN_NOARK) == 0, 0x2A8);
    File *theFile = NewFile(iFilename, iMode | 0x40002);
    if (theFile) {
        delete theFile;
        return true;
    } else
        return false;
}

bool FileExists(const char *iFilename, int iMode, String *str) {
    MILO_ASSERT((iMode & ~FILE_OPEN_NOARK) == 0, 0x2A8);
    File *theFile = NewFile(iFilename, iMode | 0x40002);
    if (theFile) {
        if (str) {
            *str = theFile->Filename();
        }
        delete theFile;
        return true;
    } else
        return false;
}

String UniqueFilename(const char *c1, const char *c2) {
    int i = 0;
    String ret;
    File *file = nullptr;
    do {
        i++;
        ret = MakeString("%s_%06d.bmp", c1, i);
        delete file;
        file = NewFile(ret.c_str(), 1);
    } while (file);
    return ret;
}

DataNode OnFileGetDrive(DataArray *da) { return FileGetDrive(da->Str(1)); }
DataNode OnFileGetPath(DataArray *da) { return FileGetPath(da->Str(1)); }
DataNode OnFileGetBase(DataArray *da) { return FileGetBase(da->Str(1)); }
DataNode OnFileAbsolutePath(DataArray *da) {
    return FileMakePath(da->Str(1), da->Str(2));
}
DataNode OnFileRelativePath(DataArray *da) {
    return FileRelativePath(da->Str(1), da->Str(2));
}
DataNode OnToggleFakeFileErrors(DataArray *da) {
    gFakeFileErrors = !gFakeFileErrors;
    Hmx::Object *obj = ObjectDir::Main()->Find<Hmx::Object>("cheat_display", true);
    if (obj) {
        static Message msg(
            "cheat_display", DataNode("Fake File errors"), DataNode("show_bool")
        );
        msg[2] = gFakeFileErrors;
        obj->Handle(msg, true);
    }
    return 0;
}

DataNode OnEnumerateFrameRateResults(DataArray *da) {
    DataNode ret(new DataArray(0), kDataArray);
    gFrameRateArray = ret.Array();
    char *suffix = (char *)FrameRateSuffix();
    const char *pattern = MakeString("ui/framerate/venue_test/*%s", suffix);
    RecursePatternInternal(pattern, OnFrameRateRecurseCB, false, false);
    gFrameRateArray = 0;
    return ret;
}

void FileInit() {
    strcpy(gRoot, ".");
    strcpy(gExecRoot, ".");
#ifdef HX_NATIVE
    NativeInitSystemRoot();
#else
    strcpy(gSystemRoot, FileMakePath(gExecRoot, "../../system/run"));
#endif
    FilePath::Root().Set(gRoot, gRoot);
    DataRegisterFunc("file_root", OnFileRoot);
    DataRegisterFunc("file_exec_root", OnFileExecRoot);
    DataRegisterFunc("file_get_drive", OnFileGetDrive);
    DataRegisterFunc("file_get_path", OnFileGetPath);
    DataRegisterFunc("file_get_base", OnFileGetBase);
    DataRegisterFunc("file_get_ext", OnFileGetExt);
    DataRegisterFunc("file_match", OnFileMatch);
    DataRegisterFunc("file_absolute_path", OnFileAbsolutePath);
    DataRegisterFunc("file_relative_path", OnFileRelativePath);
    DataRegisterFunc("with_file_root", OnWithFileRoot);
    DataRegisterFunc("synch_proc", OnSynchProc);
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    // Neither "toggle_fake_file_errors" nor "enumerate_frame_rate_results" occurs in
    // retail band.exe -- dev-only DataRegisterFunc entries, exactly the
    // loadmgr_debug/loadmgr_print class this lane already gated in LoadMgr::Init.
    // rb3-Wii's FileInit has HolmesClientInit() inside this same ifdef block
    // (not called unconditionally like dc3-decomp's newer version), and retail
    // bytes confirm it: objdiff showed the HolmesClientInit call as base-only
    // insert residue at 88.98% match (idx 33, lane NCCC-0803-b2bb/f33/sonnet).
    DataRegisterFunc("toggle_fake_file_errors", OnToggleFakeFileErrors);
    DataRegisterFunc("enumerate_frame_rate_results", OnEnumerateFrameRateResults);
    HolmesClientInit();
#endif
    const char *str = OptionStr("file_order", nullptr);
    if (str && *str) {
        gOpenCaptureFile = NewFile(str, 0x301);
        MILO_ASSERT(gOpenCaptureFile, 0x18F);
    }
#ifdef HX_NATIVE
    // rb3-Wii's FileInit has no AddExitCallback(FileTerminate) call at all --
    // this is a dc3-decomp-only addition (dc3 is a newer engine snapshot; see
    // CLAUDE.md source-provenance caveat). Retail bytes lack it too (base-only
    // insert residue), so gate it native-only rather than dropping it outright.
    TheDebug.AddExitCallback(FileTerminate);
#endif
}

const char *FileRelativePathBuf(const char *iRoot, const char *iFilepath, char *oBuf) {
    MILO_ASSERT(iRoot, 0x38d);
    MILO_ASSERT(iFilepath, 0x38e);
    MILO_ASSERT(oBuf, 0x38f);
    if (*iFilepath != '\0') {
        char rootBuf[256];
        char fpBuf[256];
        strcpy(rootBuf, iRoot);
        strcpy(fpBuf, iFilepath);

        std::list<char *> rootToks;
        std::list<char *> fpToks;

        char *rootTok = strtok(rootBuf, "/");
        if (rootTok != nullptr) {
            do {
                rootToks.push_back(rootTok);
                rootTok = strtok(nullptr, "/");
            } while (rootTok != nullptr);
        }

        char *fpTok = strtok(fpBuf, "/");
        if (fpTok != nullptr) {
            do {
                fpToks.push_back(fpTok);
                fpTok = strtok(nullptr, "/");
            } while (fpTok != nullptr);
        }

        if (!fpToks.empty() && !rootToks.empty()) {
            if (strcmp(fpToks.front(), rootToks.front()) == 0) {
                while (rootToks.size() > 0 && fpToks.size() > 0
                       && strcmp(fpToks.front(), rootToks.front()) == 0) {
                    rootToks.pop_front();
                    fpToks.pop_front();
                }

                char *p = oBuf;
                while (rootToks.size() > 0) {
                    if (p != oBuf)
                        *p++ = '/';
                    *p++ = '.';
                    *p++ = '.';
                    rootToks.pop_front();
                }
                while (fpToks.size() > 0) {
                    if (p != oBuf)
                        *p++ = '/';
                    for (const char *pp = fpToks.front(); *pp != '\0'; pp++)
                        *p++ = *pp;
                    fpToks.pop_front();
                }
                MILO_ASSERT(p - oBuf < File::MaxFileNameLen, 0x3d9);
                if (p == oBuf)
                    *p++ = '.';
                *p = '\0';
                return oBuf;
            }
        }
    }
    return iFilepath;
}

// Retail RB3-Xbox holds the WHOLE relative-path body in fn_82517718 (904 B)
// against an unconditional static; this is not FileRelativePathBuf inlined.
// Three independent tells, none of which needs a build:
//   * the -0x2a0 frame is exactly 0x70 locals + 0x100 rootBuf + 0x100 fpBuf +
//     0x30 param area, leaving NO room for a third 256-byte buffer (the same
//     frame arithmetic that settled FileMakePath for lane W7-A);
//   * r5 is never read, so retail's is the TWO-arg form our File.h declares;
//   * the tail is `mr r3,r30`, where r30 holds iFilepath on the two early-out
//     paths and lbl_82CCA6B0 -- the static -- on the success path.
// Its only callees are strtok x4, list::insert x2, list::erase x4 and
// _List_base::clear x2: there is NO bl to MainThread(), so that site is
// SPURIOUS here exactly as lane W6-A's audit found for the other File.cpp
// sites, and it is deleted rather than wrapped (MILO_ASSERT still EVALUATES
// its argument in this build, and MainThread() is an extern call MSVC cannot
// elide -- lane W5-C).
// FileRelativePathBuf below has no caller outside this file, but it stays for
// the reason W5-C kept FileGetBaseBuf: an unreferenced COMDAT does not score,
// and deleting it is a native-visible API change. The two bodies are
// deliberate duplicates -- keep them in sync. Lane W8-D.
const char *FileRelativePath(const char *iRoot, const char *iFilepath) {
    MILO_ASSERT(iRoot, 0x38d);
    MILO_ASSERT(iFilepath, 0x38e);
    static char relative[256];
    if (*iFilepath != '\0') {
        char rootBuf[256];
        char fpBuf[256];
        strcpy(rootBuf, iRoot);
        strcpy(fpBuf, iFilepath);

        std::list<char *> rootToks;
        std::list<char *> fpToks;

        char *rootTok = strtok(rootBuf, "/");
        if (rootTok != nullptr) {
            do {
                rootToks.push_back(rootTok);
                rootTok = strtok(nullptr, "/");
            } while (rootTok != nullptr);
        }

        char *fpTok = strtok(fpBuf, "/");
        if (fpTok != nullptr) {
            do {
                fpToks.push_back(fpTok);
                fpTok = strtok(nullptr, "/");
            } while (fpTok != nullptr);
        }

        if (!fpToks.empty() && !rootToks.empty()) {
            // Retail compares root FIRST: the inlined strcmp loop loads
            // 0x8(r7) (rootToks.front()) into r11 and computes `subf r9,r6,r9`
            // = root_char - fp_char, so root is the left-hand argument.
            if (strcmp(rootToks.front(), fpToks.front()) == 0) {
                while (rootToks.size() > 0 && fpToks.size() > 0
                       && strcmp(rootToks.front(), fpToks.front()) == 0) {
                    rootToks.pop_front();
                    fpToks.pop_front();
                }

                char *p = relative;
                while (rootToks.size() > 0) {
                    if (p != relative)
                        *p++ = '/';
                    *p++ = '.';
                    *p++ = '.';
                    rootToks.pop_front();
                }
                while (fpToks.size() > 0) {
                    if (p != relative)
                        *p++ = '/';
                    for (const char *pp = fpToks.front(); *pp != '\0'; pp++)
                        *p++ = *pp;
                    fpToks.pop_front();
                }
                MILO_ASSERT(p - relative < File::MaxFileNameLen, 0x3d9);
                if (p == relative)
                    *p++ = '.';
                *p = '\0';
                return relative;
            }
        }
    }
    return iFilepath;
}

const char *FileMakePathBuf(const char *root, const char *file, char *buffer) {
    MILO_ASSERT(root, 0x300);
    MILO_ASSERT(file, 0x301);
    MILO_ASSERT(buffer, 0x302);
    char buf[256];
    if (file >= buffer && file < buffer + File::MaxFileNameLen) {
        strcpy(buf, file);
        file = buf;
    } else if (root >= buffer && root < buffer + File::MaxFileNameLen) {
        strcpy(buf, root);
        root = buf;
    }
    char driveBuf[256];
    const char *fileDrive = FileGetDriveBuf(file, driveBuf);
    if (*fileDrive != '\0') {
        file += strlen(fileDrive) + 1;
    }
    char *c = buffer;
    if (*file == '/' || *file == '\\' || *file == '\0') {
        if (*fileDrive != '\0') {
            sprintf(buffer, "%s:%s", fileDrive, file);
            c = buffer + strlen(fileDrive) + 1;
        } else {
            const char *rootDrive = FileGetDriveBuf(root, driveBuf);
            if (*rootDrive != '\0') {
                sprintf(buffer, "%s:%s", rootDrive, file);
                c = buffer + strlen(rootDrive) + 1;
            } else {
                strcpy(buffer, file);
            }
        }
    } else {
        sprintf(buffer, "%s/%s", root, file);
        const char *rootDrive = FileGetDriveBuf(root, driveBuf);
        if (*rootDrive != '\0') {
            c = buffer + strlen(rootDrive) + 1;
        }
    }
    FileNormalizePath(buffer);
    bool curSlash = (*c == '/');
    const char *dirs[32];
    const char **endDir = &dirs[0];
    char *p = strtok(c, "/");
    while (p != nullptr) {
        if (*p != '.')
            *endDir++ = p;
        else if (p[1] == '.' && p[2] == '\0') {
            if (endDir != dirs && *endDir[-1] != '.')
                endDir--;
            else
                *endDir++ = p;
        }
        p = strtok(nullptr, "/");
    }
    MILO_ASSERT(endDir - dirs <= 32, 0x35c);
    if (endDir == dirs) {
        if (curSlash) {
            *c++ = '/';
        } else {
            *c++ = '.';
        }
    } else {
        for (const char **dir = (const char **)&dirs[0]; dir != endDir; dir++) {
            if (dir != dirs || curSlash) {
                *c++ = '/';
            }
            for (char *p = (char *)*dir; *p != '\0'; p++) {
                *c++ = *p;
            }
        }
    }
    MILO_ASSERT(c - buffer < File::MaxFileNameLen, 0x372);
    *c = '\0';
    return buffer;
}

// Retail RB3-Xbox has NO out-of-line 3-arg helper on this path: fn_82516B10 is
// 792 B and holds the whole body against an unconditional static, calling the
// ONE-arg FileGetDrive (`mr r3,r31; bl fn_82516680` -- r4 is never set) with no
// `char driveBuf[256]` local (its 0x200 frame is exactly 0x50 saves + 0x80
// dirs[32] + 0x100 buf[256] + 0x30 param area; a driveBuf would force >=0x300).
// So this is NOT FileMakePathBuf inlined -- MSVC at /O1 will not inline a
// 198-instruction helper anyway (lane W5-C measured the limit at ~40) -- it is
// retail's own body. FileMakePathBuf below is a DC3 refactor that RB3 did not
// have; it is kept because DirLoader/File_Win/the native port call it, so the
// two bodies are deliberate duplicates. Keep them in sync. Lane W7-A.
const char *FileMakePath(const char *root, const char *file) {
    MILO_ASSERT(root, 0x300);
    MILO_ASSERT(file, 0x301);
    static char static_buffer[256];
    char buf[256];
    if (file >= static_buffer && file < static_buffer + File::MaxFileNameLen) {
        strcpy(buf, file);
        file = buf;
    } else if (root >= static_buffer && root < static_buffer + File::MaxFileNameLen) {
        strcpy(buf, root);
        root = buf;
    }
    const char *fileDrive = FileGetDrive(file);
    if (*fileDrive != '\0') {
        file += strlen(fileDrive) + 1;
    }
    // `c` is assigned on EVERY path and never pre-initialized: retail's r31
    // holds `file` and is then recycled as the cursor (`.L_82347E5C: mr r31,r28`
    // is a join-point assignment). Hoisting `char *c = static_buffer;` above the
    // branch makes the initial value live across the whole chain and costs a
    // SIXTH callee-saved register -- measured as `insert: mr r30,r27` plus a
    // uniform r28/r29/r30 -> r27/r28/r29 shift over 63 arguments. Lane W7-A.
    char *c;
    if (*file == '/' || *file == '\\' || *file == '\0') {
        if (*fileDrive != '\0') {
            sprintf(static_buffer, "%s:%s", fileDrive, file);
            c = static_buffer + strlen(fileDrive) + 1;
        } else {
            const char *rootDrive = FileGetDrive(root);
            if (*rootDrive != '\0') {
                sprintf(static_buffer, "%s:%s", rootDrive, file);
                c = static_buffer + strlen(rootDrive) + 1;
            } else {
                strcpy(static_buffer, file);
                c = static_buffer;
            }
        }
    } else {
        sprintf(static_buffer, "%s/%s", root, file);
        const char *rootDrive = FileGetDrive(root);
        if (*rootDrive != '\0') {
            c = static_buffer + strlen(rootDrive) + 1;
        } else {
            c = static_buffer;
        }
    }
    FileNormalizePath(static_buffer);
    bool curSlash = (*c == '/');
    const char *dirs[32];
    const char **endDir = &dirs[0];
    // ONE strtok call site, not two: retail enters the loop at the call
    // (`b .L_82347EE4`) and feeds NULL on later iterations from the join point
    // `.L_82347EE0: li r3, 0x0`. Written as `p = strtok(c,"/"); while(p) { ...;
    // p = strtok(0,"/"); }` MSVC emits two call sites and does not merge them.
    // Lane W7-A.
    // ⛔ The peeled first strtok call (retail emits ONE call site, entered by
    // `b .L_82347EE4`, with `.L_82347EE0: li r3,0` feeding NULL on the back
    // edge) is NOT reachable from the source loop shape. Four spellings were
    // measured BYTE-IDENTICAL: this one; `while ((p = strtok(arg,"/")) != 0)`;
    // `for(;;) { p = strtok(arg,"/"); if (!p) break; ... }`; and a `goto` into
    // a do/while, which cannot be guard-duplicated at source level at all.
    // MSVC normalises the CFG and re-derives the peel every time. Lane W7-A.
    char *p = strtok(c, "/");
    while (p != nullptr) {
        if (*p != '.')
            *endDir++ = p;
        else if (p[1] == '.' && p[2] == '\0') {
            if (endDir != dirs && *endDir[-1] != '.')
                endDir--;
            else
                *endDir++ = p;
        }
        p = strtok(nullptr, "/");
    }
    MILO_ASSERT(endDir - dirs <= 32, 0x35c);
    if (endDir == dirs) {
        if (curSlash) {
            *c++ = '/';
        } else {
            *c++ = '.';
        }
    } else {
        for (const char **dir = (const char **)&dirs[0]; dir != endDir; dir++) {
            if (dir != dirs || curSlash) {
                *c++ = '/';
            }
            for (char *p = (char *)*dir; *p != '\0'; p++) {
                *c++ = *p;
            }
        }
    }
    MILO_ASSERT(c - static_buffer < File::MaxFileNameLen, 0x372);
    *c = '\0';
    return static_buffer;
}

const char *FileLocalize(const char *iFilename, char *buffer) {
    GfxMode mode = GetGfxMode();
    bool isOg = (mode == kNewGfx);
    if (!SystemLocale().Null() || isOg) {
        Symbol lang2 = SystemLocale();
        if (!lang2.Null()) {
            for (const char *p = iFilename; *p != '\0'; p++) {
                if (*p == '/' && p[1] == 'e' && p[2] == 'n' && p[3] == 'g'
                    && p[4] == '/') {
                    static char mybuffer[256];
                    if (!buffer)
                        buffer = mybuffer;
                    strcpy(buffer, iFilename);
                    if (!HongKongExceptionMet()
                        || (strstr(iFilename, "locale") == 0
                            && strstr(iFilename, "ui/eng") == 0)) {
                        Symbol lang3 = SystemLocale();
                        const char *langStr = lang3.Str();
                        buffer[p + 1 - iFilename] = langStr[0];
                        buffer[p + 2 - iFilename] = langStr[1];
                        buffer[p + 3 - iFilename] = langStr[2];
                    } else {
                        buffer[p + 1 - iFilename] = 'e';
                        buffer[p + 2 - iFilename] = 'n';
                        buffer[p + 3 - iFilename] = 'g';
                    }
                    return buffer;
                }
            }
        }
        if (isOg) {
            for (const char *p = iFilename; *p != '\0'; p++) {
                if (*p == '/' && p[1] == 'o' && p[2] == 'g' && p[3] == '/') {
                    if (buffer == iFilename) {
                        ((char *)p)[1] = 'n';
                        return iFilename;
                    }
                    if (!buffer) {
                        static char mybuffer[256];
                        buffer = mybuffer;
                    }
                    strcpy(buffer, iFilename);
                    buffer[p + 1 - iFilename] = 'n';
                    return buffer;
                }
            }
        }
    }
    return iFilename;
}

bool FileDiscSpinUp() { return TheBlockMgr.SpinUp(); }

bool FileReadOnly(const char *filepath) { return true; }

File *NewFile(const char *iFilename, int iMode) {
    // Ported from retail fn_825173E0 (424 B, 27 relocations).  Four constructs
    // our inherited body carried that retail's does NOT:
    //   * (RETAINED, see below) a gNullFiles / new NullFile() early-out;
    //   * a TheDebug.Notify branch -- retail calls MainThread() at +24 and
    //     DISCARDS r3 with no test following, which is MILO_ASSERT(cond,line)
    //     compiling to ((void)(cond)): the extern call still evaluates, the
    //     Notify does not exist;
    //   * a UsingCD() call in the ArkFile guard (retail tests only mode bits);
    //   * a null check around Fail() -- retail calls result->Fail() through the
    //     vtable unconditionally, including on the mem==0 path.
    // ⛔ MEASURED NEGATIVE -- do NOT delete this branch to match retail's
    // NewFile.  Retail's fn_825173E0 provably has no gNullFiles/NullFile path
    // (424 B, 27 relocations, no operator new, no NullFile vtable relocation),
    // and removing ours took our body to a near-exact 416 B / 27.  But it is
    // the ONLY thing in this TU that forces NullFile's vtable -- and with it
    // NullFile::Write, NullFile::ReadDone, File::~File and File::Filename --
    // to be emitted into File.obj.  Retail's File.obj DOES define all four
    // (they are pinned, named and were scoring 100), so retail's File.cpp
    // instantiates NullFile somewhere we have not yet located.  Deleting the
    // branch therefore made our object define FEWER symbols than retail's:
    // A/B measured -4 matched functions / -88 B (8+16+48+16, exact).
    // ⇒ Locating retail's real NullFile emission site is a PREREQUISITE for
    // finishing this body port, and hence for naming 0x825173E0.
    if (gNullFiles) {
        return new NullFile();
    }

    MILO_ASSERT(MainThread(), 0x2FE);

    File *result = nullptr;

    if ((iFilename != nullptr) && (*iFilename != '\0')) {
        const char *filename = iFilename;
        int mode = iMode;
        char localized[256];

        if (mode & 0x2) {
            filename = FileLocalize(iFilename, localized);
        }

        if (FileIsLocal(filename)) {
            mode |= 0x10000;
        }

        if ((mode & 0x2) && !(mode & 0x20000)) {
            File *cached = FileCache::GetFileAll(filename);
            if (cached != nullptr) {
                return cached;
            }
        }

        if ((mode & 0x2) && !(mode & 0x10000)) {
            void *mem = _MemAllocTemp(sizeof(ArkFile), __FILE__, 0x19, "ArkFile", 0);
            if (mem != nullptr) {
                result = new (mem) ArkFile(filename, mode);
            } else {
                result = nullptr;
            }
        } else {
            mode &= ~0x4000;
            result = AsyncFile::New(filename, mode);
        }

        if (result->Fail()) {
            delete result;
            return nullptr;
        }

        if ((gOpenCaptureFile != nullptr) && (mode & 0x2)) {
            char path_buf[256];
            // Retail's format literal is lbl_82087CB0 = "'%s'\n" (the "./%s"
            // we had is not in the binary), and its strlen loop increments
            // BEFORE the test (`lbz; addi; cmplwi; bne`), so p ends at
            // buf+strlen+1 and the trailing -1 yields strlen.  Our
            // test-before-increment form yielded strlen-1 -- one byte short,
            // a real off-by-one in the capture log, not just a shape diff.
            sprintf(path_buf, "'%s'\n", FileMakePath(".", filename));
            const char *ptr = path_buf;
            while (*ptr++) {
            }
            gOpenCaptureFile->Write(path_buf, (ptr - path_buf) - 1);
            gOpenCaptureFile->Flush();
        }
    }

    return result;
}

void FileRecursePattern(
    const char *pattern, void (*cb)(const char *, const char *), bool recurse
) {
    RecursePatternInternal(pattern, cb, recurse, false);
}

#ifndef HX_NATIVE
// PPC (Xbox 360) implementation — logic derived from Ghidra decompile
void RecursePatternInternal(
    const char *pattern,
    void (*cb)(const char *, const char *),
    bool recurse,
    bool recurse_dirs
) {
    MILO_ASSERT(pattern && pattern[0], 0x5B8);
    String pttn(pattern);

    // Find split point: first '&', or end-of-string if absent
    unsigned int ampPos = pttn.find_first_of("&", 0);
    unsigned int wildcardPos = pttn.find_first_of("?*", 0);

    int splitPos;
    if (ampPos == FixedString::npos) {
        splitPos = (int)pttn.length() - 1;
    } else {
        splitPos = ampPos;
    }
    if (wildcardPos != FixedString::npos && wildcardPos < (unsigned int)splitPos) {
        splitPos = wildcardPos;
    }

    // If recurse enabled and no & wildcard: check for path-separator past splitPos
    if (recurse && ampPos == (int)FixedString::npos) {
        // Retail keeps pttn.length() ITSELF in r28 (`clrrwi r28,r11,0`), not
        // length()-1: we emitted an extra `subi r28,r11,0x1` and paid for it in
        // flipped strictness (`ble`/`bgt` where retail has `blt`/`bge`) and an
        // extra `addi r6,r11,1` on the substr count. rb3-Wii (File.cpp:589) has
        // the length()-1 form -- retail-Xbox differs from the Wii oracle here,
        // as it also does on the recomputed dirs.size() and the 1-arg
        // FileGetPath above. Retail bytes outrank the oracle. Lane W7-A.
        int pttnLen = (int)pttn.length();
        // Walk forward from splitPos looking for path separator
        int forwardPos = splitPos;
        while (forwardPos < pttnLen && pttn[forwardPos] != '/'
               && pttn[forwardPos] != '\\') {
            forwardPos++;
        }
        if (forwardPos == pttnLen) {
            // No path separator found — disable recurse for FileEnumerate
            recurse = false;
        } else {
            // Path separator found: we need to recurse into subdirectories
            // Two-arg substr, not the one-arg form: retail's RecursePatternInternal
            // makes 3 calls to ?substr@String@@QBA?AV1@II@Z and 0 to the one-arg
            // ?substr@String@@QBA?AV1@I@Z (measured on the split target obj's
            // relocations). dc3 -- which is NEWER than RB3 -- uses the one-arg form
            // here and we inherited it; the RB3-era rb3-Wii oracle (File.cpp:599)
            // uses the two-arg form and retail agrees with rb3-Wii.
            // Behaviourally identical: pttnLen is length()-1, so the count
            // (pttnLen+1)-forwardPos is exactly length()-forwardPos, i.e. "to end".
            String subPattern = pttn.substr(
                (unsigned int)forwardPos, (unsigned int)pttnLen - forwardPos
            );
            pttn = pttn.substr(0, (unsigned int)forwardPos);

            // Enumerate subdirectories at this level
            RecursePatternInternal(pttn.c_str(), DirListCB, false, true);
            std::vector<String> dirs(gDirList);
            if (gDirList.begin() != gDirList.end()) {
                gDirList.erase(gDirList.begin(), gDirList.end());
            }

            // Retail's fn_82517E28 makes ZERO calls to MainThread (fn_824A4C10)
            // and exactly ONE `bl fn_82516550` = the ONE-arg FileGetPath, which
            // owns its own static (lbl_82CCA0B0). So neither the MainThread()
            // nor a local `static char pathBuf[256]` is retail's. Lane W7-A.
            const char *dirBase = FileGetPath(pttn.c_str());
            pttn = dirBase;

            // Retail RECOMPUTES dirs.size() every iteration -- the loop test is
            // `lwz 0x74(r31); lwz 0x70(r31); subf; divw r11,r11,r27; cmplw` =
            // (end-begin)/sizeof(String) INSIDE the loop, with 12 in r27 -- where
            // hoisting it into `numDirs` gives a countdown (`subic. r30,r30,1`).
            // Lane W7-A.
            for (unsigned int i = 0; i < dirs.size(); i++) {
                const char *combined = MakeString(
                    "%s/%s%s", pttn, dirs[i], subPattern
                );
                RecursePatternInternal(combined, cb, recurse, recurse_dirs);
            }
            return;
        }
    }

    // Walk backward from splitPos to find last path separator
    // Retail's LOOP walks down to -1 (`subic. r30,r30,0x1` / `bge` back-edge,
    // so pttn[0] IS examined) but its final TEST is still `pos > 0`
    // (`cmpwi cr6,r30,0x0` / `bgt`). The two halves take DIFFERENT bounds --
    // flipping both together re-inverted the test. Lane W7-A.
    int pos = splitPos;
    while (pos >= 0 && pttn[pos] != '/' && pttn[pos] != '\\') {
        pos--;
    }
    // A conditional EXPRESSION yielding a String temporary in both arms, not an
    // if/else of two assignments. Retail constructs `String(".")` at 0xa0
    // (`bl ??0String@@QAA@PBD@Z`, bit 0) or the substr temp at 0xb0 (bit 1),
    // assigns via ??4String@@QAAAAV0@ABV0@@Z, then destroys whichever was built
    // -- driven by MSVC's conditional-destruction bitmask at 0x54(r31)
    // (`li r30,1` / `li r30,2` / `rlwinm.` tests / `rlwinm` clears). Two plain
    // assignments need no temporary and emit no bitmask at all. Lane W7-A.
    String dirStr;
    dirStr = (pos <= 0) ? String(".") : pttn.substr(0, (unsigned int)pos);
    FileEnumerate(dirStr.c_str(), cb, recurse, pttn.c_str(), recurse_dirs);
}
#endif
