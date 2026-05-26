#include "hamobj/HamMaster.h"
#include "HamAudio.h"
#include "flow/PropertyEventProvider.h"
#include "hamobj/HamSongData.h"
#include "math/Decibels.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "midi/DataEventList.h"
#include "midi/MidiParserMgr.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "synth/Faders.h"
#include "synth/Synth.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "utl/SongPos.h"
#include "utl/TimeConversion.h"

HamMaster::HamMaster(HamSongData *data, MidiParserMgr *mgr)
    : mSongData(data), mAudio(nullptr), mMidiParserMgr(mgr), mSongInfo(nullptr),
      mLoader(0), mLoaded(0), mSongMs(0), mStreamMs(-1), mStreamJumped(0), mPreJumpMs(-1), mPostJumpMs(-1),
      mStreamMsAtJump(-1), unk9c(0), unka0(0), unka4(0), unkb0(0), mMetronome(0) {
    Reset();
    mAudio = new HamAudio();
}

HamMaster::~HamMaster() { delete mAudio; }

BEGIN_HANDLERS(HamMaster)
    HANDLE_EXPR(stream_time, mStreamMs / 1000)
    HANDLE_EXPR(song_duration_ms, SongDurationMs())
    HANDLE_EXPR(event_beat, EventBeat(_msg->Sym(2)))
    HANDLE_ACTION(toggle_metronome, mMetronome = !mMetronome)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(HamMaster)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void HamMaster::Poll(float f1) {
    if (IsLoaded() && mAudio->GetSongStream()) {
        mSongMs = f1;
        mSongPos = mSongData->CalcSongPos(this, mSongMs);
        float f8 = mAudio->GetSongStream()->GetJumpBackTotalTime(f1);
        float f9 = f8 + mSongMs;
        mStreamJumped = f9 < mStreamMs;
        Marker marker1, marker2;
        bool jp = mAudio->GetSongStream()->CurrentJumpPoints(marker1, marker2);
        if (!mStreamJumped && jp && marker1.posMS <= marker2.posMS) {
            if (mStreamMs <= marker2.posMS && marker2.posMS < f9) {
                mStreamJumped = true;
            } else {
                mStreamJumped = false;
            }
        }
        if (mStreamJumped) {
            float f10;
            if (jp) {
                f10 = MsToTick(marker2.posMS) - 1.0f;
            } else {
                f10 = mSongPos.GetTotalTick();
            }
            if (mMidiParserMgr) {
                mMidiParserMgr->Reset();
            }
            mStreamMsAtJump = mStreamMs;
            static Message msg("stream_jump");
            Export(msg, true);
        }
        CheckBeat();
        CheckLevels();
        mAudio->Poll();
    }
#ifdef HX_NATIVE
    // TODO(native): The original decomp is missing this call. RB3's
    // BeatMaster::Poll() has mMidiParserMgr->Poll() here, which
    // dispatches MIDI events (including the "end" event that triggers
    // SetGameOver). Needs verification against the DC3 target binary.
    // Placed outside the GetSongStream() guard so it runs even when
    // audio is unavailable (e.g. web/Emscripten with no mogg files).
    if (IsLoaded() && mMidiParserMgr) {
        mMidiParserMgr->Poll();
    }
#endif
}

void HamMaster::Jump(float f1) {
    SongPos calcedPos = mSongData->CalcSongPos(this, f1);
    const SongPos &tmp = mSongPos;
    mSongPos = calcedPos;
    mPrevSongPos = tmp;
    mLastBeatIndex = -1;
    mBeatCount = 0;
    if (mMidiParserMgr) {
        mMidiParserMgr->Reset(tmp.GetTotalTick());
    }
    mAudio->Jump(f1);
}

