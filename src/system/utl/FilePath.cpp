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
    char buf[256];
    const char *path;
    if (str2 && *str2) {
        path = FileMakePathBuf(str1, str2, buf);
    } else
        path = "";

    this->String::operator=(path); // well ok then
    // *this = path;
}
