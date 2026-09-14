#pragma once
#include "utl/Str.h"
#include "utl/TextFileStream.h"
#include "utl/TextStream.h"

class LogFile : public TextStream {
public:
    String mFilePattern;
    int mSerialNumber;
    bool mDirty;
    TextFileStream *mFile;
    bool mActive;

    LogFile(const char *);
    // Retail's ~LogFile (ICF survivor at 0x823eb8e0, 76 B) is the IMPLICIT
    // destructor: no `delete mFile`, and no LogFile vptr store at entry (the
    // vtable-store elision that only an implicitly-declared dtor gets -- see
    // docs/decomp/patterns/fixable-declarations.md). It is a COMDAT emitted by
    // the first TU that needs it (network/net/NetLog.cpp). The Wii dev source's
    // `delete mFile; mFile = 0;` body is NOT in retail 360. Do not declare one.
    virtual void Print(const char *);

    void Reset();
    void AdvanceFile();
    bool IsActive() { return mActive; }
    void SetActive(bool b) { mActive = b; }
};
