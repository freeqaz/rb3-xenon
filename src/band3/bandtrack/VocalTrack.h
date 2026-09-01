#pragma once
#include "bandobj/NoteTube.h"
#include "bandobj/TrackInterface.h"
#include "bandobj/VocalTrackDir.h"
#include "bandtrack/Lyric.h"
#include "bandtrack/Track.h"
#include "bandtrack/VocalStyle.h"
#include "beatmatch/VocalNote.h"
#include "game/BandUser.h"
#include "game/TambourineManager.h"
#include "game/VocalPlayer.h"
#include "obj/Data.h"
#include "rndobj/Group.h"
#include <deque>
#include <vector>

class TambourineGem {
public:
    TambourineGem() : unk0(0), unk4(-1), unk8(2) {}

    float Time() const { return unk0; }

    float unk0;
    int unk4;
    int unk8;
};

class TambourineGemPool {
public:
    TambourineGemPool();
    ~TambourineGemPool();
    void FreeUsedGems() {
        while (!mUsedGems.empty()) {
            mFreeGems.push_back(mUsedGems.front());
            mUsedGems.front()->unk8 = 2;
            mUsedGems.pop_front();
        }
    }
    void FreeOldGems(float oldTime);
    void NewGem(float time, int gemIdx);
    void SetTambourineManager(TambourineManager *mgr) { mTambourineManager = mgr; }

    std::deque<TambourineGem *> mFreeGems; // 0x0
    std::deque<TambourineGem *> mUsedGems; // 0x28
    TambourineManager *mTambourineManager; // 0x50
};

class VocalTrack : public Track {
public:
    class LyricShift {
    public:
        LyricShift(float, float);
        LyricShift(float, float, bool);
        float unk0; // the distance the lyric shifts
        float unk4; // the ms at which the lyric shifts
        bool unk8; // gotta go fast or gotta go not fast
    };

    class RangeShift {
    public:
        RangeShift() {}
        float unk0;
        float unk4;
        float unk8;
        float unkc;
        float unk10;
        float unk14;
    };

    VocalTrack(BandUser *);
    virtual ~VocalTrack();
    virtual DataNode Handle(DataArray *, bool);
    virtual void Init();
    virtual void PushGameplayOptions(VocalParam, int);
    virtual bool IsScrolling() const;
    virtual bool InTambourinePhrase() const;
    virtual int IncrementVolume(int);
    virtual bool IsCurrentVocalParam(VocalParam p) { return mCharOptParam == p; }
    virtual void RebuildVocalHUD() { RebuildHUD(); }
    virtual int NumSingers() const;
    virtual bool UseVocalHarmony();
    virtual void SetCanDeploy(bool);
    virtual int GetNumVocalParts();
    virtual bool ShowPitchCorrectionNotice() const;
    virtual void Poll(float);
    virtual void SetDir(RndDir *);
    virtual RndDir *GetDir() { return mDir; }
    virtual BandTrack *GetBandTrack() { return mDir; }
    virtual void SetVocalStyle(VocalStyle);

    void InitPlatePool();
    void DumpAllPlates();
    void DumpPlates(std::deque<TubePlate *> &, const char *);
    void ClearLyrics();
    void ClearMarkers();
    void ClearAllTubePlates();
    void InitPlateList(std::deque<TubePlate *> &, int, int);
    void ReturnFirstMarker();
    void UpdateMarkerVisibility(float, float);
    void InvalidateMarkers(float);
    void UpdateAllTubePlates(float);
    void UpdateTubePlates(std::deque<TubePlate *> &, float, float, bool);
    void ClearTubePlates(std::deque<TubePlate *> &);
    void ResetAllTubePlates();
    void ResetTubePlates(std::deque<TubePlate *> &);
    void HookupTubePlates(NoteTube *);
    TubePlate *GetCurrentPlate(std::deque<TubePlate *> &, int);
    void ResetTimingData();
    void ReadTimingData(const DataArray *);
    void RebuildHUD();
    void JumpReset();
    RndMesh *CreateMarker(Symbol, float, bool);
    void CreateMarkers();
    void ConfigNoteTube(bool, int, int, bool, float);
    LyricPlate *GetNextLyricPlate(std::deque<LyricPlate *> &, bool);
    void DumpLyricPlates(std::deque<LyricPlate *> &, bool);
    void UpdateScrolling(float);
    void UpdateTambourineGems();
    void PollLyricAnimations(std::deque<LyricPlate *> &, float, bool);
    void UpdateLyricZ();
    void PollKaraoke(float);
    void HideCoda();
    bool WantBeatLines(int);
    VocalNoteList *GetVocalNoteList(int);
    void SetAlternateNoteList(int, VocalNoteList *);
    Lyric *GetLastLyric(std::deque<LyricPlate *> &);
    Lyric *GetLastBakedLyric(std::deque<LyricPlate *> &);
    void OnPhraseComplete(float, float, int);
    void BuildPhrase(float, float);
    void HitTambourineGem(int);
    void MissTambourineGem(int, bool);
    void Restart(VocalPlayer *, float, float);
    void UpdateVocalStyle();
    void StartUpdateArrows();
    void UpdatePitchArrow(float, int);
    void UpdateUnusedArrows();
    float GetHarmonyScore(int);
    float GetBottomDisplayPitch() const;
    float GetTopDisplayPitch() const;
    bool
    CheckDeploySections(Lyric *, float, int &, const std::vector<std::pair<float, float> > &, bool, Lyric *, float &);
    bool IdenticalLyric(const VocalNote &, const VocalNote &) const;
    void
    BuildStaticDeployZone(int, const std::pair<float, float> &, float, float &, std::deque<LyricShift> &);
    void BuildScrollingDeployZone(int, const std::pair<float, float> &);
    void BuildScrollingDeployZones(float);
    void PrepareNoteTubes(float, int, int &, int);
    void
    ProcessStaticLyrics(bool, Lyric *, float &, float &, Lyric *&, Lyric *&, float &, bool, LyricPlate *);
    Lyric *CreateLyric(const VocalNote *&, const std::vector<VocalNote> &, bool, bool, bool);

