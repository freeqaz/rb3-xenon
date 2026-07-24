#include "beatmatch/SongData.h"
#ifdef HX_NATIVE
// Native rb3-hit never constructs a BeatMatcher (mBeatMatchers stays empty), so
// SongData's BeatMatcher* loops are dead code that only needs to compile. The
// real BeatMatcher.h drags in the game/meta_band/tour/net header cascade that is
// off the native gameplay-core path; a minimal shim avoids it. X360 build is
// unaffected (still includes the real header below).
#include "beatmatch/BeatMatcher_native.h"
#else
#include "beatmatch/BeatMatcher.h"
#endif
#include "beatmatch/GameGem.h"
#include "beatmatch/GameGemDB.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/InternalSongParserSink.h"
#include "beatmatch/Phrase.h"
#include "beatmatch/PhraseAnalyzer.h"
#include "beatmatch/RGChords.h"
#include "beatmatch/SongParser.h"
#include "beatmatch/TimeSpanVector.h"
#include "beatmatch/TrackType.h"
#include "beatmatch/VocalNote.h"
#include "beatmatch/SongParserSink.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/TuningOffsetList.h"
#include "beatmatch/DrumMixDB.h"
#include "beatmatch/DrumMap.h"
#include "decomp.h"
#include "macros.h"
#include "math/FileChecksum.h"
#include "math/StreamChecksum.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "os/Timer.h"
#include "utl/BeatMap.h"
#include "utl/FakeSongMgr.h"
#include "utl/FilePath.h"
#include "utl/FileStream.h"
#include "utl/MBT.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/MemStream.h"
#include "utl/RangedDataCollection.h"
#include "utl/SongInfoAudioType.h"
#include "utl/SongInfoCopy.h"
#include "utl/TickedInfo.h"
#include <algorithm>

const int kHarm3VocalNoteList = 3;

DECOMP_FORCEACTIVE(SongData, ".vfv")

SongData::TrackInfo::TrackInfo(
    Symbol sym, SongInfoAudioType audioty, AudioTrackNum num, TrackType trackty, bool bb
)
    : mName(sym), mLyrics(new TickedInfoCollection<String>()),
      mAudioType((BeatmatchAudioType)audioty), mAudioTrackNum(num), mType(trackty),
      mIndependentSlots(bb) {}

SongData::TrackInfo::~TrackInfo() { RELEASE(mLyrics); }

SongData::SongData()
    : mNumTracks(0), mNumDifficulties(0), mLoaded(0), mSongInfo(0), mSectionStartTick(-1),
      mSectionEndTick(-1), mFakeHitGemsInFill(0), mPhraseAnalyzer(0),
      mLoadingVocalNoteListIndex(0), mTempoMap(0), mMeasureMap(0), mBeatMap(0),
      mTuningOffsetList(0), mLastGemTime(0), mMemStream(0), mSongParser(0),
      mPlayerTrackConfigList(0), mGems(0), mHopoThreshold(0), mDetailedGrid(0) {
    mVocalNoteLists.reserve(4);
    mVocalNoteLists.push_back(new VocalNoteList(this));
}

SongData::~SongData() {
    ResetTheTempoMap();
    ResetTheBeatMap();
    for (int i = 0; i < mTrackInfos.size(); i++) {
        RELEASE(mTrackInfos[i]);
        RELEASE(mFills[i]);
        RELEASE(mDrumMaps[i]);
        RELEASE(mRollInfos[i]);
        RELEASE(mTrillInfos[i]);
        RELEASE(mRGRollInfos[i]);
        RELEASE(mRGTrillInfos[i]);
        RELEASE(mDrumMixDBs[i]);
        RELEASE(mGemDBs[i]);
        RELEASE(mPhraseDBs[i]);
    }
    for (int i = 0; i < mBackupTracks.size(); i++) {
        RELEASE(mBackupTracks[i]);
    }
    for (int i = 0; i < mFakeTracks.size(); i++) {
        RELEASE(mFakeTracks[i]);
    }
    RELEASE(mPhraseAnalyzer);
    RELEASE(mTempoMap);
    RELEASE(mTuningOffsetList);
    RELEASE(mMeasureMap);
    RELEASE(mBeatMap);
    for (int i = 0; i < mVocalNoteLists.size(); i++) {
        RELEASE(mVocalNoteLists[i]);
    }
}

void Validate(MemStream *ms, const char *file, bool b) {
    if (!UsingCD() || !HasFileChecksumData())
        return;
    StreamChecksumValidator v;
    StreamChecksumValidator *vptr = &v;
    int _tmp1 = vptr->Begin(file, b);
    if (_tmp1) {
        int _tmp0 = ms->Size();
        vptr->Update((const unsigned char *)ms->Buffer(), _tmp0);
        vptr->End();
        vptr->Validate();
    }
}

void SongData::Load(
    SongInfo *info,
    int numDifficulties,
    PlayerTrackConfigList *pList,
    std::vector<MidiReceiver *> &midircvrs,
    bool bb,
    SongDataValidate v
) {
    const char *midi = FakeSongMgr::MidiFile(info);
    FileStream fs(midi, FileStream::kRead, false);
    mMemStream = new MemStream();
    {
        MemDoTempAllocations m;
        mMemStream->Resize(fs.Size());
        fs.Read((void *)mMemStream->Buffer(), fs.Size());
        if (v == kSongData_ValidateUsingNameOnly || v == kSongData_Validate) {
            Validate(mMemStream, midi, v == kSongData_ValidateUsingNameOnly);
        }
    }
    Load(midi, info, numDifficulties, pList, midircvrs, bb);
    FileStream fs88(FakeSongMgr::GetPath(info, ".vfv"), FileStream::kRead, true);
    if (!fs88.Fail()) {
        int count = 0;
        fs88 >> count;
        mVocalFeatureVectorTimes.clear();
        mVocalFeatureVectorPeaks.clear();
        for (int i = 0; i < count; i++) {
            float f94 = 0;
            float f98 = 0;
            fs88 >> f94;
            fs88 >> f98;
            mVocalFeatureVectorTimes.push_back(f94);
            mVocalFeatureVectorPeaks.push_back(f98);
        }
    }
}

