#include "beatmatch/DrumFillTrackWatcherImpl.h"
#include "beatmatch/BeatMatchControllerSink.h"
#include "beatmatch/BeatMatchSink.h"
#include "beatmatch/BeatMatchUtl.h"
#include "beatmatch/SongData.h"
#include "utl/Std.h"

DrumFillTrackWatcherImpl::DrumFillTrackWatcherImpl(
    int track,
    const UserGuid &u,
    int slot,
    SongData *song_data,
    GameGemList *gemlist,
    TrackWatcherParent *parent,
    DataArray *cfg
)
    : DrumTrackWatcherImpl(track, u, slot, song_data, gemlist, parent, cfg),
      mFillInfo(mSongData->GetDrumFillInfo(track)), mFillNotes(0) {}

DrumFillTrackWatcherImpl::~DrumFillTrackWatcherImpl() {}

void DrumFillTrackWatcherImpl::CodaSwing(int i1, int i2) { FillSwing(i1, i2, -1, false); }

void DrumFillTrackWatcherImpl::FillSwing(int i1, int i2, int i3, bool b4) {
    if (i3 > -1 && mSongData->GetFakeHitGemsInFill()) {
        FakeHitGem(mParent->GetNow(), i3, kGemHitFlagNone);
    }
    int lanes = mFillInfo->LanesAt(i1);
    if (lanes & (1 << i2)) {
        mFillNotes++;
        int bucket = mParent->GetVelocityBucket(i2);
        int slot = b4 && i2 == 4 ? 4 : mParent->GetVirtualSlot(i2);
        // NOT a `float db = ...;` temp (which is what the rb3-Wii DEV oracle has,
        // verbatim).  Retail evaluates the CALLEE expression before the argument:
        // 0x82xxxx loads mParent (0x20) and its vptr *before* `bl
        // VelocityBucketToDb`, then reloads mParent for `this` -- i.e. mParent is
        // read TWICE (that extra load is the target's +4 bytes over the temp
        // spelling).  Only a single full-expression produces that order; hoisting
        // the result into a temp sinks the whole vtable lookup past the call and
        // cascades into an r28<->r30 swap that reads as a regalloc defect.
        mParent->PlayDrum(slot, true, VelocityBucketToDb(bucket));
        FOREACH (it, mSinks) {
            (*it)->FillSwing(Track(), mFillNotes, i2, i1, false);
        }
    } else {
        FOREACH (it, mSinks) {
            (*it)->FillSwing(Track(), mFillNotes, i2, i1, false);
        }
    }
}

void DrumFillTrackWatcherImpl::ResetFill() {
    if (mFillNotes != 0) {
        FOREACH (it, mSinks) {
            (*it)->FillReset();
        }
        mFillNotes = 0;
    }
}

void DrumFillTrackWatcherImpl::RegisterFill(int i1) {
    FOREACH (it, mSinks) {
        (*it)->FillComplete(mFillNotes, i1);
    }
    ResetFill();
}
