#include "bandtrack/GemManager.h"
#include "bandobj/ArpeggioShape.h"
#include "bandtrack/NowBar.h"
#include "bandtrack/Track.h"
#include "beatmatch/FillInfo.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/RGUtl.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/Game.h"
#include "game/Player.h"
#include "game/SongDB.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MetaPerformer.h"
#include "game/TrainerPanel.h"
#include "game/GemTrainerPanel.h"
#include "game/GameConfig.h"
#include "game/PracticePanel.h"
#include "game/RGTrainerPanel.h"
#include "game/GemPlayer.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Group.h"
#include "rndobj/Trans.h"
#include "track/TrackWidget.h"
#include "utl/Std.h"
#include "math/Utl.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"

// Retail RB3 compiled this TU's MILO_WARN with arguments evaluated for side
// effects (the global sizeof()-form strips arg evaluation). Mirror MILO_FAIL's
// (void)(args) comma form per-TU so survivor args (virtual calls / TickFormat)
// are emitted while the message string vanishes.
#ifndef HX_NATIVE
#undef MILO_WARN
#define MILO_WARN(...) ((void)(__VA_ARGS__))
#endif

int sBeardThreshold = 480;

int GetBeardThreshold() { return sBeardThreshold; }

DataNode SetKeyGlow(DataArray *arr) {
    sBeardThreshold = arr->Int(1);
    return 0;
}

GemManager::GemManager(const TrackConfig &cfg, TrackDir *dir)
    : mTrackDir(dir), mTrackConfig(cfg), mTemplate(cfg),
      mConfig(SystemConfig("track_graphics")), mGemData(0), mGemsEnabledStart(0),
      mBegin(0), mEnd(0), unkb8(TheGame->mDrumFillsMod), mNowBar(0), mBonusGems(0), mInCoda(0), unkc4(0),
      unkc8(dir->SecondsToY(dir->TopSeconds())),
      unkcc(dir->SecondsToY(dir->BottomSeconds())), mTailsGrp(0), unkfc(0), unk100(0),
      unk104(0), mEnabledSlots(0), unk10c(0), mNextArpeggioPhrase(0), unk12c(0),
      unk130(-1), unk134(960) {
    mNowBar = new NowBar(mTrackDir, mTrackConfig);
    mTemplate.Init(mTrackDir->Find<ObjectDir>("gem_tail", true));
    static bool firstPass = true;
    if (firstPass) {
        sBeardThreshold =
            SystemConfig("track_graphics")->FindInt("key_glow_threshold_ticks");
        firstPass = false;
    }
    unk134 = SystemConfig("track_graphics")->FindInt("rg_run_space_ticks");
    SetupGems(0);
    UpdateLeftyFlip(false);
    DataRegisterFunc("set_key_glow", SetKeyGlow);
    RndDir *outline = mTrackDir->Find<RndDir>("chord_shape_outline", true);
    unk12c = outline->LocalXfm().v.y + 0.01f;
    unkd8.reserve(10);
}

GemManager::~GemManager() {
    RELEASE(mNowBar);
    ClearArpeggios();
    mGems.clear();
}

void GemManager::InitRGTuning(BandUser *bandUser) {
    MILO_ASSERT(bandUser, 0xAE);
    bool isRG = bandUser->GetTrack()->GetType() == real_guitar;
    bool isRB = bandUser->GetTrack()->GetType() == real_bass;
    if (isRG || isRB) {
        BandSongMetadata *metadata = (BandSongMetadata *)TheSongMgr.Data(
            TheSongMgr.GetSongIDFromShortName(MetaPerformer::Current()->Song(), true)
        );
        std::vector<int> vec18;
        if (isRG) {
            vec18.reserve(6);
            for (int i = 0; i < 6; i++) {
                vec18.push_back(metadata->RealGuitarTuning(i));
            }
        } else {
            vec18.reserve(4);
            for (int i = 0; i < 4; i++) {
                vec18.push_back(metadata->RealBassTuning(i));
            }
        }
        RGSetTuning(vec18);
    }
}

void GemManager::DrawTrackMasks(int i1, int i2) {
    for (int i = i2 != -1 ? i2 : i1; i <= i1; i += 0xf0) {
        if (i > unk10c) {
            int i174 = 0;
            int i3 = i;
            if (TheGame->InTrainer()) {
                i3 = GetLoopTick(i, i174);
            }
            i3 = TheSongDB->GetCommonPhraseID(mTrackConfig.TrackNum(), i3);
            Extent ext170(-1, -1);
            if (i3 != -1 && TheSongDB->IsUnisonPhrase(i3)) {
                if (TheSongDB->GetCommonPhraseExtent(
                        mTrackConfig.TrackNum(), i3, ext170
                    )) {
                    Symbol nameSym = mGemData->FindArray(unison, false)->Sym(1);
                    TrackWidget *w = GetWidgetByName(nameSym);
                    Transform tf98;
                    tf98.Reset();
                    tf98.v.y = mTrackDir->SecondsToY(TickToSeconds(ext170.unk0 + i174));
                    w->AddInstance(
                        tf98, TickToSeconds(ext170.unk4) - TickToSeconds(ext170.unk0)
                    );
                    unk10c = ext170.unk4 + i174;
                }
            }
        }
    }

    for (; mNextArpeggioPhrase < mArpeggioPhrases.size(); mNextArpeggioPhrase++) {
        ArpeggioPhrase *curPhrase = &mArpeggioPhrases[mNextArpeggioPhrase];
        if (curPhrase->mEndTick >= i2)
            continue;
        if (curPhrase->mStartTick > i1)
            break;
        const Gem &curGem = mGems[curPhrase->mGemId];
        ArpeggioShapePool *pool = mTrackDir->GetArpeggioShapePool();
        ArpeggioShape *poolShape = pool->GetArpeggioShape();
        bool lefty = mTrackConfig.IsLefty();
        float f11 = mTrackDir->SecondsToY(TickToSeconds(curPhrase->mStartTick));
        if (curPhrase->unk10) {
            poolShape->ShowChordShape(false);
        } else {
            Symbol nameSym = mGemData->FindArray(arpeggio, false)->Sym(1);
            TrackWidget *w5 = GetWidgetByName(nameSym);
            Transform tfc8;
            tfc8.Reset();
            tfc8.v.y = f11;
            int i10 = curPhrase->mEndTick;
            if (TheTrainerPanel && TheGame->InTrainer()) {
                i10 = Min(
                    curPhrase->mEndTick,
                    (curPhrase->mStartTick
                     - (GetLoopTick(curPhrase->mStartTick)
                        - TheTrainerPanel->GetCurrentStartTick()))
                        + TheTrainerPanel->GetLoopTicks(TheTrainerPanel->GetCurrSection())
                );
                curPhrase->mEndTick = i10;
            }
            w5->AddInstance(
                tfc8, TickToSeconds(i10) - TickToSeconds(curPhrase->mStartTick)
            );
            RndMesh *mesh = mTrackDir->GetChordMesh(curGem.unk_0x48, lefty);
            poolShape->SetChordShape(mesh);
            poolShape->ShowChordShape(true);
            String str168;
            int i180 = -1;
            curGem.GetChordFretLabelInfo(str168, i180);
            Transform tff8;
            mTrackDir->MakeSlotXfm(i180, tff8);
            Symbol s184;
            if (GetChordWidgetName(normal, chord_fret, s184)) {
                TrackWidget *w10 = GetWidgetByName(s184);
                if (w10)
                    w10->ApplyOffsets(tff8);
            }
            poolShape->SetFretNumber(str168, tff8.v);
        }
        poolShape->SetYPos(f11);
        poolShape->SetChordLabel(
            curGem.mChordLabel, mTrackDir->GetCurrentChordLabelPosOffset(), lefty
        );
        poolShape->HookupToParentGroup();
        curPhrase->mShape = poolShape;
        mActiveArpeggios.push_back(curPhrase);
    }
}

ArpeggioShapePool *TrackDir::GetArpeggioShapePool() { return nullptr; }
float TrackDir::GetCurrentChordLabelPosOffset() const { return 0; }