void SongData::Load(
    const char *midifile,
    SongInfo *info,
    int numDifficulties,
    PlayerTrackConfigList *pList,
    std::vector<MidiReceiver *> &midircvrs,
    bool bb
) {
#ifdef MILO_DEBUG
    if (DataVariable("log_midi_file_load").Int()) {
        MILO_LOG("Loading MIDI file %s\n", midifile);
    }
#endif
    mSongInfo = info;
    mNumDifficulties = numDifficulties;
    mDetailedGrid = false;
    mHopoThreshold = mSongInfo->GetHopoThreshold();
    mKeyboardRangeSections.resize(numDifficulties);
    MILO_ASSERT(mTempoMap == NULL, 0xF5);
    MILO_ASSERT(mTuningOffsetList == NULL, 0xF6);
    MILO_ASSERT(mMeasureMap == NULL, 0xF7);
    MILO_ASSERT(mBeatMap == NULL, 0xF8);
    MILO_ASSERT(mPhraseAnalyzer == NULL, 0xF9);
    mBeatMap = new BeatMap();
    SetTheBeatMap(mBeatMap);
    mPhraseAnalyzer = new PhraseAnalyzer(this);
    int numchannels = mSongInfo->NumChannelsOfTrack(kAudioTypeDrum);
    mSongParser =
        new SongParser(*this, numDifficulties, mTempoMap, mMeasureMap, numchannels);
    mTuningOffsetList = new TuningOffsetList();
    for (int i = 0; i < midircvrs.size(); i++) {
        mSongParser->AddReceiver(midircvrs[i]);
    }
    mNumFilesLoaded = 0;
    mPlayerTrackConfigList = pList;
    mSongParser->SetNumPlayers(mPlayerTrackConfigList->NumConfigs());
    mSongParser->SetSectionBounds(mSectionStartTick, mSectionEndTick);
    mSongParser->ReadMidiFile(*mMemStream, midifile, mSongInfo);
    if (bb)
        while (!Poll())
            ;
}

bool SongData::Poll() {
    if (mSongParser) {
        Timer timer;
        timer.Restart();
        mSongParser->Poll();
        if (mSongParser->NoMidiReader()) {
            mNumFilesLoaded++;
            RELEASE(mMemStream);
            int i6 = mNumFilesLoaded;
            if (i6 <= mSongInfo->NumExtraMidiFiles()) {
                const char *midi = mSongInfo->GetExtraMidiFile(i6 - 1);
                MILO_LOG("MIDI: merged filename: %s\n", midi);
                mSongPath = FilePath(FileRoot(), midi);
                MILO_LOG("MIDI: merged filename2: %s\n", mSongPath);
                FileStream fs(midi, FileStream::kRead, false);
                mMemStream = new MemStream();
                {
                    MemDoTempAllocations m;
                    mMemStream->Resize(fs.Size());
                    fs.Read((void *)mMemStream->Buffer(), fs.Size());
                }
                mSongParser->MergeMidiFile(*mMemStream, midi);
                return false;
            } else {
                PostLoad(mPlayerTrackConfigList);
                RELEASE(mSongParser);
                return true;
            }
        } else
            return false;
    } else
        return true;
}

void SongData::AddSink(SongParserSink *sink) { mSongParserSinks.push_back(sink); }

void SongData::FixUpTrackConfig(PlayerTrackConfigList *plist) {
    std::vector<TrackType> types;
    types.reserve(mNumTracks);
    for (int i = 0; i < mNumTracks; i++) {
        types.push_back(mTrackInfos[i]->mType);
    }
    plist->Process(types);
}

void SongData::SetUpTrackDifficulties(PlayerTrackConfigList *plist) {
    mTrackDifficulties.clear();
    mTrackDifficulties.reserve(plist->mTrackDiffs.size());
    for (int i = 0; i < (int)plist->mTrackDiffs.size(); i++) {
        int topush = plist->mTrackDiffs[i];
        if (topush == -1)
            topush = 0;
        mTrackDifficulties.push_back(topush);
    }
}

void SongData::UpdatePlayerTrackConfigList(PlayerTrackConfigList *plist) {
    SetUpTrackDifficulties(plist);
    ComputeVocalRangeData();
}

void SongData::PostLoad(PlayerTrackConfigList *pList) {
    FixUpTrackConfig(pList);
    SetUpTrackDifficulties(pList);
    for (std::vector<SongParserSink *>::iterator it = mSongParserSinks.begin();
         it != mSongParserSinks.end();
         ++it) {
        (*it)->SetNumTracks(mNumTracks);
    }
    for (int i = 0; i < mGemDBs.size(); i++) {
        mGemDBs[i]->Finalize();
    }
    if (mTempoMap)
        mTempoMap->Finalize();
    else
        MILO_WARN("%s: MIDI file does not have a valid tempo map", SongFullPath());
    MakeBackupTracks();
    RestoreAllTracksFromBackup();
    for (int i = 0; i < mNumTracks; i++) {
        PostLoadTrack(i);
    }
    mPhraseAnalyzer->Analyze();
    for (std::vector<BeatMatcher *>::iterator it = mBeatMatchers.begin();
         it != mBeatMatchers.end();
         ++it) {
        (*it)->PostLoad();
    }
    PostLoadVocals();
    mLoaded = true;
}

void SongData::PostLoadTrack(int track) {
    TrackInfo *trackInfo = mTrackInfos[track];
    if (!trackInfo->FakeAudio()) {
        const PhraseList &phrases = mPhraseDBs[track]->GetPhraseList(0, kCommonPhrase);
        for (std::vector<Phrase>::const_iterator it = phrases.mPhrases.begin();
             it != phrases.mPhrases.end();
             ++it) {
            mPhraseAnalyzer->AddInfo(
                track,
                trackInfo->mType,
                it->GetTick(),
                it->GetTick() + it->GetDurationTicks(),
                trackInfo->mAudioType == kAudioVocals
            );
        }
        for (std::vector<BeatMatcher *>::iterator it = mBeatMatchers.begin();
             it != mBeatMatchers.end();
             ++it) {
            (*it)->AddTrack(
                track,
                trackInfo->mName,
                (SongInfoAudioType)trackInfo->mAudioType,
                trackInfo->mType,
                trackInfo->mIndependentSlots
            );
        }
        for (std::vector<SongParserSink *>::iterator it = mSongParserSinks.begin();
             it != mSongParserSinks.end();
             ++it) {
            (*it)->AddTrack(
                track,
                trackInfo->mName,
                (SongInfoAudioType)trackInfo->mAudioType,
                trackInfo->mType,
                trackInfo->mIndependentSlots
            );
        }
        if (HasTrackDiffs()) {
            SendPhrases(track);
            SendGems(track);
        }
    }
}

