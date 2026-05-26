#include "meta/FixedSizeSaveable.h"
#include "meta/FixedSizeSaveableStream.h"
#include "os/Debug.h"
#include <typeinfo>

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
    MILO_ASSERT(fixedStream.Tell() + padSize <= fixedStream.Size(), 0x30);
    memset(buf, sPadder, 1024);
    for (; padSize > 0x400; padSize -= 0x400) {
        fixedStream.Write(buf, 0x400);
    }
    if (padSize > 0) {
        fixedStream.Write(buf, padSize);
    }
}

void FixedSizeSaveable::DepadStream(FixedSizeSaveableStream &fixedStream, int padSize) {
    char buf[1024];
    MILO_ASSERT(fixedStream.Tell() + padSize <= fixedStream.Size(), 0x46);
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
    std::map<Symbol, int> &symMap = fs.GetSymbolToIDMap();
    int size = symMap.size();
    MILO_ASSERT(size <= max, 0x9A);
    fs << size;
    for (std::map<Symbol, int>::iterator it = symMap.begin(); it != symMap.end(); it++) {
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