void GemManager::ClearArpeggios() {
    ArpeggioShapePool *pool = mTrackDir->GetArpeggioShapePool();
    while (!mActiveArpeggios.empty()) {
        pool->ReleaseArpeggioShape(mActiveArpeggios.back()->mShape);
        mActiveArpeggios.pop_back();
    }
    while (!mExpiredArpeggios.empty()) {
        pool->ReleaseArpeggioShape(mExpiredArpeggios.back()->mShape);
        mExpiredArpeggios.pop_back();
    }
}

void GemManager::ResetArpeggios(float f1) {
    ClearArpeggios();
    mNextArpeggioPhrase = 0;
    int tick = MsToTickInt(f1);
    for (; mNextArpeggioPhrase < mArpeggioPhrases.size()
         && mArpeggioPhrases[mNextArpeggioPhrase].mEndTick < tick;
         mNextArpeggioPhrase++)
        ;
}

void GemManager::UpdateArpeggios(float f1, bool b2) {
    float ms = mTrackDir->YToSeconds(unk12c) * 1000.0f + f1;
    int i1 = MsToTickInt(ms);
    while (!mActiveArpeggios.empty()) {
        ArpeggioPhrase *currentArpeggio = mActiveArpeggios.front();
        MILO_ASSERT(currentArpeggio->mShape, 0x185);
        if (currentArpeggio->mEndTick < i1) {
            currentArpeggio->mShape->FadeOutChordShape();
            mExpiredArpeggios.push_back(currentArpeggio);
            mActiveArpeggios.erase(mActiveArpeggios.begin());
        } else {
            if (!b2 || !currentArpeggio->unk10) {
                float ms2 = ms;
                if (currentArpeggio->mStartTick > i1) {
                    ms2 = TickToMs(currentArpeggio->mStartTick);
                }
                currentArpeggio->mShape->SetYPos(mTrackDir->SecondsToY(ms2 / 1000.0f));
            }
            break;
        }
    }
    ms = mTrackDir->SecondsToY((f1 / 1000.0f + mTrackDir->BottomSeconds()) - 0.5f);
    while (!mExpiredArpeggios.empty() && mExpiredArpeggios.front()->mShape->GetYPos() < ms
    ) {
        MILO_ASSERT(mExpiredArpeggios.front()->mShape, 0x1A8);
        mTrackDir->GetArpeggioShapePool()->ReleaseArpeggioShape(
            mExpiredArpeggios.front()->mShape
        );
        mExpiredArpeggios.erase(mExpiredArpeggios.begin());
    }
}

void GemManager::ClearTrackMasks() {
    if (mGemData) {
        DataArray *arpArr = mGemData->FindArray(arpeggio, false);
        if (arpArr) {
            Symbol name = arpArr->Sym(1);
            GetWidgetByName(name)->Clear();
        }
        DataArray *unisonArr = mGemData->FindArray(unison, false);
        if (unisonArr) {
            Symbol name = unisonArr->Sym(1);
            GetWidgetByName(name)->Clear();
        }
        unk10c = 0;
    }
}
void GemManager::SetupRealGuitarFretPos() {
    const BandUser *bandUser = mTrackConfig.GetBandUser();
    bool isRG = bandUser->GetTrack()->GetType() == real_guitar;
    bool isRB = bandUser->GetTrack()->GetType() == real_bass;
    if (isRG || isRB) {
        std::vector<GameGem> gameGems;
        int i2 = -1;
        int i38 = 0;
        int i3c = -1;
        for (int i = 0; i < mGems.size(); i++) {
            const GameGem &curGameGem = mGems[i].GetGameGem();
            if (i3c != -1 && curGameGem.GetTick() > i3c) {
                ProcessRealGuitarRun(gameGems, i38);
                i2 = curGameGem.GetLowestString();
                i3c = -1;
                if (!curGameGem.IsRealGuitarChord()) {
                    gameGems.push_back(curGameGem);
                }
            } else if (curGameGem.IsRealGuitarChord()) {
                ProcessRealGuitarRun(gameGems, i38);
                i2 = -1;
                i38++;
            } else if (curGameGem.IsMuted()) {
                ProcessRealGuitarRun(gameGems, i38);
                i2 = curGameGem.GetLowestString();
                i38++;
            } else if (i2 != (int)curGameGem.GetLowestString()) {
                ProcessRealGuitarRun(gameGems, i38);
                i2 = curGameGem.GetLowestString();
                gameGems.push_back(curGameGem);
            } else {
                if (!gameGems.empty()) {
                    GameGem &last = gameGems.back();
                    if (curGameGem.GetTick() - last.GetTick() > unk134) {
                        ProcessRealGuitarRun(gameGems, i38);
                        i2 = curGameGem.GetLowestString();
                        gameGems.push_back(curGameGem);
                        continue;
                    }
                }
                if (i3c == -1) {
                    if (TrillStartsAt(mTrackConfig.TrackNum(), curGameGem, i3c)) {
                        ProcessRealGuitarRun(gameGems, i38);
                        i2 = curGameGem.GetLowestString();
                        gameGems.push_back(curGameGem);
                        continue;
                    }
                }
                gameGems.push_back(curGameGem);
            }
        }
        ProcessRealGuitarRun(gameGems, i38);
    }
}

void GemManager::ProcessRealGuitarRun(std::vector<GameGem> &gems, int &iref) {
    if (!gems.empty()) {
        if (gems.size() == 1) {
            mGems[iref].SetFretPos(2);
        } else {
            int i3 = mGems[iref].GetGameGem().GetLowestString();
            unsigned int u10 = 0;
            for (int i = 0; i < gems.size(); i++) {
                const GameGem &curGem = mGems[iref + i].GetGameGem();
                u10 |= 1 << curGem.GetFret(i3);
            }
            int i2 = -1;
            int i9 = 0;
            int i5c = 100000;
            int i60 = -1;
            for (int i = 0; i < 0x16; i++) {
                if (u10 & 1 << i) {
                    i5c = std::min(i5c, i);
                    i60 = std::max(i60, i);
                    i9++;
                }
            }
            if (i9 > 5) {
                i9 = 2;
                i2 = mGems[iref].GetGameGem().GetFret(i3);
                if (i60 - i2 < 5) {
                    i9 = 4 - (i60 - i2);
                } else {
                    if (i2 - i5c < 5) {
                        i9 = i2 - i5c;
                    }
                }
                mGems[iref].SetFretPos(i9);
                for (int i = 1; i < gems.size(); i++) {
                    int i1 = mGems[iref + i].GetGameGem().GetFret(i3);
                    if (i1 > i2) {
                        i9 = (i9 + 1) % 5;
                        i2 = i1;
                    } else if (i1 < i2) {
                        i9--;
                        if (i9 < 0) {
                            i9 = 4;
                        }
                        i2 = i1;
                    }
                    mGems[iref + i].SetFretPos(i9);
                }
            } else {
                if (i5c == i60) {
                    for (int i = 0; i < gems.size(); i++) {
                        mGems[iref + i].SetFretPos(2);
                    }
                } else {
                    float scale = 5.0f / (i60 - i5c);
                    for (int i = 0; i < gems.size(); i++) {
                        float gemfloat = mGems[iref + i].GetGameGem().GetFret(i3) - i5c;
                        int i68 = scale * gemfloat;
                        i68 = std::min(i68, 4);
                        mGems[iref + i].SetFretPos(i68);
                    }
                }
            }
        }
        iref += gems.size();
        gems.clear();
    }
}