void SongData::SendPhrases(int track) {
    int curTrackDiff = mTrackDifficulties[track];
    PhraseDB *curPhraseDB = mPhraseDBs[track];
    for (int i = 0; i < kNumPhraseTypes; i++) {
        const PhraseList &phrases =
            curPhraseDB->GetPhraseList(curTrackDiff, (BeatmatchPhraseType)i);
        for (std::vector<Phrase>::const_iterator it = phrases.mPhrases.begin();
             it != phrases.mPhrases.end();
             ++it) {
            for (std::vector<SongParserSink *>::iterator sit = mSongParserSinks.begin();
                 sit != mSongParserSinks.end();
                 ++sit) {
                (*sit)->AddPhrase((BeatmatchPhraseType)i, track, *it);
            }
        }
    }
}

void SongData::ChangeTrackDiff(int track, int newDiff) {
    if ((unsigned int)track < mTrackDifficulties.size() && track != -1) {
        mTrackDifficulties[track] = newDiff;
        PhraseDB *curPhraseDB = mPhraseDBs[track];
        for (int i = 0; i < kNumPhraseTypes; i++) {
            if ((unsigned int)(i - kArpeggioPhrase) <= 1) {
                const PhraseList &phrases =
                    curPhraseDB->GetPhraseList(newDiff, (BeatmatchPhraseType)i);
                for (std::vector<Phrase>::const_iterator it = phrases.mPhrases.begin();
                     it != phrases.mPhrases.end();
                     ++it) {
                    for (std::vector<SongParserSink *>::iterator sit =
                             mSongParserSinks.begin();
                         sit != mSongParserSinks.end();
                         ++sit) {
                        (*sit)->AddPhrase((BeatmatchPhraseType)i, track, *it);
                    }
                }
            }
        }
    }
}

void SongData::SendGems(int track) {
    const GameGemDB *curDB = mGemDBs[track];
    const std::vector<GameGem> &gems =
        curDB->GetDiffGemList(mTrackDifficulties[track])->mGems;
    for (std::vector<GameGem>::const_iterator it = gems.begin(); it != gems.end(); ++it) {
        for (std::vector<SongParserSink *>::iterator sit = mSongParserSinks.begin();
             sit != mSongParserSinks.end();
             ++sit) {
            (*sit)->AddMultiGem(track, *it);
        }
    }
}

void SongData::PostLoadVocals() {
    for (int i = 0; i < mVocalNoteLists.size(); i++) {
        VocalNoteList *curList = mVocalNoteLists[i];
        bool b = i == 0 || i == 1;
        if (i == 2) {
            curList->CopyLyricPhrases();
        }
        if (i > 1) {
            curList->CopyPhrasesFrom(mVocalNoteLists[1]);
        }
        curList->NotesDone(*mTempoMap, b);
        if (i == 2 && curList->mLyricPhrases.empty()) {
            MILO_WARN(
                "%s: no lyric phrases authored for HARM2, defaulting to HARM1 phrases",
                SongFullPath()
            );
            curList->CopyLyricPhrases();
        }
    }

    std::vector<std::pair<float, float> > floatPairVec;
    for (int i = 1; i < mVocalNoteLists.size(); i++) {
        VocalNoteList *curList = mVocalNoteLists[i];
        std::vector<std::pair<float, float> > morePairs;
        curList->GenerateLegalFreestyleSections(morePairs);
        if (i == 1)
            floatPairVec = morePairs;
        else {
            std::vector<std::pair<float, float> > pairs3;
            IntersectTimeSpans(floatPairVec, morePairs, pairs3);
            floatPairVec = pairs3;
        }
    }

    for (int i = 1; i < mVocalNoteLists.size(); i++) {
        VocalNoteList *curList = mVocalNoteLists[i];
        std::vector<std::pair<float, float> > morePairs;
        IntersectTimeSpans(curList->mFreestyleSections, floatPairVec, morePairs);
        curList->SetFreestyleSections(morePairs);
    }

    if (mVocalNoteLists.size() == 4) {
        VocalNoteList *list2 = mVocalNoteLists[2];
        VocalNoteList *list3 = mVocalNoteLists[3];
        std::vector<std::pair<float, float> > morePairs;
        IntersectTimeSpans(
            list2->mFreestyleSections, list3->mFreestyleSections, morePairs
        );
        list2->SetFreestyleSections(morePairs);
        list3->SetFreestyleSections(morePairs);
    }

    for (int i = 0; i < mVocalNoteLists.size(); i++) {
        mVocalNoteLists[i]->RemoveInvalidFreestyleSections();
    }
    ComputeVocalRangeData();
    ValidateVocalSPPhrases();
}

void SongData::ComputeVocalRangeData() {
    mRangeSections.clear();
    RangeSection s(0, 0);
    mRangeSections.push_back(s);
    for (std::map<int, float>::iterator it = mRangeShifts.begin();
         it != mRangeShifts.end();
         ++it) {
        s.unk0 = it->first;
        s.unk4 = it->second;
        mRangeSections.push_back(s);
    }
    int u10start = 0;
    int i9 = 0;
    if (mPlayerTrackConfigList->UseVocalHarmony()) {
        i9 = mVocalNoteLists.size() - 1;
        u10start = 1;
    }

    for (int i = 0; i < mRangeSections.size(); i++) {
        RangeSection &curSect = mRangeSections[i];
        int i2 = -1;
        if (i + 1 < mRangeSections.size()) {
            i2 = mRangeSections[i + 1].unk0;
        }
        for (int u10 = u10start; u10 <= i9; u10++) {
            mVocalNoteLists[u10]->UpdatePitchRangeTickDelimited(
                curSect.unk0, i2, curSect.unk8, curSect.unkc
            );
        }
        if (curSect.unk8 > 200.0f) {
            curSect.unk8 = i > 0 ? mRangeSections[i - 1].unk8 : 60.0f;
        }
        if (curSect.unkc < 0) {
            curSect.unkc = i > 0 ? mRangeSections[i - 1].unkc : 72.0f;
        }
        static bool dumpRangeSections;
        if (dumpRangeSections) {
            MILO_LOG(
                "New Range Section: tick %d, (%.1f, %.1f) size: %.0f\n",
                curSect.unk0,
                curSect.unk8,
                curSect.unkc,
                curSect.unkc - curSect.unk8
            );
        }
    }
}

