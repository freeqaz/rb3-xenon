#pragma once

// Retail RB3-360 SongPos is 0x14 — NO mPhrase (proven 2026-06-11: retail
// Performer ctor fn_8267F0F0 zero-inits SongPos at +0x0/+0x8/+0xc/+0x10,
// skipping +0x4, and Performer::mQuarantined sits at 0x230 = 0x21c + 0x14).
// DC3 later inserted int mPhrase @ 0x8 (size 0x18). Keep the DC3 form only
// behind SONGPOS_DC3_PHRASE (BeatClock.cpp, currently compiled nowhere).
// See docs/decomp/research/2026-06-11-player-plus4-layout.md.
class SongPos {
private:
    float mTotalTick; // 0x0
    float mTotalBeat; // 0x4
#ifdef SONGPOS_DC3_PHRASE
    int mPhrase;
#endif
    int mMeasure; // 0x8
    int mBeat; // 0xc
    int mTick; // 0x10
public:
    // NOTE: retail/Wii default ctor deliberately does NOT init mTotalBeat.
    SongPos() : mTotalTick(0), mMeasure(0), mBeat(0), mTick(0) {}
    SongPos(float totalTick, float totalBeat, int measure, int beat, int tick)
        : mTotalTick(totalTick), mTotalBeat(totalBeat), mMeasure(measure),
          mBeat(beat), mTick(tick) {}
    float GetTotalTick() const { return mTotalTick; }
    float GetTotalBeat() const { return mTotalBeat; }
    int GetMeasure() const { return mMeasure; }
    int GetBeat() const { return mBeat; }
    int GetTick() const { return mTick; }

    int &AccessMeasure() { return mMeasure; }
    int &AccessBeat() { return mBeat; }
    int &AccessTick() { return mTick; }
    float &AccessTotalTick() { return mTotalTick; }
    float &AccessTotalBeat() { return mTotalBeat; }
#ifdef SONGPOS_DC3_PHRASE
    int GetPhrase() const { return mPhrase; }
    int &AccessPhrase() { return mPhrase; }
#endif
};