void GemManager::SetupRealGuitarImportantStrings() {
    static Symbol real_guitar("real_guitar");
    static Symbol real_bass("real_bass");
    const BandUser *bandUser = mTrackConfig.GetBandUser();
    bool isRG = bandUser->GetTrack()->GetType() == real_guitar;
    bool isRB = bandUser->GetTrack()->GetType() == real_bass;
    if (isRG || isRB) {
        for (int i = 1; i < mGems.size(); i++) {
            const GameGem &cur = mGems[i].GetGameGem();
            const GameGem &prev = mGems[i - 1].GetGameGem();
            unsigned int curID = cur.GetRGChordID();
            if (curID == (unsigned int)prev.GetRGChordID() && !cur.IsMuted() &&
                cur.IsRealGuitarChord()) {
                const_cast<GameGem &>(cur).SetImportantStrings(prev.GetImportantStrings());
            } else if (!prev.IsMuted() && !cur.IsMuted()) {
                int prevSlots = prev.NumSlots();
                int curSlots = cur.NumSlots();
                unsigned char bits = 0;
                if (prevSlots >= 3 && curSlots >= 3) {
                    int matches = 0;
                    unsigned int s;
                    for (s = 0; s < 6; s++) {
                        int curFret = cur.GetFret(s);
                        if (curFret != prev.GetFret(s) &&
                            (cur.mTick - prev.mTick) < 0x780 &&
                            cur.GetFret(s) != -1 &&
                            cur.GetRGNoteType(s) != 1 &&
                            prev.GetFret(s) != -1 &&
                            prev.GetRGNoteType(s) != 1) {
                            matches++;
                            bits |= (unsigned char)(1 << s);
                        }
                    }
                    if (matches > 0 && matches <= 2) {
                        const_cast<GameGem &>(cur).SetImportantStrings(bits);
                        if (prev.GetImportantStrings() == 0) {
                            const_cast<GameGem &>(prev).SetImportantStrings(bits);
                        }
                    }
                }
            }
            if (cur.GetImportantStrings() == 0 && !cur.IsMuted() &&
                cur.IsRealGuitarChord()) {
                int localIntervals[4] = { 10, 11, 4, 3 };
                unsigned char root = cur.GetRootNote();
                unsigned char strings;
                for (int j = 0; j < 4 && cur.GetImportantStrings() == 0; j++) {
                    RGStringContainsNote(
                        (unsigned char)((root + localIntervals[j]) % 12), cur, strings
                    );
                    if (strings != 0 && GameGem::CountBitsInSlotType(strings) <= 2) {
                        const_cast<GameGem &>(cur).SetImportantStrings(strings);
                    }
                }
            }
        }
    }
}

void GemManager::SetupRealGuitarAreaStrumSections() {
    const std::vector<GameGem> &gems =
        TheSongDB->GetGems(mTrackConfig.TrackNum());
    for (int i = 0; i < gems.size();) {
        const GameGem &gem = gems[i];
        if (gem.GetRGStrumType() == 0) {
            i++;
        } else {
            mGems[i].mIsRepeatChord = false;
            for (int j = i - 1; j >= 0; j--) {
                if ((unsigned int)gems[j].GetRGChordID()
                    != (unsigned int)gem.GetRGChordID()) {
                    break;
                }
                mGems[j].mIsRepeatChord = false;
            }
            for (i++; i < gems.size(); i++) {
                if ((unsigned int)gems[i].GetRGChordID()
                    != (unsigned int)gem.GetRGChordID()) {
                    break;
                }
                mGems[i].mIsRepeatChord = false;
            }
        }
    }
}