void SongData::UnflipGems(int i1, int i2, int diff) {
    TickedInfoCollection<String> &backup_mixes =
        mBackupTracks[i2]->mMixes->GetMixList(diff);
    std::vector<GameGem> &backup_gems =
        mBackupTracks[i2]->mGems->GetDiffGemList(diff)->mGems;
    std::vector<GameGem> &gems = mGemDBs[i1]->GetDiffGemList(diff)->mGems;
    TickedInfoCollection<String> &mixes = mDrumMixDBs[i1]->GetMixList(diff);
    MILO_ASSERT(backup_gems.size() == gems.size(), 0x2CE);
    MILO_ASSERT(backup_mixes.Size() == mixes.Size(), 0x2CF);
    DataArray *cfg = SystemConfig("beatmatcher", "audio");
    DataArray *submixArr = cfg->FindArray("flipped_submixes", false);
    if (submixArr && !backup_mixes.mInfos.empty()) {
        Symbol tag = backup_mixes.mInfos[0].mInfo.c_str();
        Symbol s;
        bool found = submixArr->FindData(tag, s, false);
        if (found) {
            mixes.SetInfo(0, s);
        }
        int i11 = 1;
        int _tmp2 = backup_mixes.mInfos.size();
        int i7 = _tmp2 == 1 ? 0x7fffffff : backup_mixes.mInfos[1].mTick;
        for (int g = 0; g < gems.size(); g++) {
            MILO_ASSERT(backup_gems[g].GetTick() == gems[g].GetTick(), 0x2F4);
            if (backup_gems[g].GetTick() >= i7) {
                tag = backup_mixes.mInfos[i11].mInfo.c_str();
                found = submixArr->FindData(tag, s, false);
                if (found) {
                    mixes.SetInfo(i11, s);
                }
                i11++;
                if (i11 == backup_mixes.mInfos.size()) {
                    if (!found)
                        return;
                    i7 = 0x7fffffff;
                } else
                    i7 = backup_mixes.mInfos[i11].mTick;
            } else if (found)
                gems[g].Flip(backup_gems[g]);
        }
    }
}

void SongData::RestoreGems(int i1, int i2, int diff) {
    std::vector<BackupTrack *> &_ref0 = mBackupTracks;
    GameGemList *backup_gems = _ref0[i2]->mGems->GetDiffGemList(diff);
    GameGemList *gems = mGemDBs[i1]->GetDiffGemList(diff);
    gems->mGems = backup_gems->mGems;
    if (_ref0[i2]->mMixes) {
        const TickedInfoCollection<String> &backup_mixes =
            _ref0[i2]->mMixes->GetMixList(diff);
        TickedInfoCollection<String> &mixes = mDrumMixDBs[i1]->GetMixList(diff);
        mixes.CopyFrom(backup_mixes);
    }
}

void SongData::TrimOverlappingGems(int i1, int i2, int diff) {
    GameGemList *backup_gems = mBackupTracks[i2]->mGems->GetDiffGemList(diff);
    GameGemList *gems = mGemDBs[i1]->GetDiffGemList(diff);
    std::vector<GameGem> &glist = gems->mGems;
    std::vector<GameGem> &blist = backup_gems->mGems;

    glist.clear();
    glist.reserve(blist.size());

    if (blist.size() == 0) {
        MILO_WARN("Empty track found for SongData::TrimOverlappingGems!");
        return;
    }

    std::vector<GameGem>::iterator cur = blist.begin();
    while (cur != blist.end()) {
        std::vector<GameGem>::iterator next = cur + 1;
        int shortestDurationTick = cur->mDurationTicks;
        float shortestDurationMs = (float)cur->mDurationMs;

        while (next != blist.end() && abs(cur->mTick - next->mTick) < 10) {
            int nextDurTick = next->mDurationTicks;
            if (nextDurTick < shortestDurationTick)
                shortestDurationTick = nextDurTick;
            float nextDurMs = (float)next->mDurationMs;
            if (nextDurMs < shortestDurationMs)
                shortestDurationMs = nextDurMs;
            ++next;
        }

        if (next != blist.end()) {
            int nextTick = next->mTick;
            if (shortestDurationTick + cur->mTick > nextTick) {
                shortestDurationTick = nextTick - cur->mTick;
                shortestDurationMs = next->mMs - cur->mMs;
            }
        }

        if (shortestDurationTick < 0) return;
        if (shortestDurationMs < 0.0f) return;
        MILO_ASSERT(shortestDurationTick >= 0, 0x360);
        MILO_ASSERT(shortestDurationMs >= 0.0f, 0x361);

        GameGem merged = *cur;
        merged.mDurationTicks = shortestDurationTick;
        merged.mDurationMs = (unsigned short)shortestDurationMs;

        for (std::vector<GameGem>::iterator it = cur + 1; it != next; ++it) {
            merged.mSlots |= it->mSlots;
        }

        glist.push_back(merged);
        cur = next;
    }
}

void SongData::ValidateVocalSPPhrases() {
    int maxList;
    Symbol voxSym;
    int firstList;
    std::vector<VocalNoteList *> &_ref0 = mVocalNoteLists;
    if (mPlayerTrackConfigList->UseVocalHarmony()) {
        firstList = 1;
        maxList = std::min<int>(kHarm3VocalNoteList, _ref0.size() - 1);
        voxSym = "HARM1";
    } else {
        firstList = 0;
        maxList = 0;
        voxSym = "PART VOCALS";
    }

    int trackIdx;
    for (trackIdx = 0; trackIdx < mTrackInfos.size(); trackIdx++) {
        if (mTrackInfos[trackIdx]->mName == voxSym)
            break;
    }

    if (trackIdx == mTrackInfos.size() || mNumDifficulties <= 0)
        return;

    const PhraseList &phrases =
        mPhraseDBs[trackIdx]->GetPhraseList(mTrackDifficulties[trackIdx], kCommonPhrase);
    VocalNoteList *curVoxList = _ref0[firstList];
    int i = 0;
    if (phrases.mPhrases.size() == 0) return;
    for (; i < phrases.mPhrases.size(); i++) {
        float ms = phrases.mPhrases[i].mMs;
        float endMs = ms + phrases.mPhrases[i].GetDurationMs();
        VocalPhrase *phrase = NULL;
        bool found1 = false;
        for (int j = firstList; j <= maxList; j++) {
            if (_ref0[j]->NextNote(ms)) {
                found1 = true;
                break;
            }
        }
        if (!found1) {
            TheDebug.Notify(MakeString(
                "NOTIFY %s %s : vocal overdrive phrase at %i ms is after all notes.\n",
                SongFullPath(),
                curVoxList->GetTrackName(),
                (int)ms
            ));
        } else {
            VocalNote *next = curVoxList->NextNote(ms);
            if (next && next->GetMs() < ms) {
                TheDebug.Notify(MakeString(
                    "NOTIFY %s %s : vocal overdrive phrase at %i ms begins during a note.\n",
                    SongFullPath(),
                    curVoxList->GetTrackName(),
                    (int)ms
                ));
            } else if (next
                       && (phrase = &curVoxList->GetPhrases()[next->mPhrase],
                           &curVoxList->mNotes[phrase->unk10] != next)) {
                TheDebug.Notify(MakeString(
                    "NOTIFY %s %s : vocal overdrive phrase at %i ms starts mid-phrase.\n",
                    SongFullPath(),
                    curVoxList->GetTrackName(),
                    (int)ms
                ));
            } else {
                bool found2 = false;
                for (int j = firstList; j <= maxList; j++) {
                    VocalNote *n2 = _ref0[j]->NextNote(ms);
                    if (n2) {
                        VocalPhrase *p =
                            &_ref0[j]->GetPhrases()[n2->mPhrase];
                        if (p
                            && _ref0[j]->mNotes[p->unk14 - 1].GetMs()
                                <= endMs) {
                            found2 = true;
                            break;
                        }
                    }
                }
                if (!found2) {
                    TheDebug.Notify(MakeString(
                        "NOTIFY %s %s : vocal overdrive phrase at %i ms ends prematurely.\n",
                        SongFullPath(),
                        curVoxList->GetTrackName(),
                        (int)ms
                    ));
                } else if (phrase) {
                    int idx = phrase->unk14;
                    if (idx < curVoxList->mNotes.size()
                        && curVoxList->mNotes[idx].GetMs() < endMs) {
                        TheDebug.Notify(MakeString(
                            "NOTIFY %s %s : vocal overdrive phrase at %i ms cuts into next phrase.\n",
                            SongFullPath(),
                            curVoxList->GetTrackName(),
                            (int)ms
                        ));
                    }
                }
            }
        }
    }
}

