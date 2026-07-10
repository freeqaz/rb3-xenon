#pragma once
#include "synth/StreamReader.h"

class File;
class StandardStream;

// RB3-360 XMA stream decoder (retail RTTI .?AVXMAReader@@ @82C75A3C).
// Synth360::NewStreamDecoder allocates 0x74 bytes for it and constructs with
// (File *, StandardStream *) — ctor @82B3B938 (own TU, not yet carved/ported).
class XMAReader : public StreamReader {
public:
    XMAReader(File *, StandardStream *);
    virtual ~XMAReader();
    virtual void Poll(float);
    virtual void Seek(int);
    virtual void EnableReads(bool);
    virtual bool Done();
    virtual bool Fail();
    virtual void Init();

private:
    int unk4[0x1C]; // pad to retail sizeof 0x74
};
