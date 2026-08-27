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
    // NOT virtual: retail's ??_7XMAReader@@6B@ @0x82197138 is SIX slots and all
    // six are spoken for by body shape -- [0] deleting dtor, [1]/[2] substantial
    // (Poll/Seek), [3] the bare-`blr` hub (an EMPTY EnableReads(bool)), [4]
    // `lbz r3,0x72(r3); blr` (Done) and [5] `li r3,0; blr` (Fail returns false).
    // The bound is hard: slot 6 would be 0x82197150, which holds 0xffffffff and
    // is not an image VA. There is no slot left for Init. (This class is not in
    // objects.json, so this is a declaration-accuracy fix and is metric-neutral
    // by construction -- our build emits no XMAReader vtable at all.)
#ifdef HX_NATIVE
    virtual
#endif
        void Init();

private:
    int unk4[0x1C]; // pad to retail sizeof 0x74
};