void SongData::AddBeatMatcher(BeatMatcher *bm) { mBeatMatchers.push_back(bm); }

void SongData::PostDynamicAdd(BeatMatcher *bm, int trk) {
    if (HasBackupTrack(trk)) {
        RestoreTrackFromBackup(trk);
    }
    for (int i = 0; i < mNumTracks; i++) {
        TrackInfo *curTrackInfo = mTrackInfos[i];
        bm->AddTrack(
            i,
            curTrackInfo->mName,
            (SongInfoAudioType)curTrackInfo->mAudioType,
            curTrackInfo->mType,
            curTrackInfo->mIndependentSlots
        );
    }
}

void SongData::RemoveBeatMatcher(BeatMatcher *bm) {
    mBeatMatchers.erase(
        std::remove(mBeatMatchers.begin(), mBeatMatchers.end(), bm),
        mBeatMatchers.end()
    );
}

const PhraseList &SongData::GetPhraseList(int i, BeatmatchPhraseType ty) const {
    return mPhraseDBs[i]->GetPhraseList(mTrackDifficulties[i], ty);
}

void SongData::AddTrack(
    int, AudioTrackNum a, Symbol s, SongInfoAudioType songTy, TrackType trackTy, bool b
) {
    if (songTy == kAudioTypeFake) {
        mFakeTracks.push_back(new FakeTrack(s));
    } else {
        mNumTracks++;
        mTrackInfos.push_back(new TrackInfo(s, songTy, a, trackTy, b));
        mFills.push_back(new DrumFillInfo());
        mDrumMaps.push_back(new DrumMap());
        mRollInfos.push_back(new RangedDataCollection<unsigned int>());
        mTrillInfos.push_back(new RangedDataCollection<std::pair<int, int> >());
        mRGRollInfos.push_back(new RangedDataCollection<RGRollChord>());
        mRGTrillInfos.push_back(new RangedDataCollection<RGTrill>());
        mDrumMixDBs.push_back(new DrumMixDB(mNumDifficulties));
        mGemDBs.push_back(new GameGemDB(mNumDifficulties, mHopoThreshold));
        mPhraseDBs.push_back(new PhraseDB(mNumDifficulties));
        if (trackTy == kTrackVocals) {
            int harmPartNum = 0;
            if (strneq(s.Str(), "HARM", 4)) {
                harmPartNum = *(s.Str() + 4) - 0x31;
                if (harmPartNum < 0)
                    MILO_FAIL("Harmony part too low. Found part %d", harmPartNum);
                if (harmPartNum >= 3)
                    MILO_FAIL("Harmony part too high. Found part %d", harmPartNum);
                harmPartNum++;
            }
            while (mVocalNoteLists.size() <= harmPartNum) {
                mVocalNoteLists.push_back(new VocalNoteList(this));
            }
            mLoadingVocalNoteListIndex = harmPartNum;
            mVocalNoteLists[mLoadingVocalNoteListIndex]->SetTrackName(s);
        }
    }
}

void SongData::ClearTrack(int track) {
    mGemDBs[track]->Clear();
    mFills[track]->Clear();
    mDrumMaps[track]->Clear();
    mRollInfos[track]->Clear();
    mTrillInfos[track]->Clear();
    mRGRollInfos[track]->Clear();
    mRGTrillInfos[track]->Clear();
    mDrumMixDBs[track]->Clear();
    mPhraseDBs[track]->Clear();
    if (mTrackInfos[track]->mType == kTrackVocals) {
        if (mVocalNoteLists.size() != 1) {
            MILO_FAIL("MIDI merge can't overwrite harmony parts");
        }
        mVocalNoteLists[0]->Clear();
        mLoadingVocalNoteListIndex = 0;
    }
}

void SongData::OnEndOfTrack(int i, bool b) {
    if (b && i < mGemDBs.size()) {
        mGemDBs[i]->MergeChordGems();
    }
}

void SongData::AddMultiGem(int iii, const MultiGemInfo &info) {
    mLastGemTime = Max(mLastGemTime, info.ms + info.duration_ms);
    int curTrack = info.track;
    if (curTrack >= 100) {
        mFakeTracks[curTrack - 100]->mGems->AddMultiGem(0, info);
    } else {
        if (!mGemDBs[curTrack]->AddMultiGem(iii, info)) {
            MILO_WARN(
                "%s (%s): Overlapping or too-close gems at %s for difficulty %d",
                SongFullPath(),
                mTrackInfos[info.track]->mName,
                TickFormat(info.tick, *mMeasureMap),
                iii
            );
        }
    }
}

void SongData::AddVocalNote(const VocalNote &note) {
    mVocalNoteLists[mLoadingVocalNoteListIndex]->AddNote(note);
}

