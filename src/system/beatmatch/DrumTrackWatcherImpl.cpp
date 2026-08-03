#include "beatmatch/DrumTrackWatcherImpl.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/SongData.h"
#include "beatmatch/TrackWatcherParent.h"
#include <algorithm>

extern bool gKickAutoplay;

DrumTrackWatcherImpl::DrumTrackWatcherImpl(
    int track,
    const UserGuid &u,
    int slot,
    SongData *song_data,
    GameGemList *gemlist,
    TrackWatcherParent *parent,
    DataArray *cfg
)
    : TrackWatcherImpl(track, u, slot, song_data, gemlist, parent, cfg, 2),
      mNextKickGemToAutoplay(0), mGameCymbalLanes(0) {
    MILO_ASSERT(song_data, 0x25);
    mGameCymbalLanes = song_data->GetGameCymbalLanes();
}

DrumTrackWatcherImpl::~DrumTrackWatcherImpl() {}

void DrumTrackWatcherImpl::Restart() {
    MILO_ASSERT(mSongData, 0x2F);
    mGameCymbalLanes = mSongData->GetGameCymbalLanes();
}

int DrumTrackWatcherImpl::RelevantGem(int i1, int i2, int i3) {
    // Declaration order is codegen-load-bearing: retail initialises
    // num_unplayed (`li r7,0`) BEFORE copying i1 into g (`mr r8,r4`).
    int num_unplayed = 0;
    int g = i1;
    for (; g <= i2; g++) {
        GameGem &gem = mGemList->GetGem(g);
        int slot = gem.GetSlot();
        if (slot == i3)
            return g;
        if (!gem.GetPlayed())
            num_unplayed++;
    }
    bool choose_any = (num_unplayed == 0);
    int closest_gem = -1;
    int closest_gem_distance = 999;
    // Second loop takes its OWN index copied from i1 rather than mutating the
    // parameter (which is what the rb3-Wii DEV oracle does, verbatim).  Retail
    // emits `mr r8, r4` TWICE -- once per loop -- and that second copy only
    // exists if loop 2 has its own variable; mutating i1 in place pins the
    // parameter's register for the whole loop, which pushes `this` out of r3
    // into r7 (an extra `mr r7, r3` in the prologue) and cascades into a
    // 25-instruction regalloc divergence that reads as a permuter-class defect.
    int j = i1;
    for (; j <= i2; j++) {
        GameGem &gem = mGemList->GetGem(j);
        if (choose_any || !gem.GetPlayed()) {
            int absval = abs(i3 - gem.GetSlot());
            if (absval < closest_gem_distance) {
                closest_gem_distance = absval;
                closest_gem = j;
            }
        }
    }
    MILO_ASSERT(closest_gem != -1, 0x59);
    return closest_gem;
}

bool DrumTrackWatcherImpl::Swing(int slot, bool b1, bool b2, GemHitFlags flags) {
    KillSustainForSlot(slot);
    float now = mParent->GetNow();
    int idx = mGemList->ClosestMarkerIdx(now + mSyncOffset);
    float timeat = mGemList->TimeAt(idx);
    int i3 = idx;
    for (; i3 + 1 < mGemList->NumGems() && mGemList->TimeAt(i3 + 1) <= timeat + 20.0f;
         i3++)
        ;
    while (idx - 1 >= 0 && mGemList->TimeAt(idx - 1) >= timeat - 20.0f) {
        idx--;
    }
    int relevant = RelevantGem(idx, i3, slot);
    bool inslop = InSlopWindow(mGemList->TimeAt(relevant), now);
    int _tmp0 = mGemList->GetGem(relevant).GetTick();
    unsigned int mask = 1 << slot;
    NoteSwing(mask, _tmp0);
    if (inslop) {
        GameGem &gem = mGemList->GetGem(relevant);
        if (!gem.GetPlayed() && Playable(relevant)) {
            MILO_ASSERT(gem.NumSlots() == 1, 0x8D);
            if (slot == gem.GetSlot()) {
                if (CheckCymbal(gem, flags))
                    OnHit(now, slot, relevant, gem.GetSlots(), flags);
                else
                    OnMiss(now, slot, relevant, mask, kGemHitFlagNone);
            } else {
                if (b2)
                    return false;
                OnMiss(now, slot, relevant, mask, kGemHitFlagNone);
            }
        } else {
            if (b2)
                return false;
            OnMiss(now, slot, relevant, mask, kGemHitFlagNone);
        }
    } else {
        if (b2)
            return false;
        OnMiss(now, slot, relevant, mask, kGemHitFlagNone);
    }
    return true;
}

