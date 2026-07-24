#include "beatmatch/GameGemList.h"
#include "beatmatch/BeatMatchUtl.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include <algorithm>
#include <math.h>

// STLport-only template specializations of internal sort helpers
// (`__unguarded_partition`/`__unguarded_linear_insert`/`__introsort_loop`).
// libstdc++/libc++ don't expose these as namespace-level identifiers and use
// a different internal sort, so the asm-match block below isn't reachable on
// native. Native uses std::sort with the default `<` operator on GameGem.
#ifndef HX_NATIVE
namespace stlpmtx_std {

template <>
inline less<GameGem> __less<GameGem>(GameGem*) { return less<GameGem>(); }

template <>
GameGem *
__unguarded_partition<GameGem *, GameGem, less<GameGem> >(
    GameGem *__first,
    GameGem *__last,
    GameGem __pivot,
    less<GameGem>
) {
    for (;;) {
        while (__first->mMs < __pivot.mMs)
            ++__first;
        --__last;
        while (__pivot.mMs < __last->mMs)
            --__last;
        if (!(__first < __last))
            return __first;
        iter_swap(__first, __last);
        ++__first;
    }
}

template <>
void __unguarded_linear_insert<GameGem *, GameGem, less<GameGem> >(
    GameGem *__last,
    GameGem __val,
    less<GameGem>
) {
    GameGem *__next = __last;
    --__next;
    while (__val.mMs < __next->mMs) {
        *__last = *__next;
        __last = __next;
        --__next;
    }
    *__last = __val;
}

template <>
void __introsort_loop<GameGem *, GameGem, long, less<GameGem> >(
    GameGem *__first,
    GameGem *__last,
    GameGem *,
    long __depth_limit,
    less<GameGem> __comp
) {
    while (__last - __first > 16) {
        if (__depth_limit == 0) {
            partial_sort(__first, __last, __last, __comp);
            return;
        }
        ptrdiff_t __len = __last - __first;
        float __a = __first->mMs;
        --__depth_limit;
        GameGem *__mid = __first + __len / 2;
        float __b = __mid->mMs;
        GameGem *__pivot_ptr;
        if (__a < __b) {
            float __c = (__last - 1)->mMs;
            if (__b < __c)
                __pivot_ptr = __mid;
            else if (__a < __c)
                __pivot_ptr = __last - 1;
            else
                __pivot_ptr = __first;
        } else {
            float __c = (__last - 1)->mMs;
            if (__a < __c)
                __pivot_ptr = __first;
            else if (__b < __c)
                __pivot_ptr = __last - 1;
            else
                __pivot_ptr = __mid;
        }
        GameGem *__cut = __unguarded_partition(__first, __last, *__pivot_ptr, __comp);
        __introsort_loop(__cut, __last, (GameGem *)0, __depth_limit, __comp);
        __last = __cut;
    }
}

} // namespace stlpmtx_std
#endif // !HX_NATIVE

bool GameGemTickCmp(const GameGem &gem, int tick);

GameGemList::GameGemList(int thresh) : mHopoThreshold(thresh) {}

void GameGemList::Clear() { mGems.clear(); }

void GameGemList::CopyFrom(const GameGemList *gList) {
    mGems.clear();
    mGems.reserve(gList->mGems.size());
    mGems.insert(mGems.begin(), gList->mGems.begin(), gList->mGems.end());
}

bool GameGemList::AddMultiGem(const MultiGemInfo &info) {
    return AddGameGem(GameGem(info), info.no_strum);
}

bool GameGemList::AddRGGem(const RGGemInfo &info) {
    return AddGameGem(GameGem(info), info.no_strum);
}

int GameGemList::ClosestMarkerIdx(float ms) const {
    const GameGem *it = std::lower_bound(mGems.begin(), mGems.end(), ms, GameGemCmp);
    if (it == mGems.begin())
        return 0;
    if (it == mGems.end())
        return mGems.size() - 1;
    MILO_ASSERT(ms <= it->GetMs(), 0x83);
    const GameGem *prev_it = it - 1;
    MILO_ASSERT(ms >= prev_it->GetMs(), 0x84);
    if (fabsf(ms - prev_it->mMs) < fabsf(ms - it->mMs))
        it--;
    return it - mGems.begin();
}

int GameGemList::ClosestMarkerIdxAtOrAfter(float f) const {
    const GameGem *theGem = std::lower_bound(mGems.begin(), mGems.end(), f, GameGemCmp);
    if (theGem == mGems.begin())
        return 0;
    if (theGem == mGems.end())
        return -1;
    return theGem - mGems.begin();
}