void HamMaster::Reset() {
    mPrevSongPos = SongPos();
    mBeatCount = 0;
    mLastBeatIndex = -1;
    for (int i = 0; i < mSubmixIdxs.size(); i++) {
        mSubmixIdxs[i] = 0;
    }
    if (mMidiParserMgr) {
        mMidiParserMgr->Reset();
    }
    static Message msg("first_beat");
    Export(msg, true);
    ResetAudio();
    mStreamMs = 0;
    mPreJumpMs = 0;
    mStreamJumped = false;
    mSongMs = 0;
    if (TheSynth->CheckCommonBank(false)) {
        Fader *fade = TheSynth->Find<Fader>("music_level.fade", false);
        if (fade) {
            AddMusicFader(fade);
        }
        Fader *crossfade = TheSynth->Find<Fader>("music_level_cross_fade.fade", false);
        if (crossfade) {
            AddMusicFader(crossfade);
        }
        Fader *fadeinout = TheSynth->Find<Fader>("skills_music_fade_in_out.fade", false);
        if (fadeinout) {
            AddMusicFader(fadeinout);
        }
    }
}

float HamMaster::SongDurationMs() {
    if (mMidiParserMgr) {
        DataEventList *events = mMidiParserMgr->GetEventsList();
        static Symbol end("end");
        for (int i = 0; i < events->Size(); i++) {
            const DataEvent &ev = events->Event(i);
            if (ev.Type(1) == end) {
                float tick = mSongData->GetBeatMap()->BeatToTick(ev.Start());
                return mSongData->GetTempoMap()->TickToTime(tick);
            }
        }
    }
    return 0;
}

void HamMaster::Load(
    SongInfo *s,
    bool b2,
    int i3,
    bool b4,
    HamSongDataValidate v,
    std::vector<MidiReceiver *> *
) {
    mSyncLoad = b2;
    mSongInfo = s;
    mSongData->Load(s, b4, v);
    MILO_ASSERT(!mLoader, 0x69);
    mLoader = new HamMasterLoader(this);
    mLoaded = false;
    if (b4) {
        TheLoadMgr.PollUntilLoaded(mLoader, nullptr);
    }
}

void HamMaster::LoadOnlySongData(SongInfo *s, bool b, HamSongDataValidate v) {
    mSongInfo = s;
    mSongData->Load(s, b, v);
}

void HamMaster::ResetAudio() {
    if (mAudio && mAudio->GetTime()) {
        mAudio->Jump(0);
    }
}

float HamMaster::StreamMs() const { return mStreamMs; }

bool HamMaster::DetectStreamJump(float &f1, float &f2, float &f3) const {
    if (!mStreamJumped) {
        return false;
    } else {
        f1 = mPreJumpMs;
        f2 = mPostJumpMs;
        f3 = mStreamMsAtJump;
        return true;
    }
}

void HamMaster::AddMusicFader(Fader *fader) {
    if (GetAudio() && GetAudio()->GetSongStream()) {
        GetAudio()->GetSongStream()->Faders()->Add(fader);
    }
}

void HamMaster::SetMaps() { mSongData->SetMaps(); }

float HamMaster::EventBeat(Symbol s) {
    if (mMidiParserMgr) {
        DataEventList *events = mMidiParserMgr->GetEventsList();
        for (int i = 0; i < events->Size(); i++) {
            const DataEvent &ev = events->Event(i);
            if (ev.Type(1) == s) {
                return ev.Start();
            }
        }
    }
    return -1;
}

void HamMaster::LoaderPoll() {
    bool songPoll = mSongData->Poll();
    if (songPoll) {
        if (TheLoadMgr.EditMode() && !TheSynth->CheckCommonBank(false)) {
            ObjDirPtr<ObjectDir> dir;
            DataArray *cfg = SystemConfig("sound", "banks", "common");
            dir.LoadFile(cfg->Str(1), false, true, kLoadFront, false);
            TheSynth->SetDir(dir);
        }
        mAudio->Load(mSongInfo, mSyncLoad);
        mSongInfo = nullptr;
        if (mMidiParserMgr) {
            mMidiParserMgr->FinishLoad();
        }
        mLoaded = true;
        RELEASE(mLoader);
    }
}

