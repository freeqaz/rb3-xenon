#include "bandobj/BandSongPref.h"
#include "obj/Object.h"
#include "utl/Symbols.h"

INIT_REVS(3, 0)

BandSongPref::BandSongPref()
    : mPart2Instrument("guitar"), mPart3Instrument("bass"), mPart4Instrument("drum"),
      mAnimGenre("rocker") {}

// rb3-Wii's dev decomp has `SAVE_OBJ(BandSongPref, 24)` here (an unconditional
// MILO_ASSERT(0)), but RB3-360 retail ships a real saver: fn_822C0D48 writes the
// packed rev 3 through BinStream::WriteEndian, chains to Hmx::Object::Save, then
// streams the four Symbol members from this+0x28/0x2c/0x30/0x34 -- exactly
// mPart2Instrument, mPart3Instrument, mPart4Instrument, mAnimGenre (Hmx::Object
// is 0x28 bytes on 360, so the Wii offsets 0x1c..0x28 shift by 0xc).
BEGIN_SAVES(BandSongPref)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mPart2Instrument;
    bs << mPart3Instrument;
    bs << mPart4Instrument;
    bs << mAnimGenre;
END_SAVES

BEGIN_LOADS(BandSongPref)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    LOAD_SUPERCLASS(Hmx::Object);
    bs >> mPart2Instrument;
    bs >> mPart3Instrument;
    bs >> mPart4Instrument;
    if (d.rev != 0 && d.rev < 3) {
        unsigned char dump;
        bs >> dump;
    }
    if (d.rev > 1)
        bs >> mAnimGenre;
END_LOADS

BEGIN_COPYS(BandSongPref)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(BandSongPref)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPart2Instrument)
        COPY_MEMBER(mPart3Instrument)
        COPY_MEMBER(mPart4Instrument)
        COPY_MEMBER(mAnimGenre)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(BandSongPref)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(BandSongPref)
    SYNC_PROP(part2_inst, mPart2Instrument)
    SYNC_PROP(part3_inst, mPart3Instrument)
    SYNC_PROP(part4_inst, mPart4Instrument)
    SYNC_PROP(animation_genre, mAnimGenre)
END_PROPSYNCS
