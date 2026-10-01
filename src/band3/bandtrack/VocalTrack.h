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
    TambourineGem *NewGem(float time, int gemIdx);
    // W16-HR: TU5 helper, retail fn_82BA29A0 (name ours; no oracle has it).
    TambourineGem *SetGemState(int id, int state);
    void SetTambourineManager(TambourineManager *mgr) { mTambourineManager = mgr; }

    std::deque<TambourineGem *> mFreeGems; // 0x0
    std::deque<TambourineGem *> mUsedGems; // 0x28
    TambourineManager *mTambourineManager; // 0x50
};

class VocalTrack : public Track {
public:
    // W17: unk0's existing comment ("the distance the lyric shifts") does not
    // match usage -- VocalTrack::UpdateScrolling drains this queue and stores
    // unk0 directly as the scroller's local X (`xPos = shift.unk0; pos.x =
    // xPos;`), and lerps toward it (`t*(shift.unk0-xPos)+xPos`) while pending.
    // It is the ABSOLUTE target local X the lyric scroller shifts TO, not a
    // delta. Corrected here from direct source usage (self-derived this pass,
    // not from a prior lane doc).
    class LyricShift {
    public:
        LyricShift(float, float);
        LyricShift(float, float, bool);
        float unk0; // target local X position the lyric scroller shifts to
        float unk4; // the ms at which the lyric shifts
        bool unk8; // gotta go fast or gotta go not fast
    };

    // W17: fields verified against both call sites -- the debug MILO_LOG dump
    // in RebuildHUD ("start ms: %.2f, intro ms: %.2f, min: %.1f -> %.1f, max:
    // %.1f -> %.1f", args unk0/unk14/unk4/unkc/unk8/unk10) and the FROM/TO
    // lerp + SetRange(TO-pair) usage in UpdateScrolling. This also confirms
    // W16-FS's oracle-defect findings #9/#10
    // (docs/decomp/W16FS_UPDATESCROLLING_RETAIL_REDERIVE_2026-09-16.md) --
    // correct field roles and the TO-pair SetRange call are already present
    // in this source. Left un-renamed, comment-only, matching the LyricShift
    // precedent above (a plain nested value-struct local to these two
    // functions).
    class RangeShift {
    public:
        RangeShift() {}
        float unk0; // startMs -- ms at which this range shift becomes due
        float unk4; // rangeMinFrom -- pitch-range min BEFORE the shift (lerp source)
        float unk8; // rangeMaxFrom -- pitch-range max BEFORE the shift (lerp source)
        float unkc; // rangeMinTo -- pitch-range min AFTER the shift (lerp target; SetRange arg 1)
        float unk10; // rangeMaxTo -- pitch-range max AFTER the shift (lerp target; SetRange arg 2)
        float unk14; // introMs -- transition duration in ms
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
    // W17: renamed from unk70 (int, offset compiler-verified unchanged).
    // Tri-state cache for IsScrolling()'s answer -- all 5 usage sites in this
    // file confirmed by grep (ctor init, the two RebuildHUD writes below, the
    // two IsScrolling() reads): 0 = forced not-scrolling (set in RebuildHUD
    // when HasNetPlayer() -- networked vocals never scroll), 2 = unset/defer
    // to mVocalStyleOverride (the ctor default and RebuildHUD's non-net
    // reset). No site in this file was found setting it to 1
    // ("forced scrolling"); left as a plain rename, not investigated further.
    int mScrollOverride; // 0x80
    float unk74; // 0x84 -- W17: locally aliased "trackScale" at the top of
                 // UpdateScrolling (`float trackScale = unk74;`); a ms-time
                 // window mapped across the full track width (see unk78).
                 // Recomputed in UpdateVocalStyle as trackWidth*speed/16.8.
                 // Not renamed here -- used well outside RebuildHUD/
                 // UpdateScrolling, out of this pass's scope.
    float unk78; // 0x88 -- W17: locally aliased "trackWidth" at the top of
                 // UpdateScrolling (`float trackWidth = unk78;`); set in
                 // UpdateVocalStyle as `mDir->mTrackRightX - mDir->mTrackLeftX`.
                 // Not renamed here, same reason as unk74.
    int unk7c; // 0x8c -- W17: zero usage found anywhere in this file besides
               // ctor init (`unk7c(0)`); left unnamed, usage unknown within
               // this file (same "usage unknown here" caveat as Singer.h:184).
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
    // W17: unk104/unk108 are persistent scan cursors in UpdateScrolling --
    // unk108 is the next beat index for the beat/downbeat marker sweep,
    // unk104 the next phrase index for the phrase marker sweep (both reset to
    // 0/1 respectively in RebuildHUD). The beat-marker loop is the rare
    // unrotated "F1" loop shape flagged in W16-FS
    // (docs/decomp/W16FS_UPDATESCROLLING_RETAIL_REDERIVE_2026-09-16.md) as a
    // proven structural wall, not source-fixable by rewording. Not renamed
    // here -- also written/reset by a third function outside this pass's two
    // targets (grep-confirmed), so out of scope for this cleanup.
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
    float unk23c; // 0x25c -- W17: static-deploy margin-X, lead side (seeded
                  // from mStaticDeployMarginX in RebuildHUD; also read as the
                  // "lastLyricX" fallback in the static-lyric path elsewhere
                  // in this file). Not renamed -- used outside this pass's
                  // two target functions.
    float unk240; // 0x260 -- W17: same as unk23c, harmony side.
    std::deque<LyricShift> mLeadLyricShifts; // 0x264
    std::deque<LyricShift> mHarmonyLyricShifts; // 0x28c
    float unk294; // W17: lead-side cached LyricShift::unk0 target X, held
                  // across UpdateScrolling calls for interpolation (`xPos`).
    float unk298; // W17: harmony-side counterpart to unk294.
    float unk29c;
    float unk2a0;
    float unk2a4;
    float unk2a8;
    float unk2ac; // W17: lead-side current on-screen lyric-scroller local X,
                  // i.e. the interpolated/committed shift plus mNowBarX
                  // (`shiftedX` in UpdateScrolling's LyricShift drain).
    float unk2b0; // W17: harmony-side counterpart to unk2ac.
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