void GemManager::SetupGems(int startTick) {
    Symbol song = MetaPerformer::Current()->Song();
    if (song == gNullStr)
        return;

    auto _tmp2 = TheSongMgr.GetSongIDFromShortName(song, true);
    BandSongMetadata *metadata = (BandSongMetadata *)TheSongMgr.Data(
        _tmp2
    );
    int songKey = metadata->SongKey();
    BandUser *bandUser = const_cast<BandUser *>(mTrackConfig.GetBandUser());
    int trackNum = mTrackConfig.TrackNum();
    const std::vector<GameGem> &gems = TheSongDB->GetGems(trackNum);
    TheSongDB->GetSongDurationMs();
    InitRGTuning(bandUser);
    bool tonalityNonZero = metadata->SongTonality() != 0;

    float sectionStart = 0.0f;
    float sectionEnd = 0.0f;
    if (TheGame->mProperties.mHasSongSections) {
        ClearGems(false);
        int s1, s2;
        TheGameConfig->GetPracticeSections(s1, s2);
        float unused;
        TheGameConfig->GetSectionBounds(s1, sectionStart, unused);
        TheGameConfig->GetSectionBounds(s2, unused, sectionEnd);
    }

    mHitGems.clear();
    mMissedPhrases.clear();
    mGems.clear();
    mGems.reserve(gems.size());
    mArpeggioPhrases.clear();
    bandUser->GetSlot();
    int hasSongSections = TheGame->mProperties.mHasSongSections;
    unsigned int gameCymbalLanes = mTrackConfig.GetGameCymbalLanes();

    bool inTrill = false;
    std::pair<int, int> trillSlots;
    trillSlots.first = 0;
    trillSlots.second = 0;
    int nextSlotForTrill = -1;
    int lastArpeggioEndTick = -1;
    int nextFretForTrill = -1;
    int trillString = -1;
    int arrhythmicEndTick = -1;
    unk130 = -1;
    mNextArpeggioPhrase = 0;
    ClearArpeggios();
    ClearTrackMasks();

    int repeatedChordGemId = -1;
    int repeatedChordStartTick = -1;
    int repeatedChordEndTick = -1;
    mTrackDir->ClearChordMeshRefCounts();

    bool anyRGChord = false;
    bool anyRG = false;

    for (unsigned int i = 0; i < gems.size(); i++) {
        const GameGem &gem = gems[i];
        float startMs = gem.mMs;
        bool noTail = false;
        if (gem.mIgnoreDuration && !gem.LeftHandSlide()) {
            noTail = true;
        }
        float endMs;
        if (noTail) {
            endMs = startMs;
        } else {
            endMs = startMs + gem.mDurationMs;
        }

        unsigned int slots = 0;
        int gemTick = gem.mTick;
        bool isHopo = false;
        bool isInFill = false;
        if (((unkb8 && bandUser->GetTrackType() != 0) ||
             TheSongDB->IsInCoda(gemTick)) &&
            TheGame->mProperties.mEnableCoda) {
            isInFill = true;
        }
        if (!isInFill ||
            !TheSongDB->GetFillInfo(trackNum, gemTick)->FillAt(gem.mTick, false)) {
            slots = gem.mSlots;
            if (gem.mForceStrum && ((int)i >= 1 || gem.IsRealGuitar())) {
                isHopo = true;
            }
            if (!TheGame->mProperties.mInPracticeMode &&
                !TheGame->mProperties.mInTrainer && gem.mTick < startTick) {
                slots = 0;
            }
            if (hasSongSections &&
                !(startMs >= sectionStart && endMs < sectionEnd)) {
                slots = 0;
            }
        }

        Gem newGem(gem, slots, startMs / 1000.0f, endMs / 1000.0f, isHopo, -1, songKey, tonalityNonZero);
        newGem.mGemManager = this;

        if (gem.mIsCymbal) {
            int slotIdx = gem.GetSlot();
            if ((1U << slotIdx) & gameCymbalLanes) {
                newGem.mIsCymbalLane = true;
            }
        }

        if (slots != 0) {
            SongData *songData = TheSongDB->GetData();
            MILO_ASSERT(songData, 0x39F);
            bool inArrhythmic = false;
            int otherSlot = -1;
            unsigned int rollSlots = 0;
            bool justStartedArrhythmic = false;
            if (RollStartsAt(trackNum, gem, otherSlot, rollSlots)) {
                arrhythmicEndTick = otherSlot;
                inArrhythmic = !mTrackConfig.IsKeyboardTrack();
                justStartedArrhythmic = true;
                if (mTrackConfig.IsDrumTrack()) {
                    if (rollSlots != gem.mSlots) {
                        unsigned int diff = rollSlots & ~gem.mSlots;
                        int otherSlot = -1;
                        while (diff) {
                            diff >>= 1;
                            otherSlot++;
                        }
                        MILO_ASSERT(otherSlot >= 0, 0x3C6);
                        nextSlotForTrill = otherSlot;
                    }
                }
            } else if (TrillStartsAt(trackNum, gem, otherSlot)) {
                inArrhythmic = true;
                justStartedArrhythmic = true;
                if (gem.IsRealGuitar()) {
                    RGTrill trill;
                    songData->GetRGTrillAtTick(trackNum, GetLoopTick(gem.mTick), trill);
                    nextFretForTrill = trill.mFrets[0];
                    if (gem.GetFret() == trill.mFrets[0]) {
                        nextFretForTrill = trill.mFrets[1];
                    }
                    trillString = gem.GetLowestString();
                } else {
                    songData->GetTrillSlotsAtTick(trackNum, GetLoopTick(gem.mTick), trillSlots);
                    int slotIdx = gem.GetSlot();
                    nextSlotForTrill = trillSlots.first;
                    if (slotIdx == nextSlotForTrill) {
                        nextSlotForTrill = trillSlots.second;
                    }
                }
                arrhythmicEndTick = otherSlot;
                MILO_ASSERT(inTrill == false, 0x3EE);
                inTrill = true;
            } else if (nextSlotForTrill != -1) {
                int slotIdx = gem.GetSlot();
                if (nextSlotForTrill != slotIdx) {
                    MILO_WARN(
                        "Trill at %0.1f ms. doesn't have alternating slots. Check for earlier notifies!",
                        startMs
                    );
                }
                MILO_ASSERT(arrhythmicEndTick > gem.GetTick(), 0x3FA);
                inArrhythmic = true;
                justStartedArrhythmic = true;
                nextSlotForTrill = -1;
            } else if (nextFretForTrill != -1) {
                if (arrhythmicEndTick <= gem.mTick) {
                    MILO_WARN(
                        "Trill ending at %0.1f ms. doesn't have a second note to trill to.",
                        TickToMs((float)arrhythmicEndTick)
                    );
                    nextFretForTrill = -1;
                } else {
                    MILO_ASSERT(trillString == gem.GetLowestString(), 0x40D);
                    nextFretForTrill = -1;
                    justStartedArrhythmic = true;
                }
            } else if (gem.mTick < arrhythmicEndTick) {
                justStartedArrhythmic = true;
            }

            if (inArrhythmic) {
                float fStartTimeMs = gem.mMs;
                float fEndTimeMs = TickToMs((float)arrhythmicEndTick);
                MILO_ASSERT(fStartTimeMs >= 0.0f, 0x421);
                MILO_ASSERT(fEndTimeMs > fStartTimeMs, 0x422);
                newGem.mArrhythmicDurationSeconds = (fEndTimeMs - fStartTimeMs) / 1000.0f;
            }
            if (justStartedArrhythmic && mTrackConfig.IsKeyboardTrack() && !inTrill) {
                newGem.mInArrhythmic = true;
            }
            if (inTrill && i < gems.size() - 1) {
                MILO_ASSERT(arrhythmicEndTick > -1, 0x434);
                if (gems[i + 1].mTick > arrhythmicEndTick) {
                    inTrill = false;
                    arrhythmicEndTick = -1;
                }
            }
        }

        if (gem.IsRealGuitarChord() && slots != 0) {
            if (mTrackDir != NULL) {
                int chordA = newGem.unk_0x44;
                int chordB = newGem.unk_0x48;
                bool chordAOk = false;
                if (mTrackDir->PrepareChordMesh(chordA) != 0 || anyRGChord) {
                    chordAOk = true;
                }
                anyRGChord = chordAOk;
                if (chordB != chordA) {
                    anyRGChord = false;
                    if (mTrackDir->PrepareChordMesh(chordB) != 0 || chordAOk) {
                        anyRGChord = true;
                    }
                }
            } else {
                MILO_WARN("No track dir in setup gems, so chord meshes can't be built");
            }
        }
        if (gem.IsRealGuitar()) {
            anyRG = true;
        }

        int phraseStart = -1;
        int phraseEnd = -1;
        if (gem.IsRealGuitar() && slots != 0) {
            if (gem.mTick < lastArpeggioEndTick) {
                MILO_ASSERT(!mArpeggioPhrases.empty(), 0x476);
                ArpeggioPhrase &phrase = mArpeggioPhrases.back();
                MILO_ASSERT(phrase.mEndTick == lastArpeggioEndTick, 0x47A);
                bool matches = true;
                const GameGem &prevGem = gems[phrase.mGemId];
                for (int s = 0; s < 6; s++) {
                    signed char curFret = gem.GetFret(s);
                    signed char prevFret = prevGem.GetFret(s);
                    if (curFret != -1 && curFret != prevFret) {
                        matches = false;
                        break;
                    }
                }
                if (matches) {
                    if (gem.IsRealGuitarChord()) {
                        newGem.mIsRepeatChord = true;
                        newGem.mSuppressChordLabel = true;
                    }
                    newGem.mInArpeggio = true;
                }
            } else {
                int searchTick = gem.mTick;
                if (gem.mTick == lastArpeggioEndTick) {
                    searchTick = gem.mTick + 1;
                }
                if (TheSongDB->GetPhraseExtents(
                        (BeatmatchPhraseType)4, trackNum, searchTick, phraseStart, phraseEnd
                    )) {
                    if (!newGem.unk_0x44) {
                        MILO_WARN(
                            "Ignoring invalid arpeggio phrase at %s; must begin with a chord",
                            TickFormat(phraseStart, *TheSongDB->GetData()->GetMeasureMap())
                        );
                    } else {
                        EndRepeatedChordPhrase(repeatedChordStartTick, repeatedChordEndTick, repeatedChordGemId);
                        if (TheTrainerPanel && TheGame->mProperties.mInTrainer) {
                            int loopTick = GetLoopTick(phraseStart);
                            int offset = loopTick - TheTrainerPanel->GetCurrentStartTick();
                            int adjustedEnd =
                                phraseStart - offset +
                                TheTrainerPanel->GetLoopTicks(TheTrainerPanel->GetCurrSection());
                            if (adjustedEnd < phraseEnd) {
                                phraseEnd = adjustedEnd;
                            }
                        }
                        ArpeggioPhrase phrase(phraseStart, phraseEnd, i);
                        mArpeggioPhrases.push_back(phrase);
                        lastArpeggioEndTick = phraseEnd;
                        newGem.mSuppressFretLabel = true;
                        newGem.mSuppressChordLabel = true;
                        newGem.mInArpeggio = true;
                    }
                }
            }
        }

        bool isImmediate = false;
        if (i > 0) {
            const GameGem &prevGem = gems[i - 1];
            isImmediate = gem.mMs <
                (1000.0f * mTrackDir->ViewTimeSeconds()) + (prevGem.mMs + (float)prevGem.mDurationMs);
        }
        int rgChordID = gem.GetRGChordID();
        if (rgChordID == unk130 && gem.IsRealGuitarChord() && isImmediate) {
            newGem.mIsRepeatChord = true;
            if (!gem.IsMuted() && gem.mTick >= lastArpeggioEndTick) {
                int endTick = gem.mTick;
                bool skipDuration = false;
                if (gem.mIgnoreDuration || gem.LeftHandSlide()) {
                    skipDuration = true;
                }
                if (!skipDuration) {
                    endTick += gem.mDurationTicks;
                }
                repeatedChordEndTick = endTick;
                newGem.mSuppressChordLabel = true;
            }
        } else {
            EndRepeatedChordPhrase(repeatedChordStartTick, repeatedChordEndTick, repeatedChordGemId);
            if (gem.IsRealGuitarChord() && !gem.IsMuted()) {
                unk130 = rgChordID;
            } else {
                unk130 = -1;
            }
            if (gem.IsRealGuitarChord() && !gem.IsMuted() && gem.mTick >= lastArpeggioEndTick) {
                repeatedChordGemId = i;
                repeatedChordStartTick = gem.mTick;
                int endTick = gem.mTick;
                bool skipDuration = false;
                if (gem.mIgnoreDuration || gem.LeftHandSlide()) {
                    skipDuration = true;
                }
                if (!skipDuration) {
                    endTick += gem.mDurationTicks;
                }
                repeatedChordEndTick = endTick;
                newGem.mSuppressChordLabel = true;
            }
        }
        if (i == gems.size() - 1) {
            EndRepeatedChordPhrase(repeatedChordStartTick, repeatedChordEndTick, repeatedChordGemId);
        }

        if (gem.LeftHandSlide()) {
            bool hasNext = false;
            signed char curFret = gem.GetFret(gem.GetLowestString());
            if (i < gems.size() - 1) {
                const GameGem &nextGem = gems[i + 1];
                bool tailFlag = false;
                hasNext = nextGem.mTick - (gem.mTick + gem.mDurationTicks) <= 0x78;
                bool slotsEqual = ((gem.mSlots - nextGem.mSlots) == 0);
                if (hasNext && slotsEqual && nextGem.mForceStrum) {
                    tailFlag = true;
                }
                if (tailFlag) {
                    newGem.mTailStart = nextGem.mMs / 1000.0f;
                }
                if (hasNext) {
                    signed char nextFret = nextGem.GetFret(nextGem.GetLowestString());
                    newGem.mSlideUp = (nextFret > curFret);
                }
            }
            if (!hasNext) {
                newGem.mSlideUp = (curFret <= 7);
            }
            if (gem.ReverseSlide()) {
                newGem.mSlideUp = !newGem.mSlideUp;
            }
        }

        mGems.push_back(newGem);
    }

    mTrackDir->DeleteUnusedChordMeshes();
    if (anyRGChord) {
        mTrackDir->SyncObjects();
    } else if (anyRG) {
        mTrackDir->SyncFingerFeedback();
    }

    mEnd = 0;
    mBegin = 0;
    if (TheGame->mProperties.mHasSongSections) {
        TheSongDB->EnableGems(trackNum, sectionStart, sectionEnd);
    }
    SetupRealGuitarFretPos();
    SetupRealGuitarImportantStrings();
    SetupRealGuitarAreaStrumSections();
}

