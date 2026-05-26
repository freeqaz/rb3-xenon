#include "hamobj/HamSongData.h"
#include "hamobj/HamMaster.h"
#include "macros.h"
#include "math/FileChecksum.h"
#include "math/StreamChecksum.h"
#include "midi/Midi.h"
#include "midi/MidiParserMgr.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/BeatMap.h"
#include "utl/FakeSongMgr.h"
#include "utl/FileStream.h"
#include "utl/MemMgr.h"
#include "utl/MemStream.h"
#include "utl/SongInfoCopy.h"
#include "utl/SongPos.h"
#include "utl/TempoMap.h"
HamSongData *HamSongData::sInstance;

HamSongData::HamSongData()
    : mLoaded(0), mSongInfo(0), mTempoMap(nullptr), mMeasureMap(nullptr), mBeatMap(nullptr),
      mStream(0), mMidiReader(0) {
    sInstance = this;
}

HamSongData::~HamSongData() {
    if (sInstance == this) {
        sInstance = nullptr;
    }
    ResetTheTempoMap();
    ResetTheBeatMap();
    RELEASE(mTempoMap);
    RELEASE(mMeasureMap);
    RELEASE(mBeatMap);
}

SongPos HamSongData::CalcSongPos(HxMaster *master, float f1) {
    MILO_ASSERT(mTempoMap, 0xB9);
    MILO_ASSERT(mMeasureMap, 0xBA);
    MILO_ASSERT(mBeatMap, 0xBB);
    HamMaster *hamMaster = dynamic_cast<HamMaster *>(master);
    if (hamMaster) {
        Stream *stream = hamMaster->GetAudio()->GetSongStream();
        if (stream) {
            f1 += stream->GetJumpBackTotalTime(f1);
        }
    }
    float tick = mTempoMap->TimeToTick(f1);
    int m, b, t, bpm;
    mMeasureMap->TickToMeasureBeatTick(tick, m, b, t, bpm);
    float beat = mBeatMap->Beat((int)tick);
    return SongPos(tick, beat, 0, m, b, t);
}

void HamSongData::SetMaps() {
    SetTheTempoMap(mTempoMap);
    SetTheBeatMap(mBeatMap);
}

bool HamSongData::Poll() {
    if (mMidiReader) {
        bool ret = mMidiReader->ReadSomeEvents(20);
        if (ret) {
#ifdef HX_NATIVE
            MILO_LOG("DC3 HamSongData::Poll — MIDI read complete! tell=%d\n",
                    mStream ? mStream->Tell() : -1);
#endif
            PostLoad();
        }
        return ret;
    } else
        return true;
}

void HamSongData::PostLoad() {
    mMidiReader->ClaimMaps(mMeasureMap, mTempoMap);
    SetMaps();
    RELEASE(mMidiReader);
    RELEASE(mStream);
    MILO_ASSERT(mTempoMap, 0xA5);
    mTempoMap->Finalize();
    mLoaded = true;
}

void Validate(MemStream *ms, const char *cc, bool b) {
    if (UsingCD()) {
        if (HasFileChecksumData()) {
            StreamChecksumValidator v;
            if (v.Begin(cc, b)) {
                v.Update((const unsigned char *)ms->Buffer(), ms->Size());
                v.End();
                v.Validate();
            }
        }
    }
}

void HamSongData::Load(const char *cc, const SongInfo *info, bool b3) {
    mSongInfo = info;
    ResetTheTempoMap();
    ResetTheBeatMap();
    RELEASE(mTempoMap);
    RELEASE(mMeasureMap);
    RELEASE(mBeatMap);
    mBeatMap = new BeatMap();
    MidiParserMgr *mgr = TheMidiParserMgr;
    MidiReceiver *rcvr = mgr ? mgr : (MidiReceiver *)this;
    mMidiReader = new MidiReader(*mStream, *rcvr, cc);
    if (b3) {
        while (!Poll())
            ;
    }
}

void HamSongData::Load(const SongInfo *info, bool b2, HamSongDataValidate v) {
    const char *midi = FakeSongMgr::MidiFile(info);
#ifdef HX_NATIVE
    MILO_LOG("DC3 HamSongData::Load — midi='%s'\n", midi);
#endif
    FileStream fStream(midi, FileStream::kRead, false);
#ifdef HX_NATIVE
    MILO_LOG("DC3 HamSongData::Load — fStream size=%d\n", (int)fStream.Size());
#endif
    mStream = new MemStream();
    {
        MemTemp tmp;
        mStream->Resize(fStream.Size());
        fStream.Read((void *)mStream->Buffer(), fStream.Size());
        if (v == 2 || v == 1) {
            Validate(mStream, midi, v == 1);
        }
    }
    Load(midi, info, b2);
}
