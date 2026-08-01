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
    int g = i1;
    int num_unplayed = 0;
    for (; g <= i2; g++) {
        GameGem &gem = mGemList->GetGem(g);
        int slot = gem.GetSlot();
        if (i3 == slot)
            return g;
        if (!gem.GetPlayed())
            num_unplayed++;
    }
    bool choose_any = (num_unplayed == 0);
    int closest_gem = -1;
    int closest_gem_distance = 999;
    for (; i1 <= i2; i1++) {
        GameGem &gem = mGemList->GetGem(i1);
        if (choose_any || !gem.GetPlayed()) {
            int absval = abs(i3 - gem.GetSlot());
            if (absval < closest_gem_distance) {
                closest_gem_distance = absval;
                closest_gem = i1;
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
    if ((1 << gem.GetSlot() & mGameCymbalLanes)
        && (gem.IsCymbal() != (unsigned int)(flags >> 2 & 1)))
        return false;
    else
        return true;
}
