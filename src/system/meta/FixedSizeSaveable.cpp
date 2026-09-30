#include "meta/FixedSizeSaveable.h"
#include "meta/FixedSizeSaveableStream.h"
#include "os/Debug.h"
#include <typeinfo>
#include <list>
#include <string.h>
#include "utl/Str.h"

int FixedSizeSaveable::sCurrentMemcardLoadVer = -1;
int FixedSizeSaveable::sSaveVersion = -1;
int FixedSizeSaveable::sMaxSymbols = -1;
unsigned char FixedSizeSaveable::sPadder;
bool FixedSizeSaveable::sPrintoutsEnabled;

FixedSizeSaveable::FixedSizeSaveable() : mSaveSizeMethod(0) {}

FixedSizeSaveable::~FixedSizeSaveable() {}

void FixedSizeSaveable::Init(int i1, int i2) {
    sSaveVersion = i1;
    sMaxSymbols = i2;
}

void FixedSizeSaveable::EnablePrintouts(bool b) { sPrintoutsEnabled = b; }

void FixedSizeSaveable::PadStream(FixedSizeSaveableStream &fixedStream, int padSize) {
    char buf[1024];
#ifdef HX_NATIVE
    MILO_ASSERT(fixedStream.Tell() + padSize <= fixedStream.Size(), 0x30);
    memset(buf, sPadder, 1024);
#else
    // retail (rb3-Wii shape): no Tell/Size assert, zero padding
    memset(buf, 0, 1024);
#endif
    for (; padSize > 0x400; padSize -= 0x400) {
        fixedStream.Write(buf, 0x400);
    }
    if (padSize > 0) {
        fixedStream.Write(buf, padSize);
    }
}

void FixedSizeSaveable::DepadStream(FixedSizeSaveableStream &fixedStream, int padSize) {
    char buf[1024];
#ifdef HX_NATIVE
    MILO_ASSERT(fixedStream.Tell() + padSize <= fixedStream.Size(), 0x46);
#endif
    for (; padSize > 0x400; padSize -= 0x400) {
        fixedStream.Read(buf, 0x400);
    }
    if (padSize > 0) {
        fixedStream.Read(buf, padSize);
    }
}

void FixedSizeSaveable::SaveFixedSymbol(
    FixedSizeSaveableStream &fixedStream, const Symbol &sym
) {
#ifndef HX_NATIVE
    // retail (0x827A2B60, TU5): a symbol that cannot fit is written as a
    // zero-padded blank slot instead of overrunning the fixed size
    if (strlen(sym.Str()) >= kSymbolSize) {
        PadStream(fixedStream, kSymbolSize);
        return;
    }
#endif
    int start = fixedStream.Tell();
    fixedStream << sym;
    MILO_ASSERT(fixedStream.Tell()-start <= kSymbolSize, 0x5E);
    PadStream(fixedStream, start + (kSymbolSize - fixedStream.Tell()));
}

void FixedSizeSaveable::LoadFixedSymbol(
    FixedSizeSaveableStream &fixedStream, Symbol &sym
) {
    int start = fixedStream.Tell();
    fixedStream >> sym;
    MILO_ASSERT(fixedStream.Tell()-start <= kSymbolSize, 0x65);
    DepadStream(fixedStream, start + (kSymbolSize - fixedStream.Tell()));
}

// Retail 0x827A2C28 (200 B) / 0x827A2A50 (116 B). A string that cannot fit in
// kStringSize is replaced by a zero block (memset 0x400 with 0, not sPadder, then
// Write(buf, 0x80)) rather than overrunning the fixed slot.
void FixedSizeSaveable::SaveFixedString(
    FixedSizeSaveableStream &fixedStream, const String &str
) {
    if (str.length() >= kStringSize) {
        char buf[0x400];
        memset(buf, 0, sizeof(buf));
        fixedStream.Write(buf, kStringSize);
    } else {
        int start = fixedStream.Tell();
        fixedStream << str;
        MILO_ASSERT(fixedStream.Tell()-start <= kStringSize, 0x6C);
        PadStream(fixedStream, start + (kStringSize - fixedStream.Tell()));
    }
}

void FixedSizeSaveable::LoadFixedString(FixedSizeSaveableStream &fixedStream, String &str) {
    int start = fixedStream.Tell();
    fixedStream >> str;
    MILO_ASSERT(fixedStream.Tell()-start <= kStringSize, 0x73);
    DepadStream(fixedStream, start + (kStringSize - fixedStream.Tell()));
}

void FixedSizeSaveable::SaveSymbolID(FixedSizeSaveableStream &stream, Symbol sym) {
    int id;
    if (stream.HasSymbol(sym))
        id = stream.GetID(sym);
    else
        id = stream.AddSymbol(sym);
    stream << id;
}

void FixedSizeSaveable::LoadSymbolFromID(FixedSizeSaveableStream &stream, Symbol &sym) {
    int id = 0;
    stream >> id;
    sym = stream.GetSymbol(id);
}

void FixedSizeSaveable::LoadSymbolTable(FixedSizeSaveableStream &fs, int max, int j) {
    int size;
    fs >> size;
    MILO_ASSERT(size <= max, 0xA9);
    for (int x = 0; x < size; x++) {
        Symbol s;
        LoadFixedSymbol(fs, s);
        int someInt;
        fs >> someInt;
        fs.SetSymbolID(s, someInt);
    }
    if (max > size) {
        DepadStream(fs, j * (max - size));
    }
}