void SongData::AddPitchOffset(int tick, float offset) {
    mTuningOffsetList->AddInfo(tick, offset);
}

void SongData::AddLyricShift(int i) {
    mVocalNoteLists[mLoadingVocalNoteListIndex]->AddLyricShift(mTempoMap->TickToTime(i));
}

void SongData::OnTambourineGem(int i) {
    mVocalNoteLists[mLoadingVocalNoteListIndex]->AddTambourineGem(i);
}

void SongData::StartVocalPlayerPhrase(int i, int j) {
    mVocalNoteLists[mLoadingVocalNoteListIndex]->StartPlayerPhrase(i, j);
}

void SongData::EndVocalPlayerPhrase(int i, int j) {
    mVocalNoteLists[mLoadingVocalNoteListIndex]->EndPlayerPhrase(i, j);
}

void SongData::AddPhrase(
    BeatmatchPhraseType ty,
    int diff,
    int i3,
    float ms,
    int ticks,
    float dur_ms,
    int dur_ticks
) {
    int i1 = diff;
    if (diff == -1) {
        diff = 0;
        i1 = 3;
    }
    for (int d = diff; d <= i1; d++) {
        mPhraseDBs[i3]->AddPhrase(ty, d, ms, ticks, dur_ms, dur_ticks);
    }
}

void SongData::AddDrumFill(int track, int lanes, int startTick, int endTick, bool bre) {
    std::vector<DrumFillInfo *> &_ref0 = mFills;
    unsigned char fillBool = (unsigned char)(_ref0[track]->AddFill(startTick, endTick - startTick, bre));
    bool laneBool = _ref0[track]->AddLanes(startTick, lanes);
    if (!(fillBool & 1 & laneBool)) {
        MILO_WARN(
            "%s (%s): Error adding %s at %s",
            SongFullPath(),
            mTrackInfos[track]->mName,
            bre ? "Big Rock Ending lanes" : "drum fill",
            TickFormat(startTick, *mMeasureMap)
        );
    }
}

void SongData::AddRoll(
    int rollIdx, int dataIdx, unsigned int roll, int startTick, int endTick
) {
    if (rollIdx < 100)
        mRollInfos[rollIdx]->AddData(dataIdx, startTick, endTick, roll);
}

void SongData::AddTrill(
    int track, int dataIdx, int t1, int t2, int startTick, int endTick
) {
    if (track < 100)
        mTrillInfos[track]->AddData(dataIdx, startTick, endTick, std::make_pair(t1, t2));
}

void SongData::AddRGRoll(
    int track, int dataIdx, const RGRollChord &chord, int startTick, int endTick
) {
    mRGRollInfos[track]->AddData(dataIdx, startTick, endTick, chord);
}

void SongData::AddRGTrill(
    int track, int dataIdx, const RGTrill &trill, int startTick, int endTick
) {
    mRGTrillInfos[track]->AddData(dataIdx, startTick, endTick, trill);
}

void SongData::AddMix(int track, int tick, int diff, const char *mixName) {
    if (diff < 0 || diff >= mNumDifficulties) {
        MILO_WARN(
            "%s (%s): Error adding mix '%s' at %s; difficulty %d is out of range",
            SongFullPath(),
            mTrackInfos[track]->mName,
            mixName,
            TickFormat(tick, *mMeasureMap),
            diff
        );
    } else {
        if (!mDrumMixDBs[track]->AddMix(diff, tick, mixName)) {
            MILO_WARN(
                "%s (%s): Error adding mix '%s' at %s",
                SongFullPath(),
                mTrackInfos[track]->mName,
                mixName,
                TickFormat(tick, *mMeasureMap)
            );
        }
    }
}

void SongData::AddLyricEvent(int track, int tick, const char *lyricEvent) {
    if (mTrackInfos[track]->mLyrics->AddInfo(tick, lyricEvent))
        return;
    else
        MILO_WARN(
            "%s (%s): Error adding lyric event '%s' at %s",
            SongFullPath(),
            mTrackInfos[track]->mName,
            lyricEvent,
            TickFormat(tick, *mMeasureMap)
        );
}

void SongData::DrumMapLane(int track, int tick, int lane, bool laneOn) {
    if (laneOn) {
        if (!mDrumMaps[track]->LaneOn(tick, lane)) {
            MILO_WARN(
                "%s (%s): Error adding drum lane %d at %s",
                SongFullPath(),
                mTrackInfos[track]->mName,
                lane,
                TickFormat(tick, *mMeasureMap)
            );
        }
    } else {
        if (!mDrumMaps[track]->LaneOff(tick, lane)) {
            MILO_WARN(
                "%s (%s): Error ending drum lane %d at %s",
                SongFullPath(),
                mTrackInfos[track]->mName,
                lane,
                TickFormat(tick, *mMeasureMap)
            );
        }
    }
}

void SongData::AddBeat(int tick, int level) {
    if (!mBeatMap->AddBeat(tick, level)) {
        MILO_WARN(
            "%s (BEAT): Error adding beat at %s, probably double note",
            SongFullPath(),
            TickFormat(tick, *mMeasureMap)
        );
    }
}

void SongData::SetDetailedGrid(bool b) { mDetailedGrid = b; }
void SongData::AddRangeShift(int i, float f) { mRangeShifts[i] = f; }

void SongData::AddKeyboardRangeShift(int i1, int i2, float f3, int i4, int i5) {
    RangeSection sect(i1, f3);
    sect.unk8 = i4;
    sect.unkc = i5;
    std::vector<RangeSection> &curRanges = mKeyboardRangeSections[i1];
    if (sect.unkc == -1.0f) {
        float fSpan = 16.0f;
        if (curRanges.size() != 0) {
            fSpan = curRanges.back().unkc - curRanges.back().unk8;
            MILO_ASSERT(fSpan > 0.0f, 0x589);
        }
        sect.unkc += fSpan;
    }
    curRanges.push_back(sect);
}

void SongData::AddRGGem(int diff, const RGGemInfo &info) {
    mLastGemTime = Max(mLastGemTime, info.ms + info.duration_ms);
    if (!mGemDBs[info.track]->AddRGGem(diff, info)) {
        MILO_WARN(
            "%s, %s: Overlapping or too-close real guitar gems at tick %d (%s)",
            SongFullPath(),
            mTrackInfos[info.track]->mName,
            info.tick,
            TickFormat(info.tick, *mMeasureMap)
        );
    }
}

