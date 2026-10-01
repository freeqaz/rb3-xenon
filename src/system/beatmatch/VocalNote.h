#pragma once
#include "math/Utl.h"
#include "utl/Str.h"
#include "obj/Data.h"
#include "utl/MBT.h"
#include "utl/TempoMap.h"

class VocalNote {
public:
    VocalNote()
        : mPhrase(-1), mBeginPitch(0), mEndPitch(0), mMs(0), mTick(0), mDurationMs(0),
          mDurationTicks(0), mPhraseEnd(0), mUnpitchedPhrase(0), mUnpitchedNote(0),
          mUnpitchedEasy(0), mPitchRangeEnd(0), mPlayerMask(0), mBends(0), mLyricShift(0),
          mAllowCombine(1) {}

    int GetTick() const { return mTick; }
    void SetNoteTime(float ms, int tick) {
        mMs = ms;
        mTick = tick;
    }
    void SetStartPitch(int pitch) { mBeginPitch = pitch; }
    void SetEndPitch(int pitch) { mEndPitch = pitch; }
    float GetDurationMs() const { return mDurationMs; }
    float GetMs() const { return mMs; }
    unsigned short GetDurationTicks() const { return mDurationTicks; }
    bool IsUnpitched() const { return mUnpitchedNote; }
    void SetPhraseEnd(bool b) { mPhraseEnd = b; }
    bool LyricShift() const { return mLyricShift; }

    void SetDurationTime(float ms, int tick) {
        mDurationMs = ms;
        mDurationTicks = tick;
    }
    void SetBends(bool bends) { mBends = bends; }
    void SetText(const char *text) { mText = text; }

    int StartPitch() const { return mBeginPitch; }
    int EndPitch() const { return mEndPitch; }
    int EndTick() const { return mTick + mDurationTicks; }
    float EndMs() const { return mMs + mDurationMs; }
    bool PlayableBy(int) const;
    // Out of line in retail (0x826F16E0, a COMDAT in VocalPart's span);
    // VocalNoteList::PitchAt calls it after its own end-of-note test.
    float PitchAt(float ms) const {
        if (EndPitch() == StartPitch())
            return (float)StartPitch();
        float fraction =
            Max<float>(0.0f, Min<float>(ms, mMs + mDurationMs) - mMs) / mDurationMs;
        return fraction * (float)EndPitch() + (1.0f - fraction) * (float)StartPitch();
    }

    int mPhrase; // 0x0
    int mBeginPitch; // 0x4
    int mEndPitch; // 0x8
    float mMs; // 0xc
    int mTick; // 0x10
    float mDurationMs; // 0x14
    unsigned short mDurationTicks; // 0x18
    String mText; // 0x1c
    bool mPhraseEnd; // 0x28
    bool mUnpitchedPhrase; // 0x29
    bool mUnpitchedNote; // 0x2a
    bool mUnpitchedEasy; // 0x2b
    bool mPitchRangeEnd; // 0x2c
    unsigned char mPlayerMask; // 0x2d
    bool mBends; // 0x2e
    bool mLyricShift; // 0x2f
    bool mAllowCombine; // 0x30
};

class VocalPhrase {
public:
    VocalPhrase();
    // No user-declared copy ctor: the implicit one is trivial, so copies are a
    // 0x38-byte memcpy (retail 0x826B9BB0 VocalTrainerPanel::CopyPhrasesImp).

    bool Diff() const { return (unk14 - unk10) == 0; }

    float unk0;
    float unk4;
    int unk8;
    int unkc;
    int unk10;
    int unk14;
    bool unk18;
    bool unk19;
    bool unk1a;
    int unk1c;
    float unk20;
    float unk24;
    float unk28;
    unsigned char unk2c;
    bool mTambourinePhrase; // 0x2d
    float unk30;
    float unk34;
};

class SongData;

class VocalNoteList {
public:
    VocalNoteList(SongData *);
    void Clear();
    void CopyPhrasesFrom(const VocalNoteList *);
    void CopyLyricPhrases();
    void AddNote(const VocalNote &);
    void NotesDone(const TempoMap &, bool);
    void DeterminePhraseTimes(const TempoMap &);
    void Finalize();
    void DetermineFreestyleSections();
    void AddTambourineGem(int);
    void SetFreestyleSections(const std::vector<std::pair<float, float> > &);
    void GenerateLegalFreestyleSections(std::vector<std::pair<float, float> > &) const;
    void RemoveInvalidFreestyleSections();
    void UpdatePitchRangeTickDelimited(int, int, float &, float &);
    void AddLyricShift(float);
    void StartPlayerPhrase(int, int);
    void EndPlayerPhrase(int, int);
    VocalNote *NextNote(float) const;
    const VocalNote *NoteAt(float) const;
    float PitchAt(float) const;
    void CapLastFreestyleSection(float);
    void GetPracticePhrases(std::vector<VocalPhrase> &, int, int) const;
    void GetPracticePhrases2(std::vector<VocalPhrase> &, int, int) const;
    int GetNumPracticePhrases(const std::vector<VocalPhrase> &) const;
    static bool
    IsIllegalFreestyleSection(DataArray *, const std::pair<float, float> &);

    const char *PrintTick(int tick) const;
    Symbol GetTrackName() const { return mTrackName; }
    void SetTrackName(Symbol name) { mTrackName = name; }
    const std::vector<VocalNote> &GetNotes() const { return mNotes; }
    std::vector<VocalPhrase> &GetPhrases() { return mPhrases; }
    std::vector<VocalPhrase> &GetLyricPhrases() { return mLyricPhrases; }
    int HasNoteInRange(int, int) const;

    std::vector<VocalPhrase> mPhrases; // 0x0
    std::vector<VocalPhrase> mLyricPhrases; // 0xc
    std::vector<VocalNote> mNotes; // 0x18
    std::vector<int> mTambourineGems; // 0x24
    std::vector<std::pair<float, float> > mFreestyleSections; // 0x30
    Symbol mTrackName; // 0x3c
    SongData *mSongData; // 0x40
    DataArray *mFreestyleMinDuration; // 0x44
    DataArray *mFreestylePad; // 0x48
};
