#pragma once
#include "hamobj/DancerSequence.h"
#include "hamobj/Difficulty.h"
#include "hamobj/ErrorNode.h"
#include "hamobj/FilterVersion.h"
#include "hamobj/ScoreUtl.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "rndobj/PropAnim.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

enum MoveMirrored {
    kMirroredNo = 0,
    kMirroredYes = 1,
    kNumMoveMirrored = 2
};

class MoveFrame {
public:
    enum {
        // for DC1
        kNumHam1Nodes = 16
    };
    void Save(BinStream &) const;
    void Load(BinStreamRev &);
    const Ham1NodeWeight &NodeWeightHam1(int, MoveMode, MoveMirrored) const;
    const Ham2FrameWeight &FrameWeight(MoveMirrored) const;
    const Vector3 &NodeWeight(int, MoveMirrored) const;
    const Vector3 &NodeInverseScale(int, MoveMirrored) const;
    void SetNodeScale(int, MoveMirrored, const Vector3 &);
    float QuantizedSeconds(float) const;
    float GetBeat() const { return mBeat; }
    FilterVersionType Version() const {
        int filterMask = (mTypeMask & 0x300000) >> 5;
        return filterMask ? kFilterVersionHam1 : kFilterVersionHam2;
    }
    float Beat() const { return mBeat; }
    int TypeMask() const { return mTypeMask; }

private:
    float mBeat; // 0x0
    int mTypeMask; // 0x4 - flags?
    Ham1NodeWeight mHam1NodeWeights[kNumMoveModes][kNumMoveMirrored][kNumHam1Nodes]; // 0x8
    Vector3 mNodeWeights[kNumMoveMirrored][kMaxNumErrorNodes]; // 0x508
    Vector3 mNodeScales[kNumMoveMirrored][kMaxNumErrorNodes]; // 0x928
    Vector3 mNodesInverseScale[kNumMoveMirrored][kMaxNumErrorNodes]; // 0xd48
    Ham2FrameWeight mFrameWeights[kNumMoveMirrored]; // 0x1168
};

/** "Data associated with a ham Move" */
class HamMove : public RndPropAnim {
public:
    // Retail RB3 (Xbox 360) layout: sizeof == 0x54 (84 bytes), NOT DC3's stripped
    // 0x10. Recovered from objdiff of the vector<LocalizedName> ops: element
    // stride is 0x54 (li r30,0x54 / addi ...,0x54) and elements are copied via a
    // *non-trivial* copy ctor `bl` (not inlined). Retail's element copy ctor
    // (ICF-folded w/ SampleZone's, fn 0x827139b0) shows the shape: a String
    // sub-object at offset 0x0 (12B: vptr/mCap/mStr, copied by String's copy ctor
    // fn 0x826FCE18), then a trivially-copied tail out to 0x54. Semantic fields
    // beyond mName/mLanguage were not individually identified (the accessors are
    // inlined/anonymous in retail); the tail is carried as opaque padding so the
    // size/copy-shape match without inventing member semantics.
    struct LocalizedName {
        bool operator==(const Symbol s) const { return mLanguage == s; }

        /** The move's name, in that language. */
        String mName; // 0x0 (String, 0xC bytes)
        /** The localized name's language. (i.e. eng, esp, fre) */
        Symbol mLanguage; // 0xc
        /** Retail carries ~0x44 more bytes here (fields not individually RE'd). */
        char mPad[0x54 - 0x10]; // 0x10 .. 0x54
    };
    enum TexState {
        kTexNone = 0,
        kTexNormal = 1,
        kTexFlip = 2,
        kTexDblFlip = 3
    };
    // Hmx::Object
    virtual ~HamMove();
    OBJ_CLASSNAME(HamMove);
    OBJ_SET_TYPE(HamMove);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndPropAnim
    virtual void SetFrame(float, float);
    virtual float StartFrame();
    virtual float EndFrame();

    OBJ_MEM_OVERLOAD(0x68)
    NEW_OBJ(HamMove)
    static float sMinFrameDistBeats;

    void SetTexture(RndTex *);
    bool IsRest() const;
    bool IsFinalPose() const;
    bool SuppressGuideGesture() const;
    bool SuppressPracticeOptions() const;
    void RefreshBarks();
    float Confusability(const HamMove *) const;
    const char *DisplayName() const;
    float AdjustNormalizedPercentToConfusability(float, float);
    float ConfusabilityWithMoveDataArray(const DataArray *);
    std::vector<MoveFrame> &GetMoveFrames();
    const std::vector<MoveFrame> &GetMoveFrames() const;
    MoveMirrored Mirrored() const;
    void Update(const HamMove *);
    const FilterVersion *FilterVer() const;
    const std::vector<float> *RatingOverride() const;
    float PSNRThreshold(MoveRating) const;
    FilterVersionType Version() const;
    float PSNRToDetectFrac(float) const;

    bool Scored() const { return mScored; }
    DancerSequence *GetDancerSequence() const { return mDancerSeq; }
    RndTex *Tex() const { return mTex; }
    RndTex *SmallTex() const { return mSmallTex; }

protected:
    HamMove();

    void SyncMirror();
    float FindConfusabilty(const HamMove *) const;
    void SetName(Symbol, const char *);
    bool IsCheatWinning() const;

    /** "Move to mirror" */
    ObjPtr<HamMove> mMirror; // 0x20
    /** "Texture to describe the move" */
    ObjPtr<RndTex> mTex; // 0x2c
    ObjPtr<RndTex> mSmallTex; // 0x38
    /** "Texture state describes how to display the tex" */
    TexState mTexState; // 0x44
    std::vector<MoveFrame> mMoveFrames; // 0x48
    /** "True if this is move is scored.
        False if it's a rest or some kind of indicator (like freestyle)" */
    bool mScored; // 0x54
    /** "True if this move is a paradiddle" */
    bool mParadiddle; // 0x55
    /** "True if this move is the final pose in the song" */
    bool mFinalPose; // 0x56
    /** "Prevent the Guide Gesture from appearing for the duration of this move" */
    bool mSuppressGuide; // 0x57
    /** "Prevent the Practice Options from appearing for the duration of this move" */
    bool mSuppressPracticeOptions; // 0x58
    /** "Prevent this move from appear in the dance battle minigame" */
    bool mOmitMinigame; // 0x59
    std::vector<LocalizedName> mLocalizedNames; // 0x5c
    const char *mDisplayName; // 0x68
    Difficulty mDifficulty; // 0x6c
    Symbol mVerb; // 0x70
    Symbol mMoveSound; // 0x74
    std::vector<float> mRatingStates; // 0x78
    /** "Whether to use shoudler displacements for detection" - specific to Ham1! */
    bool mShoulderDisplacements; // 0x84
    /** "Generated threshold for super perfect"/
        "perfect/flawless"/"awesome/nice"/"ok/almost" */
    float mThresholds[kNumMoveRatings]; // 0x88
    /** "Override threshold for super perfect /
        perfect/flawless / awesome/nice / ok/almost (0 means no override)" */
    float mOverrides[kNumMoveRatings]; // 0x98
    bool mDirty; // 0xa8
    /** "id used when comparing to other moves" */
    Hmx::CRC mConfusabilityID; // 0xac
    std::map<Hmx::CRC, float> mConfusabilities; // 0xb0
    ObjPtr<DancerSequence> mDancerSeq; // 0xc8
};

struct HamMoveKey {
    HamMove *move;
    float beat;
};

struct HamMoveScore {
    HamMove *mMove;
    int mRatingStateIndex; // 0x4
    float mDetectFrac; // 0x8
    bool mSlowMo; // 0xc
};
