#pragma once
#include "bandtrack/Tail.h"
#include "beatmatch/GameGem.h"
#include "track/TrackWidget.h"
#include "bandtrack/GemRepTemplate.h"
#include "system/utl/TimeConversion.h"
#include "types.h"
#include <functional>

class GemManager;

class Gem {
public:
    Gem(const GameGem &, unsigned int, float, float, bool, int, int, bool);
    ~Gem();
    Gem &operator=(const Gem &);
    bool OnScreen(float);
    void Poll(float, float, float, float, float);
    void AddRep(GemRepTemplate &, RndGroup *, Symbol, const TrackConfig &, bool);
    bool UseRGChordStyle() const;
    void RemoveRep();
    void AddInstance(Symbol, int);
    void AddChordInstance(Symbol);
    void AddStrumInstance(Symbol, Symbol);
    void AddWidgetInstanceImpl(TrackWidget *, int);
    void AddHopoTails(Symbol);
    void RemoveAllInstances();
    void SetType(Symbol);
    void UpdateTailPositions();
    void CreateWidgetInstances(Symbol);
    void Miss();
    void Hit();
    void PartialHit(unsigned int);
    void Release();
    void ApplyDuration(float, float, float);
    void ReleaseSlot(int);
    void KillDuration();
    void Reset();
    float GetStart() const;
    void InitChordInfo(int, bool);
    void SetFretPos(int);
    void GetChordFretLabelInfo(String &, int &) const;

    bool CompareBounds() { return mEnd > mStart ? true : false; }
    bool Check66B0() const { return mIsCymbalLane; }
    const GameGem &GetGameGem() const { return mGameGem; }
    unsigned int Slots() const { return mSlots; }
    bool Released() const { return mReleased; }
    bool GetHit() const { return mHit; }

    // Retail X360 layout reconstructed from operator=/InitChordInfo/AddInstance asm:
    // bool flags are individual bytes (not bitfields); ints/floats full-width.
    GemManager *mGemManager; // 0x0
    const GameGem &mGameGem; // 0x4
    std::set<TrackWidget *> mWidgets; // 0x8
    float mStart; // 0x20
    float mEnd; // 0x24
    float mTailStart; // 0x28
    bool mHit; // 0x2c
    bool mMissed; // 0x2d
    bool mReleased; // 0x2e
    bool mHopo; // 0x2f
    bool mInvisible; // 0x30
    unsigned int mSlots; // 0x34
    std::vector<Tail *> mTails; // 0x38
    bool mBeard; // 0x44
    int mBeardTick; // 0x48
    float mArrhythmicDurationSeconds; // 0x4c
    bool mInArrhythmic; // 0x50
    int unk_0x58; // 0x54 (copied as a word by operator=)
    float unk_0x40; // 0x58 — retail copies this with lfs/stfs in operator=, so it is a float
    bool mIsCymbalLane; // 0x5c — gem is on a game-cymbal lane
    unsigned int unk_0x44; // 0x60 - some RG chord shape
    unsigned int unk_0x48; // 0x64 - some other RG chord shape
    class String mChordLabel; // 0x68 (String = 0xC: vptr/cap/mStr)
    int mFirstFret; // 0x74
    int mFirstFretString; // 0x78
    bool mIsRepeatChord; // 0x7c — RG repeated chord
    bool mInArpeggio; // 0x7d — gem is inside an arpeggio phrase
    bool mSuppressChordLabel; // 0x7e — hide chord label
    bool mSuppressFretLabel; // 0x7f — hide fret label
    int mFretPos; // 0x80
    bool mSlideUp; // 0x84 — left-hand slide direction (true=up, false=down)
    int mKeyFingerNumber; // 0x88
};