void SongData::SetFakeHitGemsInFill(bool b) { mFakeHitGemsInFill = b; }
bool SongData::GetFakeHitGemsInFill() const { return mFakeHitGemsInFill; }

unsigned int SongData::GetRollingSlotsAtTick(int i1, int tick) const {
    RangedDataCollection<unsigned int> *pRollInfo = mRollInfos[i1];
    MILO_ASSERT(pRollInfo, 0x5B4);
    return pRollInfo->GetDataAtTick(mTrackDifficulties[i1], tick);
}

bool SongData::RollStartsAt(int idx, int startTick, int &endTick) const {
    RangedDataCollection<unsigned int> *pRollInfo = mRollInfos[idx];
    MILO_ASSERT(pRollInfo, 0x5C2);
    return pRollInfo->DataStartsAt(mTrackDifficulties[idx], startTick, endTick);
}

bool SongData::GetNextRoll(int idx, int tick, unsigned int &roll, int &endTick) const {
    RangedDataCollection<unsigned int> *pRollInfo = mRollInfos[idx];
    MILO_ASSERT(pRollInfo, 0x5D0);
    int start;
    return pRollInfo->GetNextDataWithRange(
        mTrackDifficulties[idx], tick, roll, start, endTick
    );
}

bool SongData::TrillStartsAt(int idx, int startTick, int &endTick) {
    RangedDataCollection<std::pair<int, int> > *pTrillInfo = mTrillInfos[idx];
    MILO_ASSERT(pTrillInfo, 0x5F7);
    return pTrillInfo->DataStartsAt(mTrackDifficulties[idx], startTick, endTick);
}

RangedDataCollection<unsigned int> *SongData::GetRollInfo(int idx) const {
    RangedDataCollection<unsigned int> *pRollInfo = mRollInfos[idx];
    MILO_ASSERT(pRollInfo, 0x5DE);
    return pRollInfo;
}

bool SongData::GetTrillSlotsAtTick(int idx, int tick, std::pair<int, int> &p) const {
    RangedDataCollection<std::pair<int, int> > *pTrillInfo = mTrillInfos[idx];
    MILO_ASSERT(pTrillInfo, 0x5E8);
    return pTrillInfo->GetDataAtTick(mTrackDifficulties[idx], tick, p);
}

RangedDataCollection<std::pair<int, int> > *SongData::GetTrillInfo(int idx) const {
    RangedDataCollection<std::pair<int, int> > *pTrillInfo = mTrillInfos[idx];
    MILO_ASSERT(pTrillInfo, 0x605);
    return pTrillInfo;
}

bool SongData::RGRollStartsAt(int idx, int startTick, int &endTick) const {
    RangedDataCollection<RGRollChord> *pRollInfo = mRGRollInfos[idx];
    MILO_ASSERT(pRollInfo, 0x60F);
    return pRollInfo->DataStartsAt(mTrackDifficulties[idx], startTick, endTick);
}

bool SongData::GetNextRGRoll(int idx, int tick, RGRollChord &chord, int &endTick) const {
    RangedDataCollection<RGRollChord> *pRollInfo = mRGRollInfos[idx];
    MILO_ASSERT(pRollInfo, 0x61C);
    int start;
    return pRollInfo->GetNextDataWithRange(
        mTrackDifficulties[idx], tick, chord, start, endTick
    );
}

RGRollChord SongData::GetRGRollingSlotsAtTick(int i, int j) const {
    RangedDataCollection<RGRollChord> *pRollInfo = mRGRollInfos[i];
    MILO_ASSERT(pRollInfo, 0x62A);
    return pRollInfo->GetDataAtTick(mTrackDifficulties[i], j);
}

RangedDataCollection<RGRollChord> *SongData::GetRGRollInfo(int idx) const {
    RangedDataCollection<RGRollChord> *pRollInfo = mRGRollInfos[idx];
    MILO_ASSERT(pRollInfo, 0x636);
    return pRollInfo;
}

bool SongData::GetRGTrillAtTick(int idx, int i2, RGTrill &trill) const {
    RangedDataCollection<RGTrill> *pTrillInfo = mRGTrillInfos[idx];
    MILO_ASSERT(pTrillInfo, 0x640);
    return pTrillInfo->GetDataAtTick(mTrackDifficulties[idx], i2, trill);
}

RangedDataCollection<RGTrill> *SongData::GetRGTrillInfo(int idx) const {
    RangedDataCollection<RGTrill> *trillInfo = mRGTrillInfos[idx];
    MILO_ASSERT(trillInfo, 0x64D);
    return trillInfo;
}

bool SongData::RGTrillStartsAt(int track, int startTick, int &endTick) {
    RangedDataCollection<RGTrill> *pTrillInfo = mRGTrillInfos[track];
    MILO_ASSERT(pTrillInfo, 0x656);
    return pTrillInfo->DataStartsAt(mTrackDifficulties[track], startTick, endTick);
}

void SongData::RecalculateGemTimes(int track) {
    GetGemList(track)->RecalculateGemTimes(GetTempoMap());
}

void SongData::EnableGems(int i1, float f1, float f2) {
    std::vector<GameGem> &gems = GetGemList(i1)->mGems;
    for (unsigned int i = 0; i < gems.size(); i++) {
        GameGem &gem = gems[i];
        bool match = false;
        if (gem.mMs >= f1) {
            float endTime;
            if (!gem.mIgnoreDuration)
                endTime = gem.mMs + (float)(unsigned short)gem.mDurationMs;
            else
                endTime = gem.mMs;
            if (endTime <= f2) {
                match = true;
            }
        }
        if (!match) {
            gem.unk18 = 0x80;
        }
    }
}

GameGemList *SongData::GetGemListByDiff(int track, int diff) {
    return mGemDBs[track]->GetDiffGemList(diff);
}

GameGemList *SongData::GetGemList(int track) {
    return GetGemListByDiff(track, mTrackDifficulties[track]);
}

TickedInfoCollection<String> &SongData::GetSubmixes(int track) const {
    return mDrumMixDBs[track]->GetMixList(mTrackDifficulties[track]);
}

SongPos SongData::CalcSongPos(float f) {
    MILO_ASSERT(mTempoMap, 0x6BA);
    MILO_ASSERT(mMeasureMap, 0x6BB);
    MILO_ASSERT(mBeatMap, 0x6BC);
    float tick = mTempoMap->TimeToTick(f);
    int itick = tick;
    int m, b, t, x;
    mMeasureMap->TickToMeasureBeatTick(itick, m, b, t, x);
    float beat = mBeatMap->Beat(itick);
    return SongPos(tick, beat, m, b, t);
}

