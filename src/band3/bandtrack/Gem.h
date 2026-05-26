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

    GemManager *mGemManager; // 0x0
    const GameGem &mGameGem; // 0x4
    std::set<TrackWidget *> mWidgets; // 0x8
    float mStart; // 0x20
    float mEnd; // 0x24
    float mTailStart; // 0x28
    unsigned int mSlots; // 0x2c
    std::vector<Tail *> mTails; // 0x30
    int mBeardTick; // 0x38
    float mArrhythmicDurationSeconds; // 0x3c
    float unk_0x40; // 0x40
    unsigned int unk_0x44; // 0x44 - some RG chord shape
    unsigned int unk_0x48; // 0x48 - some other RG chord shape
    class String mChordLabel; // 0x4c
    signed char unk_0x58; // 0x58
    int mFirstFret; // 0x5c
    int mFirstFretString; // 0x60
    char mFretPos; // 0x64
    char mKeyFingerNumber; // 0x65
    bool mHit : 1;
    bool mMissed : 1;
    bool mReleased : 1;
    bool mHopo : 1;
    bool mInvisible : 1;
    bool mBeard : 1;
    bool mInArrhythmic : 1;
    bool mIsCymbalLane : 1; // 0x66 bit 0 — gem is on a game-cymbal lane (was unk_0x66_7)
    // byte 0x67 bitfield (MSB-first declaration order; first declared = bit 7)
    bool mIsRepeatChord : 1; // 0x67 bit 7 — RG repeated chord (shows repeat indicator)
    bool mInArpeggio : 1;    // 0x67 bit 6 — gem is inside an arpeggio phrase (shows section indicator)
    bool mSuppressChordLabel : 1; // 0x67 bit 5 — hide chord label (arpeggio context)
    bool mSuppressFretLabel : 1;  // 0x67 bit 4 — hide fret label (arpeggio context)
    bool mSlideUp : 1;       // 0x67 bit 3 — left-hand slide direction (true=up, false=down)
    bool unk_0x67_5 : 1;     // 0x67 bit 2
    bool unk_0x67_6 : 1;     // 0x67 bit 1
    bool unk_0x67_7 : 1;     // 0x67 bit 0
};