void HamMaster::CheckBeat() {
    int totalbeat1 = mSongPos.GetTotalBeat();
    int totalbeat2 = mPrevSongPos.GetTotalBeat();
    if (totalbeat1 != totalbeat2) {
        int beat = mSongPos.GetBeat();
        TheHamProvider->SetProperty("beat", beat + 1);
        if (mMetronome) {
            if (beat == 0) {
                TheSynth->PlaySound("metronome_measure", 0, 0, 0);
            } else {
                TheSynth->PlaySound("metronome_beat", 0, 0, 0);
            }
        }
        static DataNode &n = DataVariable("beat");
        n = totalbeat2;
        static Message msg("beat");
        Export(msg, true);
    }
    if (mPrevSongPos.GetMeasure() != mSongPos.GetMeasure()) {
        static DataNode &n = DataVariable("measure");
        n = mSongPos.GetMeasure();
        static Message msg("downbeat");
        TheHamProvider->Export(msg, true);
    }
    if (mPrevSongPos.GetTick() / 240 != mSongPos.GetTick() / 240) {
        static Message msg("halfbeat");
        TheHamProvider->Export(msg, true);
    }
    if (mPrevSongPos.GetTick() / 120 != mSongPos.GetTick() / 120) {
        static Message msg("quarterbeat");
        TheHamProvider->Export(msg, true);
    }
    mPrevSongPos = mSongPos;
}

void HamMaster::CheckLevels() {
    if (!TheSynth)
        return;

    PropertyEventProvider *prov =
        ObjectDir::Main()->Find<PropertyEventProvider>("audio_channels", false);
    if (!prov)
        return;

    std::vector<LevelData> &levelData = TheSynth->GetLevelData();
    if (levelData.size() == 0)
        return;

    float rightRMS = levelData[levelData.size() - 1].mRMS;
    float leftRMS;
    if (levelData.size() > 2) {
        leftRMS = levelData[levelData.size() - 2].mRMS;
    } else {
        leftRMS = rightRMS;
    }

    float rightLevel = (RatioToDb(rightRMS) + 96.0f) / 96.0f;
    float leftLevel = (RatioToDb(leftRMS) + 96.0f) / 96.0f;

    float rClamped = Clamp(0.0f, 1.0f, rightLevel);
    float lClamped = Clamp(0.0f, 1.0f, leftLevel);

    Vector2 v(rClamped, lClamped);
    mLevelHistory.push_back(v);

    // Average all entries
    float sumY = 0.0f;
    float sumX = 0.0f;
    for (std::list<Vector2>::iterator it = mLevelHistory.begin();
         it != mLevelHistory.end(); ++it) {
        sumX += it->x;
        sumY += it->y;
    }

    unsigned int histCount = 0;
    for (std::list<Vector2>::iterator it = mLevelHistory.begin();
         it != mLevelHistory.end(); ++it) {
        histCount++;
    }

    float avgX = (1.0f / (float)histCount) * sumX;
    float avgY = (1.0f / (float)histCount) * sumY;

    // Trim to <= 3 entries
    while (true) {
        if (mLevelHistory.size() <= 3)
            break;
        auto _tmp0 = mLevelHistory.begin();
        mLevelHistory.erase(_tmp0);
    }

    // Build 8-channel weighted levels
    float channels[8];

    // Right channels (3 to 0, decaying)
    float weight = 1.0f;
    int i = 4;
    do {
        i--;
        channels[i] = weight * avgX;
        weight *= 0.7f;
    } while (i != 0);

    // Left channels (4 to 7, decaying)
    weight = 1.0f;
    i = 4;
    do {
        channels[i] = weight * avgY;
        weight *= 0.7f;
        i++;
    } while (i < 8);

    // Set properties on audio_channels provider
    for (i = 0; i < 8; i++) {
        prov->SetProperty(Symbol(MakeString("channel%d", i)), DataNode(channels[i]));
    }
}

HamMasterLoader::HamMasterLoader(HamMaster *master)
    : Loader("", kLoadBack), mMaster(master) {}

void HamMasterLoader::PollLoading() { mMaster->LoaderPoll(); }