#ifdef HX_NATIVE
// Native-only vtable entry: SongData.h adds a HxSongData::CalcSongPos(HxMaster*,
// float) pure-virtual under HX_NATIVE (the native base keeps the dc3 shape). On
// the X360 retail slot layout CalcSongPos(float) above IS the base override and
// this extra entry does not exist. Delegate to the float overload.
SongPos SongData::CalcSongPos(class HxMaster *, float f) { return CalcSongPos(f); }
#endif

Symbol SongData::TrackName(int track) const { return mTrackInfos[track]->mName; }

int SongData::TrackNamed(Symbol name) const {
    for (int i = 0; i < mTrackInfos.size(); i++) {
        if (mTrackInfos[i]->mName == name)
            return i;
    }
    return -1;
}

int SongData::GetTrackTypes() const {
    int types = 0;
    for (int i = 0; i < mTrackInfos.size(); i++) {
        types |= 1 << mTrackInfos[i]->mType;
    }
    return types;
}

DrumFillInfo *SongData::GetDrumFillInfo(int idx) { return mFills[idx]; }

FillInfo *SongData::GetFillInfo(int idx) { return mFills[idx]; }

bool SongData::GetUsingRealDrums() const {
    return mPlayerTrackConfigList->UseRealDrums();
}

void SongData::SetUseDiscoUnflip(bool unflip) {
    if (mPlayerTrackConfigList->mDiscoUnflip != unflip) {
        mPlayerTrackConfigList->mDiscoUnflip = unflip;
        RestoreAllTracksFromBackup();
    }
}

void SongData::SetGameCymbalLanes(unsigned int ui) {
    MILO_ASSERT(mPlayerTrackConfigList, 0x714);
    mPlayerTrackConfigList->SetGameCymbalLanes(ui);
}

unsigned int SongData::GetGameCymbalLanes() const {
    MILO_ASSERT(mPlayerTrackConfigList, 0x71A);
    return mPlayerTrackConfigList->GetGameCymbalLanes();
}

DECOMP_FORCEACTIVE(SongData, "tracks")

const char *SongData::SongFullPath() const {
    if (!mSongPath.empty())
        return mSongPath.c_str();
    else
        return FakeSongMgr::MidiFullPath(mSongInfo);
}

VocalNoteList *SongData::GetVocalNoteList(int idx) {
    if (mPlayerTrackConfigList->UseVocalHarmony()) {
        if (idx + 1U < mVocalNoteLists.size()) {
            return mVocalNoteLists[idx + 1U];
        } else
            return nullptr;
    } else if (idx != 0)
        return nullptr;
    else
        return mVocalNoteLists.front();
}

int SongData::GetVocalNoteListCount() const {
    if (mPlayerTrackConfigList->UseVocalHarmony()) {
        return mVocalNoteLists.size() - 1;
    } else
        return 1;
}

std::vector<RangeSection> &SongData::GetKeyRangeSections(int idx) {
    return mKeyboardRangeSections[idx];
}

AudioTrackNum SongData::GetAudioTrackNum(int track) const {
    return mTrackInfos[track]->mAudioTrackNum;
}

bool SongData::GetGem(int gemId, int &startTick, int &endTick, int &slots) {
    if (mGems && gemId < mGems->NumGems()) {
        GameGem &curGem = mGems->GetGem(gemId);
        startTick = curGem.GetTick();
        endTick = startTick + curGem.GetDurationTicks();
        slots = curGem.GetSlots();
        return true;
    } else
        return false;
}

void SongData::SetTrack(Symbol name) {
    int trk = TrackNamed(name);
    mGems = nullptr;
    if (trk == -1) {
        for (int i = 0; i < mFakeTracks.size(); i++) {
            if (mFakeTracks[i]->mName == name) {
                mGems = mFakeTracks[i]->mGems->GetDiffGemList(0);
                return;
            }
        }
    } else {
        mGems = GetGemListByDiff(trk, GetNumDifficulties() - 1);
    }
}

SongData::BackupTrack::~BackupTrack() {
    delete mGems;
    delete mMixes;
}

void SongData::MakeBackupTracks() {
    for (int i = 0; i < mTrackInfos.size(); i++) {
        if (mTrackInfos[i]->mType == kTrackDrum) {
            mBackupTracks.push_back(
                new BackupTrack(i, mGemDBs[i]->Duplicate(), mDrumMixDBs[i]->Duplicate())
            );
        } else if (mTrackInfos[i]->mType == kTrackKeys) {
            mBackupTracks.push_back(new BackupTrack(i, mGemDBs[i]->Duplicate(), nullptr));
        }
    }
}

bool SongData::HasBackupTrack(int track) const {
    for (int i = 0; i < mBackupTracks.size(); i++) {
        if (track == mBackupTracks[i]->mOriginalTrack)
            return true;
    }
    return false;
}

void SongData::RestoreAllTracksFromBackup() {
    for (int i = 0; i < mNumTracks; i++) {
        if (HasBackupTrack(i)) {
            RestoreTrackFromBackup(i);
        }
    }
}

void SongData::RestoreTrackFromBackup(int track) {
    int backupIdx = 0;
    for (int i = 0; i < mBackupTracks.size(); i++) {
        if (mBackupTracks[i]->mOriginalTrack == track)
            break;
        backupIdx++;
    }
    if (backupIdx == mBackupTracks.size())
        MILO_FAIL("Trying to backup track %d with no backup\n", track);
    else {
        TrackType ty = mTrackInfos[track]->mType;
        if (ty == kTrackDrum) {
            if (mPlayerTrackConfigList->mDiscoUnflip) {
                for (int i = 0; i < mNumDifficulties; i++) {
                    UnflipGems(track, backupIdx, i);
                }
            } else {
                for (int i = 0; i < mNumDifficulties; i++) {
                    RestoreGems(track, backupIdx, i);
                }
            }
        } else if (ty == kTrackKeys) {
            if (mPlayerTrackConfigList->unk2c) {
                for (int i = 0; i < mNumDifficulties; i++) {
                    RestoreGems(track, backupIdx, i);
                }
            } else {
                for (int i = 0; i < mNumDifficulties; i++) {
                    TrimOverlappingGems(track, backupIdx, i);
                }
            }
        }
    }
}

SongData::FakeTrack::FakeTrack(Symbol sym) : mName(sym), mGems(new GameGemDB(1, 0x78)) {}
SongData::FakeTrack::~FakeTrack() { delete mGems; }
