#include "moviebink/BinkMovieImpl.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include "xdk/win_types.h"
#include "xdk/xapilibi/fileapi.h"
#include "xdk/xapilibi/handleapi.h"
#include "xdk/xapilibi/minwinbase.h"
#include "xdk/xbdm/xbdm.h"

extern "C" LONG CompareFileTime(const FILETIME *, const FILETIME *);

void MakeDir(const char *path) {
    String dir(path);
    String parent(FileGetPath(dir.c_str()));
    if (parent != dir) {
        MakeDir(parent.c_str());
    }
    CreateDirectoryA(dir.c_str(), 0);
}

bool BinkMovieImpl::PlatformCacheFile(const char *filename) {
    if (UsingCD() || mLoop) {
        return true;
    }

    DmMapDevkitDrive();

    FileStat stat;
    if (FileGetStat(mFilename.c_str(), &stat) < 0) {
        return false;
    }

    long long fileTime = ((long long)(long)stat.st_mtime + 0x2B6109100LL) * 10000000LL;
    FILETIME srcTime;
    srcTime.dwHighDateTime = (DWORD)fileTime;
    srcTime.dwLowDateTime = (DWORD)(fileTime >> 32);

    String cachePath(FileMakePath("DEVKIT:", filename));
    FileQualifiedFilename(cachePath, cachePath.c_str());

    WIN32_FILE_ATTRIBUTE_DATA attrData;
    if (GetFileAttributesExA(cachePath.c_str(), GetFileExInfoStandard, &attrData) == 0
        || CompareFileTime(&attrData.ftLastWriteTime, &srcTime) < 0) {
        File *fin = NewFile(mFilename.c_str(), FILE_OPEN_READ);
        MILO_ASSERT(fin, 0x46);

        MakeDir(FileGetPath(cachePath.c_str()));

        HANDLE hFile = CreateFileA(
            cachePath.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
        );
        if ((int)hFile == -1) {
            if (fin) {
                delete fin;
            }
            return false;
        }

        char buffer[0x10000];
        while (!fin->Eof()) {
            int bytesRead = fin->Read(buffer, 0x10000);
            DWORD written;
            WriteFile(hFile, buffer, bytesRead, &written, 0);
        }
        CloseHandle(hFile);
        delete fin;
    }

    mFilename = cachePath;
    return true;
}
