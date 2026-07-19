#include "utl/FilePath.h"
#include "os/File.h"
#include "utl/BinStream.h"

FilePath FilePath::sRoot;
FilePath FilePath::sNull("");

BinStream &operator>>(BinStream &bs, FilePath &fp) {
    char buf[0x100];
    bs.ReadString(buf, 0x100);
    fp.SetRoot(buf);
    return bs;
}

void FilePath::Set(const char *str1, const char *str2) {
    const char *path;
    if (str2 && *str2) {
        path = FileMakePath(str1, str2);
    } else
        path = "";

    this->String::operator=(path); // well ok then
    // *this = path;
}

// sw2 scatter-include (default/FilePath <- char/CharBone.cpp)
#define gRev gRev_CharBone
#define gAltRev gAltRev_CharBone
#include "char/CharBone.cpp"
#undef gRev
#undef gAltRev