void FixedSizeSaveable::SaveStd(
    FixedSizeSaveableStream &fs, const std::vector<Symbol> &vec, int max
) {
    int size = vec.size();
    MILO_ASSERT(size <= max, 0xBA);
    fs << size;
    for (std::vector<Symbol>::const_iterator it = vec.begin(); it != vec.end(); it++) {
        SaveSymbolID(fs, *it);
    }
    if (max > size)
        PadStream(fs, (max - size) * 4);
}

FixedSizeSaveableStream &
operator<<(FixedSizeSaveableStream &fs, const FixedSizeSaveable &saveable) {
    MILO_ASSERT(FixedSizeSaveable::sSaveVersion >= 0, 0x10C);
    MILO_ASSERT(FixedSizeSaveable::sMaxSymbols >= 0, 0x10D);

    int oldtell = fs.Tell();
    saveable.SaveFixed(fs);
    int newtell = fs.Tell();

    MILO_ASSERT_FMT(!fs.Fail(), "FixedSizeSaveableStream operator<< fixedStream failed!");
    MILO_ASSERT_FMT(
        saveable.mSaveSizeMethod,
        "You must set the save size method of a FixedSizeSaveable object by            using the SETSAVESIZE macro in its constructor!"
    );

    if (oldtell + saveable.mSaveSizeMethod(FixedSizeSaveable::GetSaveVersion())
        != newtell) {
        MILO_FAIL(
            "Bad save file size!  %s wrote %d instead of the expected %d",
            typeid(saveable).name(),
            newtell - oldtell,
            saveable.mSaveSizeMethod(FixedSizeSaveable::GetSaveVersion())
        );
    }

    return fs;
}

FixedSizeSaveableStream &
operator>>(FixedSizeSaveableStream &fs, FixedSizeSaveable &saveable) {
    MILO_ASSERT(FixedSizeSaveable::sSaveVersion >= 0, 0x12D);
    MILO_ASSERT(FixedSizeSaveable::sMaxSymbols >= 0, 0x12E);
    MILO_ASSERT(FixedSizeSaveable::sCurrentMemcardLoadVer > 0, 0x12F);

    int asdf = FixedSizeSaveable::sCurrentMemcardLoadVer;

    int oldtell = fs.Tell();
    saveable.LoadFixed(fs, asdf);
    int newtell = fs.Tell();

    MILO_ASSERT_FMT(
        saveable.mSaveSizeMethod,
        "You must set the save size method of a FixedSizeSaveable object by            using the SETSAVESIZE macro in its constructor!"
    );

    if (oldtell + saveable.mSaveSizeMethod(asdf) != newtell) {
        MILO_FAIL(
            "Bad load!  %s read %d instead of the expected %d!",
            typeid(saveable).name(),
            newtell - oldtell,
            saveable.mSaveSizeMethod(asdf)
        );
    }
    return fs;
}

void FixedSizeSaveable::SaveSymbolTable(FixedSizeSaveableStream &fs, int max, int j) {
    std::hash_map<Symbol, int> &symMap = fs.GetSymbolToIDMap();
    int size = symMap.size();
    MILO_ASSERT(size <= max, 0x9A);
    fs << size;
    for (std::hash_map<Symbol, int>::iterator it = symMap.begin(); it != symMap.end(); it++) {
        SaveFixedSymbol(fs, it->first);
        fs << it->second;
    }
    if (max > size)
        PadStream(fs, j * (max - size));
}

void FixedSizeSaveable::SaveStd(
    FixedSizeSaveableStream &fs, const std::set<Symbol> &set, int max
) {
    int size = set.size();
    MILO_ASSERT(size <= max, 0xEC);
    fs << size;
    for (std::set<Symbol>::const_iterator it = set.begin(); it != set.end(); it++) {
        SaveSymbolID(fs, *it);
    }
    if (max > size)
        PadStream(fs, (max - size) * 4);
}

void FixedSizeSaveable::LoadStd(
    FixedSizeSaveableStream &fs, std::set<Symbol> &set, int max
) {
    int size;
    fs >> size;
    MILO_ASSERT(size <= max, 0xF9);
    for (int idx = 0; idx < size; idx++) {
        Symbol s;
        LoadSymbolFromID(fs, s);
        set.insert(s);
    }
    if (max > size)
        DepadStream(fs, (max - size) * 4);
}

void FixedSizeSaveable::LoadStd(
    FixedSizeSaveableStream &fs, std::vector<Symbol> &vec, int max
) {
    int size;
    fs >> size;
    MILO_ASSERT(size <= max, 0xC5);
    vec.resize(size);
    for (int x = 0; x < size; x++) {
        LoadSymbolFromID(fs, vec[x]);
    }
    if (max > size)
        DepadStream(fs, (max - size) * 4);
}

// Retail 0x827A3078 (136 B) / 0x827A2ED8 (148 B): the std::list<Symbol> overloads.
void FixedSizeSaveable::SaveStd(
    FixedSizeSaveableStream &fs, const std::list<Symbol> &list, int max
) {
    int size = list.size();
    fs << size;
    for (std::list<Symbol>::const_iterator it = list.begin(); it != list.end(); it++) {
        SaveSymbolID(fs, *it);
    }
    if (max > size)
        PadStream(fs, (max - size) * 4);
}

void FixedSizeSaveable::LoadStd(
    FixedSizeSaveableStream &fs, std::list<Symbol> &list, int max
) {
    int size;
    fs >> size;
    for (int idx = 0; idx < size; idx++) {
        Symbol s;
        LoadSymbolFromID(fs, s);
        list.push_back(s);
    }
    if (max > size)
        DepadStream(fs, (max - size) * 4);
}