void TrackDir::ClearChordMeshRefCounts() {}
int TrackDir::PrepareChordMesh(unsigned int) { return 0; }
void TrackDir::DeleteUnusedChordMeshes() {}
void TrackDir::SyncFingerFeedback() {}

void GemManager::EndRepeatedChordPhrase(
    int &repeatedChordStartTick, int &repeatedChordEndTick, int &i3
) {
    if (i3 != -1) {
        MILO_ASSERT(repeatedChordStartTick != -1, 0x56D);
        MILO_ASSERT(repeatedChordEndTick != -1, 0x56E);
        ArpeggioPhrase phrase(repeatedChordStartTick, repeatedChordEndTick, i3);
        phrase.unk10 = true;
        mArpeggioPhrases.push_back(phrase);
        i3 = -1;
        repeatedChordStartTick = -1;
        repeatedChordEndTick = -1;
    }
}

bool GemManager::RollStartsAt(int i1, const GameGem &gem, int &iref, unsigned int &uiref)
    const {
    int tick = GetLoopTick(gem.GetTick());
    bool ret;
    if (gem.IsRealGuitar()) {
        ret = TheSongDB->GetData()->RGRollStartsAt(i1, tick, iref);
    } else {
        ret = TheSongDB->GetData()->RollStartsAt(i1, tick, iref);
    }
    if (ret) {
        uiref = TheSongDB->GetData()->GetRollingSlotsAtTick(i1, tick);
    }
    iref += gem.GetTick() - tick;
    return ret;
}

bool GemManager::TrillStartsAt(int i1, const GameGem &gem, int &iref) const {
    int tick = GetLoopTick(gem.GetTick());
    bool ret;
    if (gem.IsRealGuitar()) {
        ret = TheSongDB->GetData()->RGTrillStartsAt(i1, tick, iref);
    } else {
        ret = TheSongDB->GetData()->TrillStartsAt(i1, tick, iref);
    }
    if (ret) {
        iref += gem.GetTick() - tick;
    }
    return ret;
}

void GemManager::SetGemsEnabled(float f) {
    mGemsEnabledStart = f;
    UpdateGemStates();
}

void GemManager::UpdateLeftyFlip(bool poll) {
    ClearGems(true);
    Player *player = mTrackConfig.GetBandUser()->GetPlayer();
    if (player) {
        GemStatus *gemStatus = ((GemPlayer *)player)->mGemStatus;
        MILO_ASSERT(gemStatus, 0x5B1);
        if (gemStatus->GetSize() > 0) {
            MILO_ASSERT(gemStatus->GetSize() == mGems.size(), 0x5BA);
            while (mBegin < mGems.size() && gemStatus->Get0xD(mBegin)) {
                AdvanceBegin();
            }
        }
    }
    RndText::Alignment alignment =
        mTrackConfig.IsLefty() ? RndText::kBottomLeft : RndText::kBottomRight;
    TrackWidget *chordLabelWidget =
        mTrackDir->Find<TrackWidget>("chord_label.wid", true);
    chordLabelWidget->SetTextAlignment(alignment);
    Symbol type = mTrackConfig.Type();
    static Symbol real_keys("real_keys");
    if (type != real_keys) {
        RndDir *smasher = mTrackDir->SmasherPlate();
        if (smasher) {
            auto _tmp1 = mTrackConfig.GetBandUser()->GetControllerType();
            bool isKeys = _tmp1 == kControllerKeys;
            static Message msg("set_lefty", 0);
            msg[0] = mTrackConfig.UseLeftyGems() && !isKeys;
            smasher->Handle(msg, true);
        }
    }
    UpdateSlotPositions();
    if (type == "drum") {
        if (mTrackConfig.UseLeftyGems()) {
            type = Symbol("drum_lefty");
        }
        Symbol widgetName;
        float crashY = 0.0f;
        float beardY = 0.0f;
        static Symbol crash("crash");
        static Symbol crash_cymbal("crash_cymbal");
        static Symbol beard("beard");
        if (mGemData) {
            if (GetWidgetName(widgetName, 4, crash)) {
                TrackWidget *w = GetWidgetByName(widgetName);
                if (!w->Empty()) {
                    crashY = w->GetFirstInstanceY();
                    w->Clear();
                }
            }
            if (GetWidgetName(widgetName, 4, crash_cymbal)) {
                TrackWidget *w = GetWidgetByName(widgetName);
                if (!w->Empty()) {
                    crashY = w->GetFirstInstanceY();
                    w->Clear();
                }
            }
            if (GetWidgetName(widgetName, 4, beard)) {
                TrackWidget *w = GetWidgetByName(widgetName);
                if (!w->Empty()) {
                    beardY = w->GetFirstInstanceY();
                    w->Clear();
                }
            }
        }
        mGemData = mConfig->FindArray(Symbol("gem"), Symbol("gems"), type);
        Symbol cymbalSym =
            (mTrackConfig.GetGameCymbalLanes() & 0x10) ? crash_cymbal : crash;
        if ((double)crashY != 0.0 && GetWidgetName(widgetName, 4, cymbalSym)) {
            TrackWidget *w = GetWidgetByName(widgetName);
            Transform xfm1;
            mTrackDir->MakeSlotXfm(4, xfm1);
            xfm1.v.y = crashY;
            w->AddInstance(xfm1, 0.0f);
        }
        if ((double)beardY != 0.0 && GetWidgetName(widgetName, 4, beard)) {
            TrackWidget *w = GetWidgetByName(widgetName);
            Transform xfm2;
            mTrackDir->MakeSlotXfm(4, xfm2);
            xfm2.v.y = beardY;
            w->AddInstance(xfm2, 0.0f);
        }
    } else {
        mGemData = mConfig->FindArray(Symbol("gem"), Symbol("gems"), type);
    }
    UpdateGemStates();
    float gameMs = TheGame->mLastPollMs;
    ResetArpeggios(gameMs);
    if (poll) {
        PlayerState state;
        PollHelper(gameMs, state);
    }
}

void GemManager::UpdateSlotPositions() {
    Transform tf48;
    for (int i = 0; i < GetMaxSlots(); i++) {
        RndDir *dir = mNowBar->FindSmasher(i)->Dir();
        RndTransformable *smashTrans =
            dir->Find<RndTransformable>("smasher.trans", false);
        if (smashTrans) {
            tf48 = smashTrans->WorldXfm();
        } else
            tf48 = dir->WorldXfm();
        mTrackDir->SetSlotXfm(i, tf48);
    }
    for (int i = mBegin; i < mEnd; i++) {
        mGems[i].UpdateTailPositions();
    }
}

Hmx::Object *GemManager::GetSmasherObj(int slot) {
    GemSmasher *smasher = mNowBar->FindSmasher(slot);
    if (!smasher) return 0;
    return smasher->Dir();
}

int GemManager::GetNumGems() const { return mGems.size(); }
const Gem &GemManager::GetGem(int idx) const { return mGems[idx]; }