int DrumTrackWatcherImpl::NextGemAfter(int gem_id, bool timeout) {
    int slot = mGemList->GetGem(gem_id).GetSlot();
    int last_tick = mGemList->GetGem(gem_id).GetTick();
    int num_skips = 0;
    for (int i = gem_id + 1; i < mGemList->NumGems(); i++) {
        int tick = mGemList->GetGem(i).GetTick();
        if (timeout && tick - last_tick > 10) {
            num_skips++;
            last_tick = tick;
            if (num_skips == 2) {
                int next = gem_id + 1;
                return std::min(mGemList->NumGems() - 1, next);
            }
        }
        if (mGemList->GetGem(i).GetSlot() == slot)
            return i;
    }
    return -1;
}

void DrumTrackWatcherImpl::PollHook(float f) { CheckForKickAutoplay(f); }

void DrumTrackWatcherImpl::JumpHook(float f) {
    int idx = mGemList->ClosestMarkerIdxAtOrAfter(f + mSyncOffset);
    if (idx == -1)
        idx = mGemList->NumGems();
    mNextKickGemToAutoplay = idx;
}

void DrumTrackWatcherImpl::CheckForKickAutoplay(float f) {
    if (gKickAutoplay && mIsCurrentTrack && !IsCheating()) {
        float offset = f + mSyncOffset;
        int cap = mGemList->NumGems() - 1;
        while (mNextKickGemToAutoplay <= cap) {
            int i8 = mNextKickGemToAutoplay;
            float timeAt = mGemList->TimeAt(i8);
            GameGem &curGem = mGemList->GetGem(i8);
            int slot = curGem.GetSlot();
            if (offset >= timeAt) {
                if (Playable(i8) && slot == 0) {
                    if (!mParent->InCodaFreestyle(curGem.GetTick(), true)) {
                        if (!mParent->InFill(curGem.GetTick(), true)) {
                            HitGem(f, i8, curGem.GetSlots(), kGemHitFlagNone);
                        }
                    }
                }
                mNextKickGemToAutoplay++;
            } else
                break;
        }
    }
}

bool DrumTrackWatcherImpl::CheckCymbal(const GameGem &gem, GemHitFlags flags) const {
    // `!= (bool)(flags & kGemHitFlagCymbal)`, NOT the rb3-Wii DEV oracle's
    // `!= (unsigned int)(flags >> 2 & 1)`.  Retail normalises EACH side to a
    // single bit (`extrwi ...,1,29` / `extrwi ...,1,27`) and compares them with
    // `cmplw cr6` + `bnelr cr6` -- a genuine bool==bool.  Any int-typed spelling
    // lets MSVC align the two bit positions and fuse them into `xor` + a
    // record-form bit test, which is 5 instructions wrong and one byte-length
    // too long (84.55% -> 85.9%); the bool cast reaches 99.5%.
    //
    // RESIDUAL (1 instruction, lane DI-2/C): target `and. r11, r11, r10`
    // (rA = the `1<<slot` shift), we emit `and. r11, r10, r11` (rA = the
    // mGameCymbalLanes load).  MSVC canonicalises this AND's operand order and
    // it is NOT source-steerable -- five spellings all compile byte-identical:
    //   `1 << gem.GetSlot() & mGameCymbalLanes`   (this, = the oracle)
    //   `mGameCymbalLanes & 1 << gem.GetSlot()`   (operands swapped)
    //   `1U << gem.GetSlot() & mGameCymbalLanes`  (unsigned shift)
    //   both swap variants with the slot hoisted into a local first
    // Note the SAME `(1<<i) & member` shape inside the inlined GameGem::GetSlot
    // just above (instr 4) DOES match as `and. rD, slw, mem`, so MSVC can emit
    // retail's order -- the inversion is specific to this outer expression.
    if ((1 << gem.GetSlot() & mGameCymbalLanes)
        && (gem.IsCymbal() != (bool)(flags & kGemHitFlagCymbal)))
        return false;
    else
        return true;
}
