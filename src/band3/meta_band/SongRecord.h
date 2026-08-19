#pragma once
#include "BandProfile.h"
#include "game/Defines.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/SavedSetlist.h"
#include "obj/Object.h"

class SongRecord : public Hmx::Object {
public:
    SongRecord(const BandSongMetadata *);
    virtual ~SongRecord() {}
    virtual DataNode Handle(DataArray *, bool);

    bool UpdateScoreType();
    bool UpdateSharedStatus();
    bool UpdatePerformanceData();
    bool UpdateReview();
    bool UpdateRestricted();
    /** Retail 0x825BAE40 -- the sixth member of the Update* family, absent from
        BOTH oracles (rb3-Wii and DC3 declare only the five above). Decoded off
        retail bytes; the body is unambiguous:

            bool old = mDemo;                                // lbz r30,0x2d(r3)
            mDemo = TheSongMgr.IsDemo(mData->ID());          // 0x108 -> ID(), IsDemo
            if (mDemo == old) return false;                  // cmplw / beq
            UpdatePerformanceData();                         // bl 0x825BA6C8
            return true;

        Every callee is map-named: 0x82575F68 = ?IsDemo@BandSongMgr@@QBA_NH@Z,
        0x825BA6C8 = ?UpdatePerformanceData@SongRecord@@QAA_NXZ, and the global it
        loads (0x82C72BA8) is ?TheSongMgrPtr@@3PAVBandSongMgr@@A. The store is to
        +0x2d, which /d1reportSingleClassLayoutSongRecord puts at mDemo -- so this
        is the mDemo analogue of UpdateSharedStatus (0x2c) / UpdateRestricted
        (0x2e).

        ⚠ THE NAME IS INFERRED, NOT RECOVERED. 0x825BAE40 is absent from
        target_symbol_map.json and neither oracle has the method, so no oracle can
        supply the real spelling; `UpdateDemo` is chosen to parallel the family.
        The name is score-invisible either way (a `bl` target is a relocation
        argument, which the default ruler masks), so this costs nothing if wrong.

        DECLARED, NOT DEFINED -- the MusicLibraryUnkOp / Unk825BC900 house pattern.
        Only the call shape into it is reproduced here, not its body. */
    bool UpdateDemo(); // retail 0x825BAE40
    int GetTier(Symbol) const;
    short GetInstrumentMask(ScoreType) const;
    Symbol GetShortDifficultySym(ScoreType) const;
    int GetScore() const { return mScores[mActiveScoreType]; }
    Symbol GetShortDifficultySym() const {
        return GetShortDifficultySym(mActiveScoreType);
    }
    int GetStars() const { return mStars[mActiveScoreType]; }
    int GetNotesPct() const { return mNotesPct[mActiveScoreType]; }
    short GetInstrumentMask() const { return GetInstrumentMask(mActiveScoreType); }
    int GetReview() const { return mReview; }
    short GetBandInstrumentMask() const { return mBandInstrumentMask; }
    Difficulty GetDifficulty(ScoreType s) const { return mDiffs[s]; }
    const BandSongMetadata *Data() const { return mData; }
    inline const BandSongMetadata *GetData() const { return mData; }
    bool GetRestricted() const { return mRestricted; }
    bool IsNotBand() const { return mActiveScoreType != kScoreBand; }
    bool IsDemo() const { return mDemo; }

    Symbol mShortName; // real 0x28
    // RESOLVED (was: "DO NOT swap ... retail most likely has a FOURTH bool").
    // There is no fourth bool.  Our NAMES for 0x2c and 0x2e were swapped, and
    // every use site inherited the same mis-naming from the oracle -- so the
    // wrong names over the wrong layout cancelled out to correct offsets
    // everywhere EXCEPT the two Update* fns and the ctor, whose identity is
    // pinned by their callee rather than by a name we are free to swap.
    //
    // Map-independent anchor: fn_825BA970 derefs the pointer global
    // TheSessionMgr, loads +0x50 (GetMachineMgr()), calls
    // BandMachineMgr::IsSongShared(id) and stores the bool to 0x2c.  Our
    // UpdateSharedStatus compiles to that body instruction-for-instruction
    // (26/28 equal), and the two-level global->+0x50->call shape is unique to
    // it -- UpdateRestricted is one-level (TheSongMgr).  So 0x2c holds SHARED.
    // Corroborated twice: fn_825BA9E0 (one-level, IsRestricted) stores 0x2e,
    // and the ctor stores 0x2d then 0x2e for mDemo then mRestricted.
    //
    // The "net 0" the prior lane measured was real but mis-read: the two
    // MusicLibrary Rebuild* fns sat at a FALSE 100% (100.0 normalized vs
    // 99.0/99.2 raw -- callee identity masked by functionRelocDiffs=none).
    // Swapping the fields alone breaks them; swapping the fields AND flipping
    // the inherited mis-naming at their use sites keeps them and wins the
    // Update* pair.  See also OwnedSongSortNode::IsEnabled, which becomes
    // semantically sane under the corrected names (not-local && !shared =>
    // disabled; restricted => disabled).
    bool mIsShared; // real 0x2c
    bool mDemo; // real 0x2d
    bool mRestricted; // real 0x2e
    std::map<Symbol, int> mTier; // 0x30
    ScoreType mActiveScoreType; // 0x48
    int mScores[11]; // 0x40
    Difficulty mDiffs[11]; // 0x6c
    int mStars[11]; // 0x98
    int mNotesPct[11]; // 0xc4
    short mBandInstrumentMask; // 0xfc
    int mReview; // 0x100
    int mPlays; // 0x104
    const BandSongMetadata *mData; // 0x108
};

class SetlistRecord : public Hmx::Object {
public:
    SetlistRecord(SavedSetlist *);
    virtual ~SetlistRecord() {}
    virtual DataNode Handle(DataArray *, bool);

    bool IsLocal() const;
    bool IsNetSetlist() const;
    bool IsProfileOwner(const BandProfile *) const;
    const char *GetOwner() const;
    Symbol GetSetlistTypeSym() const;
    bool HasViewableGamercard() const;
    void ViewGamercard(class LocalBandUser *);
    Symbol GetToken() const { return mToken; }
    SavedSetlist *GetSetlist() const { return mSetlist; }

    SavedSetlist *mSetlist; // 0x28
    Symbol mToken; // 0x2c
    bool unk24; // 0x30
    int mID; // 0x34
    int mBattleTimeLeft; // 0x38
    int unk30; // 0x3c
};