void GemManager::PollVisibleGems(float f1, float f2) {
    float div = f1 / 1000.0f;
    float top = mTrackDir->TopSeconds() + div;
    float bot = mTrackDir->BottomSeconds() + div;
    for (int i = mBegin; i < mEnd; i++) {
        mGems[i].Poll(f1, f2, unkc4, top, bot);
    }
}

Symbol GemManager::GetTypeForGem(int gemId) {
    FillLogic fillLogic = TheGame->GetFillLogic();
    const GameGem &gem = TheSongDB->GetGems(mTrackConfig.TrackNum())[gemId];
    int gemTick = gem.GetTick();
    GemPlayer *player = (GemPlayer *)mTrackConfig.GetBandUser()->GetPlayer();
    if (player) {
        GemStatus *gemStatus = player->mGemStatus;
        if (gemStatus->GetSize() > gemId) {
            if (gemStatus->GetIgnored(gemId) || gemStatus->Get0x40(gemId)) {
                return invisible;
            }
        }
    }
    if (unkb8 && IsInFill(gemTick)) {
        return invisible;
    }
    if ((unsigned int)(fillLogic - 1) <= 1U && IsEndOfFill(gemTick)) {
        switch (fillLogic) {
        case kFillsDeployGemAndDim:
            return dim;
        case kFillsDeployGemAndInvisible:
            return invisible;
        }
    } else {
        if (mGemsEnabledStart < 0.0f || mGemsEnabledStart > gem.GetMs()) {
            return invisible;
        }
        bool isUnison;
        if (!InMissedPhrase(gemId) && IsSpotlightGem(gemId, isUnison)) {
            return isUnison ? unison : star;
        }
        if (gem.IsRealGuitar()) {
            if (gem.IsRealGuitarChord()) {
                if (mGems[gemId].mIsRepeatChord) {
                    return repeat;
                }
            } else if (mGems[gemId].mInArpeggio) {
                return section;
            }
        }
    }
    return normal;
}

void GemManager::AdvanceBegin() {
    mGems[mBegin].RemoveRep();
    mBegin++;
}

void GemManager::AdvanceEnd() {
    Gem &lastGem = mGems[mEnd];
    Symbol gemType = GetTypeForGem(mEnd);
    if (!mTailsGrp) {
        if (mTrackConfig.IsKeyboardTrack()) {
            mTailsGrp = mTrackDir->Find<RndGroup>("key_shift_tails.grp", true);
        } else
            mTailsGrp = mTrackDir->Find<RndGroup>("tails.grp", true);
    }
    unsigned int slots = lastGem.Slots();
    lastGem.AddRep(mTemplate, mTailsGrp, gemType, mTrackConfig, true);
    mEnd++;
    if (mTrackConfig.IsKeyboardTrack()) {
        int tick = lastGem.GetGameGem().GetTick();
        for (; mEnd < mGems.size() && mGems[mEnd].GetGameGem().GetTick() == tick;
             mEnd++) {
            Symbol curGemType = GetTypeForGem(mEnd);
            Gem &curGem = mGems[mEnd];
            slots |= curGem.Slots();
            curGem.AddRep(mTemplate, mTailsGrp, curGemType, mTrackConfig, true);
        }
        AddChordBracket(gemType, slots, lastGem.GetGameGem().GetMs());
    }
}

void GemManager::AddChordBracket(Symbol gemType, unsigned int slots, float ms) {
    TrackDir * &_ref0 = mTrackDir;
    if (!_ref0)
        return;
    if (mTrackConfig.IsKeyboardTrack()) {
        static Symbol invisible("invisible");
        if (gemType != invisible) {
            if (slots != 0) {
                bool hasTime = TheGame->unkdc != -1.0f;
                if (hasTime) {
                    if (TheGame->unkdc > ms) return;
                }
                int lowest = mTrackConfig.GetMaxSlots();
                int highest = -1;
                int numSlots = mTrackConfig.GetMaxSlots();
                for (int s = 0; s < numSlots; s++) {
                    if (slots & (1 << s)) {
                        if (s < lowest)
                            lowest = s;
                        if (highest < s)
                            highest = s;
                    }
                }
                unsigned char isMiss = (unsigned char)(gemType == "miss");
                if (lowest < highest) {
                    Symbol name;
                    unsigned char leftBlack = _ref0->IsBlackKey(lowest);
                    GetWidgetName(
                        name, leftBlack != 0, Symbol(isMiss ? "bracket_left_miss" : "bracket_left")
                    );
                    if (leftBlack)
                        lowest--;
                    TrackWidget *wLeft = GetWidgetByName(name);
                    RememberChordWidget(wLeft);
                    GetWidgetName(
                        name, 0, Symbol(isMiss ? "bracket_span_miss" : "bracket_span")
                    );
                    TrackWidget *wSpan = GetWidgetByName(name);
                    RememberChordWidget(wSpan);
                    unsigned char rightBlack = _ref0->IsBlackKey(highest);
                    GetWidgetName(
                        name, rightBlack != 0,
                        Symbol(isMiss ? "bracket_right_miss" : "bracket_right")
                    );
                    if (rightBlack)
                        highest++;
                    TrackWidget *wRight = GetWidgetByName(name);
                    RememberChordWidget(wRight);
                    AddWidgetInstanceImpl(wLeft, lowest, ms);
                    for (int s = lowest + 1; s < highest; s++) {
                        if (!_ref0->IsBlackKey(s)) {
                            AddWidgetInstanceImpl(wSpan, s, ms);
                        }
                    }
                    AddWidgetInstanceImpl(wRight, highest, ms);
                }
            }
        }
    }
}

void GemManager::RememberChordWidget(TrackWidget *w) {
    for (int i = 0; i < unkd8.size(); i++) {
        if (unkd8[i] == w)
            return;
    }
    unkd8.push_back(w);
}

void GemManager::AddWidgetInstanceImpl(TrackWidget *w, int ui, float f) {
    Transform tf58;
    mTrackDir->MakeWidgetXfm(ui, f / 1000.0f, tf58);
    w->AddInstance(tf58, 0);
}

void GemManager::ReleaseSlot(int gem_id, int slot) {
    MILO_ASSERT(gem_id < mGems.size(), 0x7C4);
    mGems[gem_id].ReleaseSlot(slot);
    mNowBar->StopBurning(1 << slot);
}

void GemManager::ReleaseHitGems() {
    FOREACH (it, mHitGems) {
        Gem &gem = mGems[it->mGemId];
        if (gem.CompareBounds() && !gem.Released()) {
            gem.KillDuration();
            gem.Release();
        }
        mNowBar->StopBurning(gem.Slots());
    }
}

void GemManager::PruneHitGems(float f1) {
    while (!mHitGems.empty()) {
        if (mGems[mHitGems.front().mGemId].OnScreen(f1))
            break;
        else
            mHitGems.pop_front();
    }
}

void GemManager::Hit(float f1, int i2, int i3) {
    if (!mTrackConfig.AllowsOverlappingGems()) {
        ReleaseHitGems();
    }
    mGems[i2].Hit();
    unsigned int slots = mGems[i2].Slots();
    mHitGems.push_back(HitGem(f1, i2, slots));
    if (mTrackConfig.IsKeyboardTrack()) {
        CheckRemoveChordBracket(i2);
    }
    bool b28 = false;
    if (IsSpotlightGem(i2, b28)) {
        i3 |= 2;
        if (!IsSpotlightGem(i2 + 1, b28)) {
            i3 |= 4;
        }
    }
    mNowBar->Hit(f1, i2, mInCoda, i3, mGems[i2].UseRGChordStyle());
}

void GemManager::Miss(float f1, int, int slot) {
    if (slot != -1) {
        MILO_ASSERT(slot >= 0 && slot < GetMaxSlots(), 0x81A);
        if (!mTrackConfig.AllowsOverlappingGems() && mNowBar->mCurrentGem != -1) {
            Released(f1, mNowBar->mCurrentGem);
        }
        mNowBar->Miss(f1, slot);
    }
}

void GemManager::Pass(int i1) { mGems[i1].Miss(); }
void GemManager::Ignore(int) {}

