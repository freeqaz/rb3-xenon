#pragma once
#include "os/File.h"
#include "utl/Str.h"

class XboxMapFile {
public:
    XboxMapFile(const char *);
    ~XboxMapFile();

    const char *GetFunction(unsigned int, bool);
    static bool ParseStack(const char *, struct StackData *, int, FixedString &);

private:
    void ReadLine(char *, int);

    File *mFile; // 0x0
    int mStart; // 0x4
};