int GameGemList::ClosestMarkerIdxAtOrAfterTick(int tick) const {
    const GameGem *theGem =
        std::lower_bound(mGems.begin(), mGems.end(), tick, GameGemTickCmp);
    if (theGem == mGems.begin())
        return 0;
    if (theGem == mGems.end())
        return -1;
    return theGem - mGems.begin();
}

bool GameGemCmp(const GameGem &gem, float ms) { return gem.mMs < ms; }

bool GameGemTickCmp(const GameGem &gem, int tick) { return gem.mTick < tick; }

float GameGemList::TimeAt(int idx) const {
    MILO_ASSERT(idx < mGems.size(), 0xA5);
    return mGems[idx].mMs;
}

float GameGemList::TimeAtNext(int idx) const {
    MILO_ASSERT(idx < mGems.size(), 0xAC);
    if (idx + 1 == mGems.size()) {
        float ms = mGems[idx].mMs;
        return 4000.0f + ms;
    }
    return mGems[idx + 1].mMs;
}

void GameGemList::RecalculateGemTimes(TempoMap *tmap) {
    for (std::vector<GameGem>::iterator it = mGems.begin(); it != mGems.end(); it++) {
        it->RecalculateTimes(tmap);
    }
    std::sort(mGems.begin(), mGems.end());
}

void GameGemList::SetGems(
    int startTick, int loopStartTick, int loopEndTick,
    const std::vector<GameGem> &gems, int numLoops) {
    mGems.clear();
    mGems.reserve(numLoops * gems.size());
    int tickShift = loopStartTick - startTick;
    int loopLen = loopEndTick - loopStartTick;
    int tickOffset = 0;
    for (int i = 0; i < numLoops; i++) {
        for (unsigned int j = 0; j < gems.size(); j++) {
            GameGem gem = gems[j];
            gem.CopyGem((GameGem *)&gems[j], tickShift + tickOffset);
            mGems.push_back(gem);
        }
        tickOffset += loopLen;
    }
}

void GameGemList::MergeChordGems() {
    if (mGems.empty())
        return;
    std::vector<GameGem> merged;
    std::vector<GameGem>::iterator it = mGems.begin();
    while (it != mGems.end()) {
        int tick = it->mTick;
        int durTicks = it->mDurationTicks;
        GameGem chord = *it;
        bool keep = true;
        std::vector<GameGem>::iterator next = it + 1;
        while (next != mGems.end() && abs(next->mTick - tick) < 10) {
            if (abs(next->mDurationTicks - durTicks) < 10) {
                chord.mSlots |= next->mSlots;
            } else {
                keep = false;
            }
            next++;
        }
        if (keep) {
            merged.push_back(chord);
            it = next;
        } else {
            for (; it != next; it++) {
                merged.push_back(*it);
            }
        }
    }
    mGems = merged;
}

bool GameGemList::AddGameGem(const GameGem &gem, NoStrumState noStrum) {
    // Retail folds this game-code guard to the empty no-arg RAII form (see
    // MemMgr.h). Native's ctor defaults to (true,false), so the no-arg spelling
    // is identical there — keeps both the X360 match and HX_NATIVE correct.
    MemDoTempAllocations tmp;
    if (!mGems.empty()) {
        const GameGem &last = mGems.back();
        if (last.mMs > gem.mMs) {
            mGems.insert(
                std::lower_bound(mGems.begin(), mGems.end(), gem, GameGem::CompareTimes),
                gem);
            return true;
        }
        if (last.mTick != gem.mTick && last.mTick + 10 >= gem.mTick) {
            return false;
        }
    }
    if (noStrum == kStrumDefault) {
        bool willBeNoStrum = WillBeNoStrum(gem);
        mGems.push_back(gem);
        mGems.back().mForceStrum = willBeNoStrum;
    } else {
        mGems.push_back(gem);
    }
    return true;
}

void GameGemList::Finalize() {
    std::vector<GameGem>(mGems).swap(mGems);
}

bool GameGemList::WillBeNoStrum(const GameGem &gem) {
    if (gem.IsRealGuitar() && gem.RightHandTap())
        return true;
    if (mGems.empty() || gem.mTick - mGems.back().mTick > mHopoThreshold)
        return false;
    if (gem.IsRealGuitar()) {
        const GameGem &last = mGems.back();
        if (last.IsMuted())
            return false;
        if (gem.GetNumStrings() == 1 && last.GetNumStrings() == 1) {
            int str = gem.GetLowestString();
            if (str == (int)last.GetLowestString()) {
                return gem.GetFret(str) != last.GetFret(str);
            }
        }
        return false;
    }
    return !(gem.mSlots & mGems.back().mSlots) && GemNumSlots(gem.mSlots) == 1;
}

void GameGemList::Reset() {
    for (std::vector<GameGem>::iterator it = mGems.begin(); it != mGems.end(); it++) {
        it->mPlayed = false;
        it->unk10b1 = false;
    }
}