void GemManager::PartialHit(float f1, int i2, unsigned int ui, int i4) {
    mGems[i2].PartialHit(ui);
    bool b28 = false;
    if (IsSpotlightGem(i2, b28)) {
        i4 |= 2;
        if (!IsSpotlightGem(i2 + 1, b28)) {
            i4 |= 4;
        }
    }
    mNowBar->PartialHit(i2, ui, mInCoda, i4);
    mHitGems.push_back(HitGem(f1, i2, ui));
}

void GemManager::FillHit(int i1, int i2) { mNowBar->FillHit(i1, i2); }

void GemManager::Released(float f1, int i2) {
    Gem &gem = mGems[i2];
    if (gem.CompareBounds()) {
        if (!gem.GetGameGem().LeftHandSlide() && !gem.Released()) {
            gem.Release();

            float unk = f1 / 1000.0f;
            if (gem.mEnd > unk)
                gem.mTailStart = unk - gem.GetStart();
            else
                gem.KillDuration();

            mNowBar->StopBurning(gem.Slots());
        }
    }
}

void GemManager::SetSmasherGlowing(int i1, bool b2) {
    mNowBar->SetSmasherGlowing(i1, b2);
}

void GemManager::PopSmasher(int i1) { mNowBar->PopSmasher(i1); }

void GemManager::ResetSmashers(bool b1) { mNowBar->Reset(b1); }

void GemManager::Jump(float f1) {
    while (mBegin < mEnd)
        AdvanceBegin();
    mHitGems.clear();
    mMissedPhrases.clear();
    ClearTrackMasks();
    DrawTrackMasks(MsToTickInt(mTrackDir->TopSeconds() * 1000.0f + f1), MsToTickInt(f1));

    float f5 = mTrackDir->BottomSeconds();
    float threshold = f1 / 1000.0f + f5;
    int i1 = -1;
    for (int i = 0; i < mEnd; i++) {
        if (mGems[i].GetStart() < threshold)
            continue;
        if (i1 < 0)
            i1 = i;
        mGems[i].Reset();
    }
    mBegin = i1;
    mEnd = i1;
    mBegin = Clamp(0, i1, i1);
    mEnd = Clamp(0, mEnd, mEnd);
    ResetArpeggios(f1);
    PlayerState state;
    PollHelper(f1, state);
}

void GemManager::SetBonusGems(bool gems, const PlayerState &state) {
    mBonusGems = gems;
    UpdateGemStates();
}

void GemManager::SetInCoda(bool coda) { mInCoda = coda; }

bool GemManager::OnMissPhrase(int i1) {
    bool ret = true;
    int tracknum = mTrackConfig.TrackNum();
    Extent ext18(0, 0);
    if (TheSongDB->GetCommonPhraseExtent(tracknum, i1, ext18)) {
        int i2 = MsToTickInt(TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f);
        if (!mMissedPhrases.empty()) {
            Extent &back = mMissedPhrases.back();
            ret = ext18.unk4 != back.unk4;
            if (ret) {
                mMissedPhrases.push_back(Extent(i2, ext18.unk4));
            }
            if (ret || i2 == back.unk0) {
                UpdateGemStates();
            }
        } else {
            mMissedPhrases.push_back(Extent(i2, ext18.unk4));
            UpdateGemStates();
        }
    }
    return ret;
}

void GemManager::CheckRemoveChordBracket(int gemId) {
    const GameGem &gem = mGems[gemId].GetGameGem();
    int chordTick = gem.GetTick();
    int start = gemId;
    while (start >= 0 && chordTick == mGems[start].GetGameGem().GetTick()) {
        start--;
    }
    int end = gemId;
    while (end < mGems.size() && chordTick == mGems[end].GetGameGem().GetTick()) {
        end++;
    }
    bool allHit = true;
    GemStatus *gemStatus =
        ((GemPlayer *)mTrackConfig.GetBandUser()->GetPlayer())->mGemStatus;
    for (int i = start + 1; i < end; i++) {
        if (i != gemId) {
            allHit = allHit & (bool)gemStatus->GetHit(i);
        }
    }
    if (allHit) {
        float ms = gem.GetMs() / 1000.0f;
        for (int i = 0; i < unkd8.size(); i++) {
            unkd8[i]->RemoveAt(ms);
        }
    }
}

bool GemManager::IsSpotlightGem(int gemId, bool &outUnison) {
    if (!TheGame->AllowOverdrivePhrases()) {
        return false;
    }
    auto _tmp0 = mTrackConfig.TrackNum();
    int phrase_id = TheSongDB->GetPhraseID(_tmp0, gemId);
    MILO_ASSERT(phrase_id >= -1, 0xA04);
    if (phrase_id != -1) {
        Band *band = mTrackConfig.GetBandUser()->GetPlayer()->mBand;
        bool inFutureLoop = false;
        if (TheGame->InTrainer() && TheGemTrainerPanel->IsGemInFutureLoop(gemId)) {
            inFutureLoop = true;
        }
        if (!inFutureLoop) {
            CommonPhraseCapturer *capturer = band->mCommonPhraseCapturer;
            if (capturer->DidTrackFail(phrase_id, mTrackConfig.TrackNum()))
                return false;
        }
        outUnison = TheSongDB->IsUnisonPhrase(phrase_id);
        return true;
    }
    return false;
}

void GemManager::UpdateGemStates() {
    GemPlayer *player = (GemPlayer *)mTrackConfig.GetBandUser()->GetPlayer();
    if (player) {
        GemStatus *gemStatus = player->GetGemStatus();
        MILO_ASSERT(gemStatus, 0x9B1);
        if (gemStatus->GetSize() > 0) {
            MILO_ASSERT(gemStatus->GetSize() == mGems.size(), 0x9BA);
            for (int i = mBegin; i < mEnd; i++) {
                if (!gemStatus->GetHit(i) && !gemStatus->Get0x2(i)
                    && !gemStatus->Get0x4(i)) {
                    Symbol type = GetTypeForGem(i);
                    mGems[i].SetType(type);
                }
            }
        }
    }
}

bool GemManager::InMissedPhrase(int gemId) {
    if (TheGame->InTrainer() && TheGemTrainerPanel->IsGemInFutureLoop(gemId)) {
        return false;
    }
    int rawTick = mGems[gemId].GetGameGem().GetTick();
    for (int i = 0; i < mMissedPhrases.size(); i++) {
        int tick = GetLoopTick(rawTick);
        bool inPhrase = tick >= mMissedPhrases[i].unk0 && tick <= mMissedPhrases[i].unk4;
        if (inPhrase) {
            return true;
        }
    }
    return false;
}

void GemManager::PollHitGems(float ms) {
    PruneHitGems(ms);
    float tailClipY = mTemplate.mTailClipY;
    float bottom = ms / 1000.0f + mTrackDir->YToSeconds(tailClipY);
    FOREACH (it, mHitGems) {
        Gem &gem = mGems[it->mGemId];
        if (gem.OnScreen(ms)) {
            if (gem.CompareBounds() && !gem.Released()) {
                if (gem.mEnd > bottom) {
                    gem.mTailStart = bottom - gem.GetStart();
                } else {
                    gem.KillDuration();
                    mNowBar->StopBurning(gem.Slots());
                    gem.Release();
                }
            }
        }
    }
}

void GemManager::Poll(float ms, const PlayerState &state) {
    // Retail materializes the bool (li 1 / li 0 / clrlwi.) rather than
    // branching on the fcmpu directly -- the inlined InRollback() accessor,
    // not a raw `unkdc != -1.0f` compare (same pattern as Player.cpp:226).
    if (TheGame->IsWaiting() || TheGame->InRollback() ||
        (TheRGTrainerPanel && TheRGTrainerPanel->GetLegendMode())) {
        UpdateArpeggios(ms, true);
    } else {
        // RB3-360: `|| ThePracticePanel->unk5c <= 0` removed from this
        // condition — PracticePanel::unk5c is absent in retail (Wii-only
        // track-in delay state). GemManager::Poll is unpinned; the retail
        // form of this practice-mode condition is UNVERIFIED (recon open
        // question — revisit when GemManager is pinned).
        PollHelper(ms, state);
    }
}

