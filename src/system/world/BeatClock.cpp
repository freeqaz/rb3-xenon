#include "world/BeatClock.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Poll.h"
#include "utl/MeasureMap.h"
#include <cmath>

BeatClock::BeatClock()
    : mMeasureMap(new MeasureMap()), mSound(this), mBeatsPerMinute(100),
      mBeatsPerMeasure(4), mMeasuresPerPhrase(0), mUseGlobal(0), mTotalSeconds(0),
      unk50(0), mIsRunning(0), mTimeline(kTaskSeconds) {}

BeatClock::~BeatClock() { RELEASE(mMeasureMap); }

BEGIN_HANDLERS(BeatClock)
    HANDLE_ACTION(start, mIsRunning = true)
    HANDLE_ACTION(pause, mIsRunning = false)
    HANDLE_ACTION(reset, Reset())
    HANDLE(sync, OnSyncState)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(BeatClock)
    SYNC_PROP(bpm, mBeatsPerMinute)
    SYNC_PROP_SET(beats_per_measure, mBeatsPerMeasure, SetBeatsPerMeasure(_val.Int()))
    SYNC_PROP(measures_per_phrase, mMeasuresPerPhrase)
    SYNC_PROP_MODIFY(use_global, mUseGlobal, mSound = nullptr)
    SYNC_PROP(measure, mSongPos.AccessMeasure())
    SYNC_PROP(phrase, mSongPos.AccessPhrase())
    SYNC_PROP(beat, mSongPos.AccessBeat())
    SYNC_PROP(tick, mSongPos.AccessTick())
    SYNC_PROP(sub_division, mSubDivision)
    SYNC_PROP(total_beat, mSongPos.AccessTotalBeat())
    SYNC_PROP(seconds, mTotalSeconds)
    SYNC_PROP_MODIFY(sound, mSound, mUseGlobal = false)
    SYNC_PROP(timeline, (int &)mTimeline)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(BeatClock)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mBeatsPerMinute;
    bs << mBeatsPerMeasure;
    bs << mUseGlobal;
    bs << mMeasuresPerPhrase;
    bs << mSound;
    bs << mTimeline;
END_SAVES

BEGIN_COPYS(BeatClock)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(BeatClock)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mBeatsPerMinute)
        COPY_MEMBER(mBeatsPerMeasure)
        COPY_MEMBER(mMeasuresPerPhrase)
        COPY_MEMBER(mUseGlobal)
        COPY_MEMBER(mTotalSeconds)
        COPY_MEMBER(mSound)
        COPY_MEMBER(mTimeline)
    END_COPYING_MEMBERS
END_COPYS

void BeatClock::Enter() { Reset(); }
void BeatClock::Poll() { UpdateSongPos(); }

void BeatClock::Reset() {
    if (!mUseGlobal && !mSound) {
        if (mMeasuresPerPhrase != 0) {
            SetProperty("phrase", 0);
        }
        SetProperty("measure", 0);
        SetProperty("beat", 0);
        SetProperty("total_beat", 0.0f);
        SetProperty("sub_division", 0);
        SetProperty("seconds", 0.0f);
        static Message on_reset("on_reset");
        Export(on_reset, true);
    }
}

void BeatClock::SetBeatsPerMeasure(int beats) {
    mBeatsPerMeasure = beats;
    RELEASE(mMeasureMap);
    mMeasureMap = new MeasureMap();
    mMeasureMap->AddTimeSignature(0, beats, 4, true);
}

INIT_REVS(3, 0)

BEGIN_LOADS(BeatClock)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    d >> mBeatsPerMinute;
    d >> mBeatsPerMeasure;
    d >> mUseGlobal;
    if (d.rev >= 1) {
        d >> mMeasuresPerPhrase;
    }
    if (d.rev >= 2) {
        mSound.Load(d.stream, true, nullptr);
    }
    if (d.rev >= 3) {
        d >> (int &)mTimeline;
    }
    SetBeatsPerMeasure(mBeatsPerMeasure);
END_LOADS

void BeatClock::UpdateSongPos() {
    SongPos pos;
    float seconds = 0.0f;
    float totalBeat = 0.0f;

    if (mUseGlobal) {
        pos = TheTaskMgr.GetSongPos();
        seconds = TheTaskMgr.Time(mTimeline);
    } else {
        if (mSound) {
            seconds = mSound->ElapsedTime();
        } else {
            float time = TheTaskMgr.Time(mTimeline);
            float delta = time - unk50;
            unk50 = time;
            if (delta == 0.0f)
                return;
            if (!mIsRunning)
                return;
            seconds = mTotalSeconds + delta;
        }
        totalBeat = mBeatsPerMinute * seconds * (1.0f / 60.0f);
        float totalTick = totalBeat * 480.0f;
        pos.AccessTotalTick() = totalTick;
        int tick = (int)totalTick;
        int measure, beat, tickRem;
        mMeasureMap->TickToMeasureBeatTick(tick, measure, beat, tickRem);
        pos.AccessMeasure() = measure;
        pos.AccessBeat() = beat;
        pos.AccessTick() = tickRem;
    }

    int measPerPhrase = mMeasuresPerPhrase;
    if (measPerPhrase != 0) {
        pos.AccessPhrase() = pos.GetMeasure() / measPerPhrase;
        pos.AccessMeasure() = pos.GetMeasure() % measPerPhrase;
    }
    int subdivision = pos.GetTick() / 120;

    if (measPerPhrase != 0) {
        SetProperty("phrase", pos.GetPhrase());
    }
    SetProperty("measure", pos.GetMeasure());
    SetProperty("beat", pos.GetBeat());
    SetProperty("tick", pos.GetTick());
    SetProperty("total_beat", totalBeat);
    SetProperty("sub_division", subdivision);
    SetProperty("seconds", seconds);
}

DataNode BeatClock::OnSyncState(DataArray *msg) {
    BeatClock *src = msg->Obj<BeatClock>(2);
    int mode = msg->Node(3).Int(msg);

    if (src) {
        SongPos srcPos = src->mSongPos;
        SongPos oldPos = mSongPos;

        switch (mode) {
        case kSync_TicksOnly:
            SetProperty("tick", srcPos.GetTick());
            SetProperty("sub_division", src->mSubDivision);
            break;
        case kSync_BeatsAndTicks:
            SetProperty("tick", srcPos.GetTick());
            SetProperty("sub_division", src->mSubDivision);
            SetProperty("beat", srcPos.GetBeat());
            break;
        case kSync_MeasuresBeatsAndTicks:
        default:
            SetProperty("tick", srcPos.GetTick());
            SetProperty("sub_division", src->mSubDivision);
            SetProperty("beat", srcPos.GetBeat());
            SetProperty("measure", srcPos.GetMeasure());
            break;
        }

        float delta =
            (float)(mSongPos.GetBeat() - oldPos.GetBeat())
            + (float)((mSongPos.GetMeasure() - oldPos.GetMeasure()) * mBeatsPerMeasure)
            + (float)(mSongPos.GetTick() - oldPos.GetTick()) * (1.0f / 480.0f);

        if (std::fabs(delta) >= 0.0001f) {
            float newTotalBeat = mSongPos.GetTotalBeat() + delta;
            mSongPos.AccessTotalBeat() = newTotalBeat;
            mSongPos.AccessTotalTick() = newTotalBeat * 480.0f;
            BroadcastPropertyChange(Symbol("total_beat"));
            mTotalSeconds += (60.0f / mBeatsPerMinute) * delta;
            BroadcastPropertyChange(Symbol("seconds"));
        }
    }

    return DataNode(0);
}
