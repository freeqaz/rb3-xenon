#pragma once
#include "os/Debug.h"
#include "utl/PoolAlloc.h"
#include "beatmatch/GemInfo.h"
#include "utl/Symbol.h"

class TempoMap; // forward dec

class GameGem {
public:
    GameGem(const MultiGemInfo &);
    GameGem(const RGGemInfo &);
    ~GameGem();
    GameGem &operator=(const GameGem &);

    signed char GetFret(unsigned int) const;
    char GetHighestFret() const;
    bool GetShowSlashes() const;
    unsigned char GetRootNote() const;
    bool IsRealGuitar() const;
    bool GetShowChordNames() const;
    bool Loose() const;
    bool ShowChordNums() const;
    bool LeftHandSlide() const;
    bool ReverseSlide() const;
    bool Enharmonic() const;
    RGNoteType GetRGNoteType(unsigned int) const;
    unsigned char GetImportantStrings() const;
    void SetImportantStrings(unsigned char);
    unsigned char GetHandPosition() const;
    int GetRGChordID() const;
    bool RightHandTap() const;
    unsigned int GetLowestString() const;
    unsigned int GetHighestString() const;
    int GetRGStrumType() const;
    const char *GetChordNameOverride() const;
    void SetFret(unsigned int, signed char);
    bool PlayableBy(int) const;
    static int CountBitsInSlotType(unsigned int);
    int NumSlots() const;
    void Flip(const GameGem &);
    void RecalculateTimes(TempoMap *);
    bool IsMuted() const;
    int GetFret() const;
    int GetNumStrings() const;
    int GetNumFingers() const;
    void PackRealGuitarData();
    static int GetHighestSlot(unsigned int);
    bool IsRealGuitarChord() const;
    void CopyGem(GameGem *, int);

    NEW_POOL_OVERLOAD(GameGem);
    DELETE_POOL_OVERLOAD(GameGem);

    bool operator<(const GameGem &g) const { return mMs < g.mMs; }

    int GetTick() const { return mTick; }
    bool IgnoreDuration() const { return mIgnoreDuration; }
    unsigned int GetSlots() const { return mSlots; }
    bool GetForceStrum() const { return mForceStrum; }
    int GetDurationTicks() const { return mDurationTicks; }
    float GetMs() const { return mMs; }
    bool GetNoStrum() const { return mForceStrum; }

    static bool CompareTimes(const GameGem &g1, const GameGem &g2) {
        return g1.mMs < g2.mMs;
    }

    int GetSlot() const {
        for (int i = 0; i < 32; i++) {
            if (mSlots & 1 << i)
                return i;
        }
        MILO_FAIL("Bad slots %d\n", mSlots);
        return -1;
    }

    RGNoteType GetRGNoteTypeEntry(int string) const { return mRGNoteTypes[string]; }
    void SetRGNoteTypeEntry(int x, RGNoteType ty) { mRGNoteTypes[x] = ty; }

    bool GetPlayed() const { return mPlayed != 0; }
    void SetPlayed(bool played) { mPlayed = played; }
    float DurationMs() const { return mDurationMs; }
    bool Unk10B1() const { return unk10b1; }
    void SetUnk10B1(bool b) { unk10b1 = b; }
    bool IsCymbal() const { return mIsCymbal; }

    float mMs; // 0x0
    int mTick; // 0x4
    unsigned short mDurationMs; // 0x8
    unsigned short mDurationTicks; // 0xa
    unsigned int mSlots; // 0xc

    unsigned char mPlayed : 1;
    unsigned char mForceStrum : 1;
    unsigned char mIgnoreDuration : 1;
    unsigned char mIsCymbal : 1;
    unsigned char mShowChordNames : 1;
    unsigned char mShowSlashes : 1;
    unsigned char unk10b1 : 1;
    // 0x10 bit 0 is UNUSED.  mRealGuitar is NOT the 8th field of this group -- it
    // is the FIRST field of the 0x12 group below.  Witness: IsRealGuitarChord
    // (fn_8278EA68, an identity nobody disputes -- its body is the 6-iteration
    // mFrets scan with `count > 1`) opens with `lbz r11,0x12(r3); clrrwi. r11,r11,7`,
    // which keeps ONLY bit 0x80 of byte 0x12.  Our source opens that function with
    // `if (!mRealGuitar) return false;`, so mRealGuitar IS 0x12 & 0x80.
    unsigned char : 1;

    // 0x11 -- retail X360 writes this as a whole byte (`stb rX, 0x11`) at two
    // independent sites (SongDB::DisableCodaGems, GemTrack::SetEnableSlot), so it
    // sits between the two bitfield groups, NOT after mRootNote where the rb3-Wii
    // dev header puts it (the Wii DEV build genuinely stores it at 0x18).
    // Compiler-verified (/d1reportSingleClassLayout): the only offsets that move
    // are 0x11..0x18 (the bitfield groups each slide +1, mRootNote absorbs the
    // vacated 0x18). Whole-binary A/B: +2 matched, 0 regressed.  (The tail
    // from 0x13 on was later re-laid-out from retail bytes -- see below.)
    unsigned char unk18; // 0x11 (mPlayers?)

    // 0x12.  Retail's ladder of one-bit getters over this byte descends
    // 0x80,0x40,0x20,0x10,0x08,0x04 at fn_8278EA58/EAD8/EAE8/EAF8/EB08/EB18, and
    // the map had every one of them attributed one position too EARLY (it read
    // 0x80 as Loose).  That error and our own header's were mirror images, so the
    // accessor BODIES matched while every CALL SITE was charged -- a compensating
    // pair.  Both sides corrected together; unk11b0 dropped so the group stays
    // eight bits and no offset moves.
    unsigned char mRealGuitar : 1;
    unsigned char mLoose : 1;
    unsigned char mShowChordNums : 1;
    unsigned char mLeftHandSlide : 1;
    unsigned char mReverseSlide : 1;
    unsigned char mEnharmonic : 1;
    unsigned char unk11b2 : 1;
    unsigned char unk11b1 : 1;

    unsigned char unk13; // 0x13

    // TU5 layout, read off retail bytes (lane W16-HN): the Wii dev build packed
    // strum type / hand position / note types into bitfields; retail X360 keeps
    // them unpacked.  GameGem(const RGGemInfo &) stores note_types[i] as words at
    // 0x14+4i, strum_type at 0x2c, hand_position at 0x30, root_note at 0x31,
    // frets at 0x32; GetRGNoteType is `lwzx (i+5)*4`; PackRealGuitarData writes
    // the chord id at 0x38; Get/SetImportantStrings use 0x40.  sizeof stays 0x44.
    RGNoteType mRGNoteTypes[6]; // 0x14
    int mStrumType; // 0x2c
    unsigned char mHandPosition; // 0x30
    unsigned char mRootNote; // 0x31
    char mFrets[6]; // 0x32
    int mRGChordID; // 0x38
    Symbol mChordNameOverride; // 0x3c
    unsigned char mImportantStrings; // 0x40
};