void GemManager::PollHelper(float ms, const PlayerState &state) {
    int begin;
    for (begin = mBegin; begin < mGems.size() && !mGems[begin].OnScreen(ms);
         begin++) {
    }
    mEnd = Max(begin, mEnd);
    float top = ms / 1000.0f + mTrackDir->TopSeconds();
    while (mEnd < mGems.size() && mGems[mEnd].GetStart() < top) {
        AdvanceEnd();
    }
    while (mBegin < begin) {
        AdvanceBegin();
    }
    if (state.whammyActive) {
        float up = unkc4 + 0.1f;
        unkc4 = std::min(1.0f, up);
    } else {
        float down = unkc4 - 0.1f;
        unkc4 = std::max(0.0f, down);
    }
    DrawTrackMasks(MsToTickInt(top * 1000.0f), -1);
    UpdateArpeggios(ms, false);
    PollHitGems(ms);
    PollVisibleGems(ms, state.whammy);
    mNowBar->Poll(ms, state.whammyActive);
}

bool GemManager::GetWidgetName(Symbol &sref, int i2, Symbol s3) {
    DataArray *arr = mGemData->FindArray(i2, false);
    if (!arr)
        return false;
    else {
        DataArray *symArr = arr->FindArray(s3, false);
        if (!symArr)
            return false;
        else {
            sref = symArr->Sym(1);
            return true;
        }
    }
}

bool GemManager::GetChordWidgetName(Symbol s1, Symbol s2, Symbol &sref) {
    DataArray *arr = mGemData->FindArray(s2, false);
    if (!arr)
        return false;
    else {
        DataArray *symArr = arr->FindArray(s1, false);
        if (!symArr)
            return false;
        else {
            sref = symArr->Sym(1);
            return true;
        }
    }
}

int GemManager::GetSlotIntData(int i1, Symbol s2) {
    DataArray *arr = mGemData->FindArray(i1);
    return arr->FindInt(s2);
}

int GemManager::GetSlotsForGem(int gem) {
    if (gem < 0 || gem >= mGems.size())
        return 0;
    else
        return mGems[gem].Slots();
}

void GemManager::EnableSlot(int slot) {
    if (!SlotEnabled(slot)) {
        mDisabledSlotsList.remove(slot);
    }
}

void GemManager::DisableSlot(int slot) {
    if (SlotEnabled(slot)) {
        mDisabledSlotsList.push_back(slot);
    }
}

bool GemManager::SlotEnabled(int slot) const {
    return std::find(mDisabledSlotsList.begin(), mDisabledSlotsList.end(), slot)
        == mDisabledSlotsList.end();
}

void GemManager::ClearGems(bool b) {
    for (int i = mBegin; i < mEnd; i++) {
        Gem &curGem = mGems[i];
        if (b || (!curGem.GetHit() || curGem.CompareBounds()) && !curGem.Released()) {
            curGem.RemoveAllInstances();
            curGem.RemoveRep();
        }
    }
}

void GemManager::ClearAllGems() {
    for (int i = 0; i < mGems.size(); i++) {
        Gem &curGem = mGems[i];
        curGem.Release();
        curGem.RemoveAllInstances();
        curGem.RemoveRep();
    }
}

void GemManager::HideGems() {
    static Symbol invisible("invisible");
    for (int i = 0; i < mGems.size(); i++) {
        mGems[i].SetType(invisible);
    }
}

void GemManager::ClearGem(int idx) {
    Gem &curGem = mGems[idx];
    curGem.Release();
    curGem.RemoveAllInstances();
    curGem.RemoveRep();
}

bool GemManager::GetFill(int i1, FillExtent &ext) {
    Player *player = mTrackConfig.GetBandUser()->GetPlayer();
    if (!player || !player->FillsEnabled(i1))
        return false;
    else {
        FillInfo *info = TheSongDB->GetData()->GetFillInfo(mTrackConfig.TrackNum());
        return !info ? false : info->FillAt(GetLoopTick(i1), ext, true);
    }
}

bool GemManager::IsInFill(int idx) {
    Player *player = mTrackConfig.GetBandUser()->GetPlayer();
    if (player->AreFillsForced())
        return true;
    else {
        FillExtent ext(0, 0, 0);
        return GetFill(idx, ext);
    }
}

bool GemManager::IsEndOfFill(int idx) {
    bool ret = false;
    FillExtent ext(0, 0, 0);
    if (GetFill(idx, ext) && ext.end == idx)
        ret = true;
    return ret;
}

void GemManager::ClearMissedPhrases() {
    mTrackConfig.GetBandUser()->GetPlayer()->mBand->mCommonPhraseCapturer->Reset();
    mMissedPhrases.clear();
}

TrackWidget *GemManager::GetWidgetByName(Symbol name) {
    if (mWidgets.find(name) == mWidgets.end()) {
        mWidgets[name] = mTrackDir->Find<TrackWidget>(name.Str(), true);
    }
    return mWidgets[name];
}

void GemManager::UpdateEnabledSlots() {
    mEnabledSlots = 0;
    for (int i = 0; i < mGems.size(); i++) {
        mEnabledSlots |= mGems[i].Slots();
    }
}

#pragma push
#pragma force_active on
inline int GemManager::GetMaxSlots() const { return mTrackConfig.GetMaxSlots(); }
#pragma pop

#line 87 "TrackDir.h"
RndDir *TrackDir::SmasherPlate() {
    MILO_ASSERT(0, 0x57);
    return nullptr;
}
#line 1131 "GemManager.cpp"

bool TrackDir::IsBlackKey(int) const { return false; }

// sw2 scatter-include (default/band3/bandtrack/GemManager <- flow/FlowManager.cpp)
#define gRev gRev_FlowManager
#define gAltRev gAltRev_FlowManager
#include "flow/FlowManager.cpp"
#undef gRev
#undef gAltRev

// ---------------------------------------------------------------------------
// lane-AE batch-3 (sw3) force-emit: retail scattered these template/implicit
// COMDATs into the .text span pinned to default/band3/bandtrack/GemManager.
// They are emitted by other TUs but objdiff pairs target<->base WITHIN a unit,
// so this TU has to define them too.
//
// ⚠ MATCH BUILD ONLY. `stlpmtx_std::vector<...>::erase` and
// `stlpmtx_std::_Param_Construct` are STLport namespace-level names; libstdc++
// and libc++ expose neither, so these explicit instantiations cannot compile
// natively at all. Their whole purpose is objdiff's within-unit pairing, which
// only exists in the match build -- there is nothing for them to do natively.
// Guarding them is what lets char/CharBonesMeshes.cpp (which reaches this TU
// through a scatter-include chain) join the native fork surface; without it,
// CharClip/CharClipSet/Char lose CharBonesMeshes on the LOAD path.
#ifndef HX_NATIVE
#include "rndobj/Text.h"
#include "world/Crowd.h"
#include "ui/LocalePanel.h"

// ?erase@?$vector@VLine@RndText@@V?$StlNodeAlloc@VLine@RndText@@@stlpmtx_std@@@stlpmtx_std@@QAAPAVLine@RndText@@PAV34@0@Z
template RndText::Line *
stlpmtx_std::vector<RndText::Line, stlpmtx_std::StlNodeAlloc<RndText::Line> >::
    erase(RndText::Line *, RndText::Line *);

// ??$_Param_Construct@UChar3D@CharData@WorldCrowd@@U123@@stlpmtx_std@@YAXPAUChar3D@CharData@WorldCrowd@@ABU123@@Z
template void stlpmtx_std::_Param_Construct<
    WorldCrowd::CharData::Char3D,
    WorldCrowd::CharData::Char3D>(
    WorldCrowd::CharData::Char3D *, const WorldCrowd::CharData::Char3D &);

// ??0Entry@LocalePanel@@QAA@ABU01@@Z -- implicit copy ctor; a by-value pass
// cannot be elided from an lvalue reference, so this odr-uses it.
static void sw3_Sink3_LocalePanelEntry(LocalePanel::Entry) {}
void sw3_ForceEmit3_LocalePanelEntry(const LocalePanel::Entry &e) {
    sw3_Sink3_LocalePanelEntry(e);
}
#endif // !HX_NATIVE
