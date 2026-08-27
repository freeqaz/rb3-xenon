#pragma once

class StreamReader {
public:
    StreamReader() {}
    virtual ~StreamReader() {}
    virtual void Poll(float) = 0;
    virtual void Seek(int) = 0;
    virtual void EnableReads(bool) = 0;
    virtual bool Done() = 0;
    virtual bool Fail() = 0;
    // StreamReader deliberately does NOT declare Init(). Retail's own vtable
    // ??_7StreamReader@@6B@ @0x8219711c is SIX slots: one deleting destructor
    // (0x82b6a398) plus FIVE _purecall (0x828299b8) -- so exactly five pure
    // virtuals, and we declared six. Bounded by the next COL, so the length is
    // exact, and every slot is unnamed in the map, so no name or ICF fold can
    // poison this read.
    //
    // Which one is surplus is settled on retail BODIES, not on names:
    //   * VorbisReader @0x821a1a34 (9 slots) has slot 3 = `stb r4,0x3c(r3);blr`
    //     (a setter TAKING an argument => EnableReads(bool)), slot 4 =
    //     `lbz r3,0x45(r3);blr` and slot 5 = `lbz r3,0x11c(r3);blr` (two bool
    //     getters on DIFFERENT fields => Done/Fail), slot 6 = substantial work
    //     (Init) and slot 8 = a bare `blr` (=> EndData(){}). Every rival
    //     hypothesis is refuted by those shapes: if any of Poll/Seek/
    //     EnableReads/Done/Fail were the missing one, the surviving new virtual
    //     would have to appear AFTER Init, putting substantial code at slot 5
    //     and a one-instruction accessor at slot 6 -- the opposite of retail.
    //   * XMAReader @0x82197138 is the decisive length check: it is a CONCRETE
    //     reader that explicitly declares `virtual void Init();`, yet its retail
    //     table is SIX slots -- dtor, Poll, Seek, empty `blr` EnableReads,
    //     `lbz r3,0x72(r3)` Done, `li r3,0; blr` Fail. There is no slot left for
    //     Init, which there would have to be if StreamReader declared it.
    //
    // Each reader introduces its own Init instead (WavReader.h, VorbisReader.h,
    // BinkReader.h), which lands at the same slot either way, and nothing
    // dispatches Init through a StreamReader *.
#ifdef HX_NATIVE
    // milo-native-engine's FFmpegAudioReader marks Init `override`, so the
    // shared native engine fails to compile without this. HX_NATIVE is never
    // defined by the match build (cflags are exactly /D_XBOX360 and
    // /DCURL_STATICLIB), so the PPC vtable still gets the correct six slots.
    // Same shape as the AsyncFile::GetFileHandle guard in os/AsyncFile.h.
    virtual void Init() = 0;
#endif
};
