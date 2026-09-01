#pragma once
#include "ProTrainerPanel.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/RGGemMatcher.h"
#include "game/FretHand.h"
#include "game/RGTutor.h"

class RGTrainerPanel : public ProTrainerPanel {
public:
    class FingerStep {
    public:
        int mFinger; // 0x0
        int mFret; // 0x4
        int mLowString; // 0x8
        int mHighString; // 0xc
        bool unk10; // 0x10
    };

    RGTrainerPanel();
    OBJ_CLASSNAME(RGTrainerPanel);
    OBJ_SET_TYPE(RGTrainerPanel);
    static Hmx::Object *NewObject();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~RGTrainerPanel();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual void StartSectionImpl();
    virtual bool IsSongSectionComplete(BandProfile *, int, Difficulty, int);
    virtual void NewDifficulty(int, int);
    virtual void HitNotify(int);
    virtual bool MissNotify(int);
    virtual void Looped();
    virtual bool ShouldDrawTab() const;
    virtual void PostCopyGems();
    virtual void SetSongSectionComplete(BandProfile *, int, Difficulty, int);

    void SetupIsBass();
    void SetLegendModeImpl(bool);
    void HandleChordLegend(bool);
    void StrumString(int);
    void SetFret(int, int);
    void FretButtonDown(int);
    void SetLegendMode(bool);
    bool GetLegendMode() const;
    void SetLegendGemID(int);
    void PickFretboardView(const GameGem &);
    void InitFretSteps(const GameGem &);
    void UpdateStepText(int, FingerStep &);
    Symbol RGStringToken(int, bool);
    void HandleLegendLefty(bool);
    bool TestFingers(const GameGem &);
    void Swing(int);
    void FretButtonUp(int);
    int GetFret(int, int) const;

    std::vector<FingerStep> mFingerSteps; // 0x100
    bool mLegendMode; // 0x10c
    bool unke5; // 0x10d
    int mLegendGemID; // 0x110
    float unkec; // 0x114
    bool mLefty; // 0x118
    RGGemMatcher mMatcher; // 0x11c
    RGTutor mTutor; // 0x224
    RndDir *mChordLegend; // 0x240
    FretHand mFretHand; // 0x244
    bool mIsBass; // 0x274
};

extern RGTrainerPanel *TheRGTrainerPanel;