    VocalTrackDir *GetVocalTrackDir() const { return mDir; }

    DataNode OnGetDisplayMode(const DataArray *);
    DataNode OnSetDisplayMode(const DataArray *);

    bool unk68; // 0x78
    VocalStyle mVocalStyleOverride; // 0x7c
    int unk70; // 0x80
    float unk74; // 0x84
    float unk78; // 0x88
    int unk7c; // 0x8c
    ObjPtr<VocalTrackDir> mDir; // 0x90
    ObjPtr<VocalPlayer> mPlayer; // 0x9c
    std::deque<LyricPlate *> mLyricsLead; // 0xa8
    std::deque<LyricPlate *> mLyricsHarmony; // 0xd0
    float mPhraseStartMs; // 0xf8
    float mPhraseEndMs; // 0xfc
    float mNextPhraseEndMs; // 0x100
    int unkf4;
    int unkf8;
    int unkfc;
    int unk100;
    int unk104;
    int unk108;
    int mNextScrollNote[3]; // 0x11c
    int mNextDeployZone[2]; // 0x128
    int mCurLyricPhrase[2]; // 0x130
    bool unk128; // 0x138
    std::vector<std::deque<TubePlate *> > mFrontTubePlates; // 0x13c
    std::vector<std::deque<TubePlate *> > mBackTubePlates; // 0x148
    std::vector<std::deque<TubePlate *> > mPhonemeTubePlates; // 0x154
    std::deque<TubePlate *> mLeadDeployPlates; // 0x160
    std::deque<TubePlate *> mHarmonyDeployPlates; // 0x188
    std::vector<RndMesh *> mMeshPool; // 0x1b0
    int unk19c; // 0x1bc
    std::deque<std::pair<RndMesh *, float> > unk1a0; // 0x1c0
    ObjPtr<RndGroup> unk1c8; // 0x1e8
    TambourineGemPool *mTambourineGemPool; // 0x1f4
    std::deque<TambourineGem *> mTambourineGems; // 0x1f8
    VocalParam mCharOptParam; // 0x220 - vocal param
    int mCharOptMicID; // 0x224
    int unk208; // 0x228
    int unk20c; // 0x22c
    int unk210; // 0x230
    std::deque<RangeShift> mRangeShifts; // 0x234
    float unk23c; // 0x25c
    float unk240; // 0x260
    std::deque<LyricShift> mLeadLyricShifts; // 0x264
    std::deque<LyricShift> mHarmonyLyricShifts; // 0x28c
    float unk294;
    float unk298;
    float unk29c;
    float unk2a0;
    float unk2a4;
    float unk2a8;
    float unk2ac;
    float unk2b0;
    VocalNoteList *mAlternateNoteList[3]; // 0x2d4
    float mStaticDeployZoneXSize; // 0x2e0
    float mStaticDeployBufferX; // 0x2e4
    float mStaticDeployMarginX; // 0x2e8
    float mLyricShiftMs; // 0x2ec
    float mLyricShiftQuickMs; // 0x2f0
    float mLyricShiftAnticipationMs; // 0x2f4
    float mMinLyricHighlightMs; // 0x2f8
    float mMinPhraseHighlightMs; // 0x2fc
    float mLyricOverlapWindowMs; // 0x300
    bool unk2e4;
    bool unk2e5;
    NoteTube *mNoteTube; // 0x308
    bool unk2ec;
};
