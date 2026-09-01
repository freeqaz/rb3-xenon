#pragma once
#include "beatmatch/TrackType.h"
#include "beatmatch/GameGem.h"
#include "game/TrainerPanel.h"
#include "math/Mtx.h"

class BandLabel;
class RndDir;
class RndAnimatable;
class RndGroup;
class RndMesh;
class RndTransformable;

// size 0x13c
class TrainerGemTab {
public:
    // size 0x38
    class ExtraTail {
    public:
        Transform mXfm; // 0x0
        int mSlot; // 0x40
        bool mIsRGChord; // 0x44
    };

    TrainerGemTab();
    ~TrainerGemTab();
    void Init(RndDir *, TrackType);
    void SetLefty(bool);
    void Draw(int);
    void SetPattern(const TrainerSection *, const std::vector<GameGem> &);
    int SlotToGemIndex(int) const;
    int GetLane(int) const;
    void DrawStartFinish();
    void DrawExtraTails();
    void Render(int, int, float, float, int);
    void DrawTails(const GameGem &, int, int, float, float);
    void DrawRealGuitarChord(const GameGem &);

    RndDir *mGemTab; // 0x0
    TrackType mTrackType; // 0x4
    int mLanes; // 0x8
    RndAnimatable *mConfigAnim; // 0xc
    RndAnimatable *mVerticalTrans; // 0x10
    Transform mTrans; // 0x14
    RndGroup *mDrawOrderGroup; // 0x54
    int unk48;
    std::vector<GameGem> unk4c;
    const TrainerSection *unk54;
    RndMesh *mGems[9]; // 0x6c
    RndMesh *mTails[5]; // 0x90
    int unk90;
    RndGroup *mTrackGroup; // 0xa8
    RndTransformable *mInstLanes[25]; // 0xac - 0xf8, inclusive
    union {
        struct {
            RndMesh *mGemChord2Lane; // 0xfc
            RndMesh *mGemChord3Lane; // 0x100
            RndMesh *mGemChord4Lane; // 0x104
            RndMesh *mGemChord5Lane; // 0x108
            RndMesh *mGemChord6Lane; // 0x10c
        };
        RndMesh *mGemChordLanes[5]; // 0xfc
    };
    RndMesh *mGemSustainCyan; // 0x124
    BandLabel *mNumLabels[4]; // 0x128, 0x118, 0x11c, 0x120
    BandLabel *mStartLabel; // 0x138
    BandLabel *mFinishLabel; // 0x13c
    float unk12c; // 0x140
    std::vector<ExtraTail> unk130;
    bool mLefty; // 0x150
};
