#include "macros.h"
#undef MILO_DEBUG
#include "bandtrack/VocalTrack.h"
#include "GraphicsUtl.h"
#include "VocalStyle.h"
#include "bandobj/NoteTube.h"
#include "bandobj/PitchArrow.h"
#include "bandobj/StreakMeter.h"
#include "bandobj/VocalTrackDir.h"
#include "bandtrack/Lyric.h"
#include "bandtrack/TrackPanel.h"
#include "bandtrack/VocalStyle.h"
#include "beatmatch/VocalNote.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/Player.h"
#include "game/SongDB.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/GameplayOptions.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ProfileMgr.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataFunc.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/PropAnim.h"
#include "synth/MicManagerInterface.h"
#include "utl/BeatMap.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"
#include <utility>

int maxPlatesQueued;
int maxVertsInPlate;
int maxFacesInPlate;
int maxNumLyricPlates;
bool dumpLyricShifts;
bool sDumpLyricPlates;
bool sDumpPlateStates;
bool gDebugSpew;

MicClientID sNullMicClientID(-1, -1);

inline TambourineGemPool::TambourineGemPool() {
    for (int i = 0; i < 25; i++) {
        mFreeGems.push_back(new TambourineGem());
    }
    mTambourineManager = 0;
}

inline TambourineGemPool::~TambourineGemPool() {
    FreeUsedGems();
    MILO_ASSERT(mUsedGems.empty(), 0x1B6);
    for (int i = 0; i < mFreeGems.size(); i++) {
        RELEASE(mFreeGems[i]);
    }
}

void VocalTrack::UpdateMarkerVisibility(float f1, float f2) {
    for (int i = 0; i < unk1a0.size(); i++) {
        std::pair<RndMesh *, float> &curMarker = unk1a0[i];
        bool show = curMarker.second >= f1 && curMarker.second <= f2;
        curMarker.first->SetShowing(show);
    }
}

void VocalTrack::InvalidateMarkers(float f1) {
    while (!unk1a0.empty()) {
        if (f1 < unk1a0.front().second)
            break;
        ReturnFirstMarker();
    }
}

void VocalTrack::ClearMarkers() {
    while (!unk1a0.empty()) {
        ReturnFirstMarker();
    }
}

inline void TambourineGemPool::NewGem(float time, int gemIdx) {
    MILO_ASSERT(mUsedGems.empty() || time >= mUsedGems.back()->Time(), 0x1EB);
    if (mFreeGems.empty()) {
        for (int k = 0; k < 5; k++) {
            mFreeGems.push_back(new TambourineGem());
        }
    }
    TambourineGem *g = mFreeGems.front();
    mFreeGems.pop_front();
    mUsedGems.push_back(g);
    g->unk4 = gemIdx;
    g->unk0 = time;
    g->unk8 = 0;
    MILO_ASSERT(mTambourineManager, 0x1FD);
    if (mTambourineManager->GemHit(gemIdx) || mTambourineManager->GemProcessed(gemIdx)) {
        g->unk8 = 1;
    }
}

void VocalTrack::UpdateTubePlates(
    std::deque<TubePlate *> &deque, float f2, float f3, bool b4
) {
    if (mIntroPlaying || deque.empty())
        return;
    while (!deque.empty() && !deque.front()->NoVerts()
           && (deque.front()->CurrentEndX(f3) < mDir->mTrackLeftX - unk78
               || deque.front()->InvalidateMs() < f2)) {
        if (!deque.front()->Baked()) {
            MILO_WARN("popping unbaked plate");
        }
        TubePlate *cur = deque.front();
        if (sDumpPlateStates) {
            MILO_LOG(
                "%s recycling plate at %.2f sec\n",
                cur->GetMatName().c_str(),
                f2 / 1000.0f
            );
            DumpPlates(deque, cur->GetMatName().c_str());
        }
        deque.pop_front();
        cur->Reset();
        deque.push_back(cur);
    }
    float fvar1 = TheGame->InRollback() ? unk2a4 : f2;
    FOREACH (it, deque) {
        TubePlate *cur = *it;
        if (cur->CurrentEndX(f3) < mDir->mTrackLeftX) {
            cur->SetShowing(false);
        } else {
            if (cur->CurrentStartX(f3) >= mDir->mTrackRightX) {
                cur->SetShowing(false);
                break;
            } else
                cur->SetShowing(true);
        }

        if (sDumpPlateStates && !cur->Baked()) {
            MILO_LOG(
                "%s baking plate at %.2f sec\n", cur->GetMatName().c_str(), f2 / 1000.0f
            );
            DumpPlates(deque, cur->GetMatName().c_str());
        }
        cur->Bake();
        if (mVocalStyleOverride == kVocalStyleScrolling && cur->Deploy()) {
            cur->PollDeploy(fvar1);
        }
    }
#ifdef MILO_DEBUG
    if (deque.size() != 0) {
        if (deque.size() > maxPlatesQueued) {
            maxPlatesQueued = std::max<int>(maxPlatesQueued, deque.size());
            if (maxPlatesQueued >= 24) {
                MILO_WARN(
                    "Too many tube plates - please file a bug to Josh Stoddard and include the Watson output."
                );
                DumpPlates(deque, deque.front()->GetMatName().c_str());
            }
            if (sDumpPlateStates) {
                MILO_LOG("max plates queued -> %d\n", maxPlatesQueued);
            }
        }
        int numverts = deque.front()->mMesh->Verts().size();
        if (numverts > maxVertsInPlate) {
            maxVertsInPlate = std::max<int>(maxVertsInPlate, numverts);
            if (sDumpPlateStates) {
                MILO_LOG("max verts in a plate -> %d\n", maxVertsInPlate);
            }
        }
        int numfaces = deque.front()->mMesh->Faces().size();
        if (numfaces > maxFacesInPlate) {
            maxFacesInPlate = std::max<int>(maxFacesInPlate, numfaces);
            if (sDumpPlateStates) {
                MILO_LOG("max faces in a plate -> %d\n", maxFacesInPlate);
            }
        }
    }
#endif
}

void VocalTrack::UpdateAllTubePlates(float f1) {
    if (!mPlayer->IsNet()) {
        for (int i = 0; i < 3; i++) {
            UpdateTubePlates(mFrontTubePlates[i], f1, unk2a8, false);
            UpdateTubePlates(mBackTubePlates[i], f1, unk2a8, false);
            UpdateTubePlates(mPhonemeTubePlates[i], f1, unk2a8, false);
        }
    }
    bool staticVox = !IsScrolling();
    UpdateTubePlates(mLeadDeployPlates, f1, staticVox ? unk2ac : unk2a8, staticVox);
    UpdateTubePlates(mHarmonyDeployPlates, f1, staticVox ? unk2b0 : unk2a8, staticVox);
}

void VocalTrack::ClearTubePlates(std::deque<TubePlate *> &plates) {
    while (!plates.empty()) {
        delete plates.front();
        plates.pop_front();
    }
}

void VocalTrack::ClearAllTubePlates() {
    for (int i = 0; i < 3; i++) {
        ClearTubePlates(mFrontTubePlates[i]);
        ClearTubePlates(mBackTubePlates[i]);
        ClearTubePlates(mPhonemeTubePlates[i]);
    }
    ClearTubePlates(mLeadDeployPlates);
    ClearTubePlates(mHarmonyDeployPlates);
}

void VocalTrack::ResetTubePlates(std::deque<TubePlate *> &plates) {
    std::deque<TubePlate *>::iterator it = plates.begin();
    std::deque<TubePlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        (*it)->Reset();
    }
}

void VocalTrack::ResetAllTubePlates() {
    for (int i = 0; i < 3; i++) {
        ResetTubePlates(mFrontTubePlates[i]);
        ResetTubePlates(mBackTubePlates[i]);
        ResetTubePlates(mPhonemeTubePlates[i]);
    }
    ResetTubePlates(mLeadDeployPlates);
    ResetTubePlates(mHarmonyDeployPlates);
    if (sDumpPlateStates) {
        MILO_LOG("resetting all plates\n");
    }
}

void VocalTrack::DumpPlates(std::deque<TubePlate *> &plates, const char *str) {
    MILO_LOG("dumping plates in %s\n", str);
    int idx = 0;
    std::deque<TubePlate *>::iterator it = plates.begin();
    std::deque<TubePlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        TubePlate *cur = *it;
        if (!cur->NoVerts()) {
            const Transform &xfm = cur->mMesh->TransParent()->WorldXfm();
            MILO_LOG(
                "\t[%d] @ %x, xPos: %.2f, xStart: %.2f, XEnd: %.2f, verts: %d, faces: %d, baked: %d\n",
                idx++,
                cur,
                -xfm.v.x,
                cur->GetBeginX(),
                cur->GetBeginX() + cur->GetWidthX(),
                cur->mMesh->Verts().size(),
                cur->mMesh->Faces().size(),
                cur->Baked()
            );
        } else {
            MILO_LOG(
                "\t[%d] @ %x, <empty>, verts: %d, faces: %d, baked: %d\n",
                idx++,
                cur,
                cur->mMesh->Verts().size(),
                cur->mMesh->Faces().size(),
                cur->Baked()
            );
        }
    }
}

void VocalTrack::DumpAllPlates() {
    for (int i = 0; i < 3; i++) {
        DumpPlates(mFrontTubePlates[i], MakeString("part %d front", i));
        DumpPlates(mBackTubePlates[i], MakeString("part %d back", i));
        DumpPlates(mPhonemeTubePlates[i], MakeString("part %d phoneme", i));
    }
    DumpPlates(mLeadDeployPlates, "lead deploy");
    DumpPlates(mHarmonyDeployPlates, "harmony deploy");
}

TubePlate *VocalTrack::GetCurrentPlate(std::deque<TubePlate *> &plates, int i2) {
    std::deque<TubePlate *>::iterator it = plates.begin();
    std::deque<TubePlate *>::iterator itEnd = plates.end();
    for (; it != itEnd; ++it) {
        if (!(*it)->Baked())
            return *it;
    }
    plates.push_back(new TubePlate(i2));
#ifdef MILO_DEBUG
    static Symbol leadDeployMat = "deploy_mask_lead.mat";
    static Symbol harmDeployMat = "deploy_mask_harmony.mat";

    String matName = plates.front()->GetMatName();
    if (!mIntroPlaying && matName != leadDeployMat && matName != harmDeployMat) {
        MILO_WARN(
            "%s new plate added.  Please alert HUD/Track owner and include the Watson output.",
            matName.c_str()
        );
        DumpPlates(plates, plates.front()->GetMatName().c_str());
    }
#endif
    return plates.back();
}

void VocalTrack::HookupTubePlates(NoteTube *tube) {
    if (tube->Pitched()) {
        tube->SetFrontPlate(GetCurrentPlate(mFrontTubePlates[tube->Part()], 0x80));
        tube->SetBackPlate(GetCurrentPlate(mBackTubePlates[tube->Part()], 0x80));
    } else if (tube->unk_0x24) {
        bool islead = tube->Part() == 0;
        tube->SetFrontPlate(nullptr);
        tube->SetBackPlate(
            GetCurrentPlate(islead ? mLeadDeployPlates : mHarmonyDeployPlates, 0x20)
        );
    } else {
        tube->SetFrontPlate(nullptr);
        tube->SetBackPlate(GetCurrentPlate(mPhonemeTubePlates[tube->Part()], 0x40));
    }
}

DataNode ToggleDebugSpew(DataArray *) {
    gDebugSpew = !gDebugSpew;
    return gDebugSpew;
}

VocalTrack::VocalTrack(BandUser *u)
    : Track(u), unk68(0), mVocalStyleOverride(kVocalStyleScrolling), unk70(2),
      unk78(24.0f), unk7c(0), mDir(this), mPlayer(this), mPhraseStartMs(0),
      mPhraseEndMs(0), mNextPhraseEndMs(0), unkf4(0), unkf8(0), unkfc(0), unk100(0),
      unk104(1), unk108(0), unk128(0), unk19c(0), unk1c8(this), mTambourineGemPool(0),
      mCharOptMicID(-1), unk208(60), unk20c(0), unk210(0), unk23c(0.1f), unk240(0.1f),
      unk294(0), unk298(0), unk2a4(-1.0f), unk2a8(0), unk2ac(0), unk2b0(0),
      mStaticDeployZoneXSize(2.0f), mStaticDeployBufferX(0.5f),
      mStaticDeployMarginX(0.1f), mLyricShiftMs(100.0f), mLyricShiftQuickMs(20.0f),
      mLyricShiftAnticipationMs(250.0f), mMinLyricHighlightMs(100.0f),
      mMinPhraseHighlightMs(500.0f), mLyricOverlapWindowMs(100.0f), unk2e4(0),
      mNoteTube(new NoteTube()), unk2ec(1) {
    DataRegisterFunc("vocal_jitter_debug", ToggleDebugSpew);
    for (int i = 0; i < 3; i++) {
        mFrontTubePlates.push_back(std::deque<TubePlate *>());
        mBackTubePlates.push_back(std::deque<TubePlate *>());
        mPhonemeTubePlates.push_back(std::deque<TubePlate *>());
        mAlternateNoteList[i] = 0;
    }
    InitPlatePool();
}

VocalTrack::~VocalTrack() {
    RELEASE(mTambourineGemPool);
    ClearLyrics();
    ClearMarkers();
    ClearAllTubePlates();
    DeleteAll(mMeshPool);
    RELEASE(mNoteTube);
}

void VocalTrack::InitPlateList(std::deque<TubePlate *> &list, int i2, int i3) {
    MILO_ASSERT(list.empty(), 0x25C);
    for (int i = 0; i < i2; i++) {
        list.push_back(new TubePlate(i3));
    }
}

void VocalTrack::InitPlatePool() {
    for (int i = 0; i < 3; i++) {
        InitPlateList(mFrontTubePlates[i], 4, 0x80);
        InitPlateList(mBackTubePlates[i], 4, 0x80);
        InitPlateList(mPhonemeTubePlates[i], 4, 0x40);
    }
    InitPlateList(mLeadDeployPlates, 4, 0x20);
    InitPlateList(mHarmonyDeployPlates, 4, 0x20);
}

void VocalTrack::Init() {
    const BandUser *pUser = mTrackConfig.GetBandUser();
    MILO_ASSERT(pUser, 0x275);
    mTrackConfig.SetTrackNum(TheGameConfig->GetTrackNum(pUser->GetUserGuid()));
    unk74 = 3000.0f;
    RELEASE(mTambourineGemPool);
    mTambourineGemPool = new TambourineGemPool();
    if (mPlayer)
        mTambourineGemPool->SetTambourineManager(&mPlayer->mTambourineManager);
    BandUser *user = (BandUser *)mTrackConfig.GetBandUser();
    GameplayOptions *options = user->GetGameplayOptions();
    if (options) {
        DataArray *staticArr = SystemConfig()->FindArray("force_static_vocals", false);
        if (staticArr) {
            if (SystemConfig()->FindInt("force_static_vocals")) {
                SetVocalStyle((VocalStyle)0);
            }
            goto next;
        }
        SetVocalStyle(options->GetVocalStyle());
    }
next:
    ReadTimingData(SystemConfig()->FindArray("track_graphics"));
    unk1c8 = mDir->Find<RndGroup>("markers.grp", true);
    unk19c = 0;
    for (int i = 0; i < 0x20; i++) {
        CreateMarker("beat_marker.mesh", 0, false);
    }
    ClearMarkers();
}

void VocalTrack::ResetTimingData() {
    ReadTimingData(DataReadFile("config/track_graphics.dta", true));
    RebuildHUD();
}

void VocalTrack::ReadTimingData(const DataArray *a) {
    mLyricOverlapWindowMs = a->FindFloat("lyric_overlap_ms");
    DataArray *staticCfg = a->FindArray("static_vocal_parameters");
    mStaticDeployZoneXSize = staticCfg->FindFloat("static_deploy_x_size");
    mStaticDeployBufferX = staticCfg->FindFloat("static_deploy_buffer_x");
    mStaticDeployMarginX = staticCfg->FindFloat("static_phrase_margin_x");
    mLyricShiftMs = staticCfg->FindArray("lyric_shift_ms")->Float(1);
    mLyricShiftQuickMs = staticCfg->FindArray("lyric_shift_ms")->Float(2);
    mLyricShiftAnticipationMs = staticCfg->FindFloat("lyric_shift_anticipation_ms");
    mMinLyricHighlightMs = staticCfg->FindFloat("min_lyric_highlight_ms");
    mMinPhraseHighlightMs = staticCfg->FindFloat("phrase_highlight_ms");
    static bool sDump;
    if (sDump) {
        MILO_LOG("lyric timing data:\n");
        MILO_LOG("\t overlap window ms %.0f\n", mLyricOverlapWindowMs);
        MILO_LOG("\t static deploy size %.2f\n", mStaticDeployZoneXSize);
        MILO_LOG("\t static deploy gap size %.2f\n", mStaticDeployBufferX);
        MILO_LOG("\t now bar offset %.2f\n", mStaticDeployMarginX);
        MILO_LOG("\t standard lyric shift ms %.0f\n", mLyricShiftMs);
        MILO_LOG("\t fast lyric shift ms %.0f\n", mLyricShiftQuickMs);
        MILO_LOG("\t lyric shift anticipation ms %.0f\n", mLyricShiftAnticipationMs);
        MILO_LOG("\t min lyric highlight ms %.0f\n", mMinLyricHighlightMs);
        MILO_LOG("\t phrase highlight anticipation ms %.0f\n", mMinPhraseHighlightMs);
    }
}

bool VocalTrack::ShowPitchCorrectionNotice() const {
    if (mPlayer)
        return mPlayer->ShowPitchCorrectionNotice();
    else
        return false;
}

void VocalTrack::ConfigNoteTube(bool pitched, int pts, int part, bool b4, float alpha) {
    mNoteTube->SetPitched(pitched);
    mNoteTube->SetNumPoints(pts);
    mNoteTube->SetPart(part);
    mNoteTube->unk_0x24 = b4;
    mNoteTube->SetAlpha(alpha);
    if (pitched) {
        switch (part) {
        case 1:
            mNoteTube->SetBackMat(mDir->mHarm1BackMat);
            mNoteTube->SetBackParent(mDir->mTubeBack1Grp);
            mNoteTube->SetFrontMat(mDir->mHarm1FrontMat);
            mNoteTube->SetFrontParent(mDir->mTubeFront1Grp);
            break;
        case 2:
            mNoteTube->SetBackMat(mDir->mHarm2BackMat);
            mNoteTube->SetBackParent(mDir->mTubeBack2Grp);
            mNoteTube->SetFrontMat(mDir->mHarm2FrontMat);
            mNoteTube->SetFrontParent(mDir->mTubeFront2Grp);
            break;
        default:
            mNoteTube->SetBackMat(mDir->mLeadBackMat);
            mNoteTube->SetBackParent(mDir->mTubeBack0Grp);
            mNoteTube->SetFrontMat(mDir->mLeadFrontMat);
            mNoteTube->SetFrontParent(mDir->mTubeFront0Grp);
            break;
        }
    } else if (!b4) {
        mNoteTube->SetFrontMat(nullptr);
        mNoteTube->SetFrontParent(nullptr);
        switch (part) {
        case 1:
            mNoteTube->SetBackMat(mDir->mHarm1PhonemeMat);
            mNoteTube->SetBackParent(mDir->mTubePhoneme1Grp);
            break;
        case 2:
            mNoteTube->SetBackMat(mDir->mHarm2PhonemeMat);
            mNoteTube->SetBackParent(mDir->mTubePhoneme2Grp);
            break;
        default:
            mNoteTube->SetBackMat(mDir->mLeadPhonemeMat);
            mNoteTube->SetBackParent(mDir->mTubePhoneme0Grp);
            break;
        }
    } else {
        MILO_ASSERT(part < 3, 0x30D);
        mNoteTube->SetFrontMat(nullptr);
        mNoteTube->SetFrontParent(nullptr);
        mNoteTube->SetBackParent(nullptr);
        if (part != 0)
            mNoteTube->SetBackMat(mDir->mHarmDeployMat);
        else
            mNoteTube->SetBackMat(mDir->mLeadDeployMat);
    }
}

LyricPlate *VocalTrack::GetNextLyricPlate(std::deque<LyricPlate *> &plates, bool b2) {
    std::deque<LyricPlate *>::iterator it = plates.begin();
    std::deque<LyricPlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        if ((*it)->Empty())
            return *it;
    }
    RndText *text = b2 ? mDir->mLeadText : mDir->mHarmText;
    RndText *phonemeText = b2 ? mDir->mLeadPhonemeText : mDir->mHarmPhonemeText;
    RndText *newText = NewRndCopy(text);
    plates.push_back(new LyricPlate(newText, text, phonemeText));
    if (sDumpLyricPlates) {
        MILO_LOG("creating new %s lyric plate\n", b2 ? "lead" : "harmony");
        DumpLyricPlates(plates, b2);
    }
    int numplates = plates.size();
    bool grew;
    if (maxNumLyricPlates < numplates) {
        maxNumLyricPlates = numplates;
        grew = true;
    } else {
        grew = false;
    }
    bool doDump = grew && sDumpLyricPlates;
    if (doDump) {
        MILO_LOG("Max Lyric Plates: %d\n", maxNumLyricPlates);
    }
    return plates.back();
}

Lyric *VocalTrack::GetLastLyric(std::deque<LyricPlate *> &plates) {
    Lyric *last = nullptr;
    std::deque<LyricPlate *>::iterator it = plates.begin();
    std::deque<LyricPlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        if ((*it)->Empty())
            break;
        last = (*it)->LatestLyric();
    }
    return last;
}

Lyric *VocalTrack::GetLastBakedLyric(std::deque<LyricPlate *> &plates) {
    Lyric *last = nullptr;
    std::deque<LyricPlate *>::iterator it = plates.begin();
    std::deque<LyricPlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        if (!(*it)->Baked())
            break;
        last = (*it)->LatestLyric();
    }
    return last;
}

RndMesh *VocalTrack::CreateMarker(Symbol s1, float f2, bool warn) {
    RndMesh *mesh = nullptr;
    if (mMeshPool.empty()) {
        mesh = Hmx::Object::New<RndMesh>();
        unk19c++;
        if (warn) {
            MILO_WARN(
                "VocalTrack::CreateMarker() added new %s mesh at run-time (total %d); please alert HUD/Track owner",
                s1.Str(),
                unk19c
            );
        }
    } else {
        mesh = mMeshPool.back();
        mMeshPool.pop_back();
    }
    RndMesh *found = mDir->Find<RndMesh>(s1.Str(), true);
    mesh->SetGeomOwner(found->GetGeomOwner());
    mesh->SetMat(found->Mat());
    mesh->SetShowing(true);
    mesh->SetTransParent(found->TransParent(), false);
    const Transform &foundXfm = found->LocalXfm();
    mesh->SetLocalXfm(foundXfm);
    mesh->SetTransParent(mDir->mScroller, true);
    Vector3 markerPos(mesh->LocalXfm().v);
    markerPos.x = unk78 * (f2 / unk74);
    mesh->SetLocalPos(markerPos);
    unk1c8->AddObject(mesh);
    unk1a0.push_back(std::make_pair(mesh, f2));
    return mesh;
}

void VocalTrack::ReturnFirstMarker() {
    RndMesh *mesh = unk1a0.front().first;
    MILO_ASSERT(mesh, 0x393);
    MILO_ASSERT(mesh->GetGeomOwner() != mesh, 0x394);
    mMeshPool.push_back(mesh);
    unk1c8->RemoveObject(mesh);
    unk1a0.pop_front();
}

void VocalTrack::SetDir(RndDir *dir) {
    mDir = dynamic_cast<VocalTrackDir *>(dir);
    Init();
}

bool VocalTrack::WantBeatLines(int i1) {
    if (mPlayer->IsNet())
        return false;
    else {
        VocalNoteList *notes = GetVocalNoteList(0);
        std::vector<VocalPhrase> &phrases = notes->mPhrases;
        FOREACH (it, phrases) {
            if (i1 >= it->unk8 && (i1 <= it->unk8 + it->unkc)) {
                return it->mTambourinePhrase;
            }
        }
        return false;
    }
}

int VocalTrack::NumSingers() const {
    if (mPlayer)
        return mPlayer->NumSingers();
    else
        return 0;
}

bool VocalTrack::UseVocalHarmony() {
    if (mPlayer)
        return mPlayer->NumVocalParts() > 1;
    else
        return 0;
}

void VocalTrack::SetVocalStyle(VocalStyle style) {
    // Retail (this SKU) predates the rb3-Wii DEV net-vocals addition: there is no
    // HasNetPlayer()/unk2e5 (mRemoteBandVocals) block here -- the retail body is
    // just the style-changed guard. Confirmed from the target asm (no leading
    // vfptr load + bctrl, no stb to 0x305, and a leaf frame with no GPR saves).
    if (mVocalStyleOverride != style) {
        mVocalStyleOverride = style;
        UpdateVocalStyle();
        TrackPanel *panel = GetTrackPanel();
        panel->unk5f = false;
    }
}

bool VocalTrack::IsScrolling() const {
    if (unk70 == 2)
        return mVocalStyleOverride == kVocalStyleScrolling;
    else
        return unk70 == 1;
}

void VocalTrack::UpdateVocalStyle() {
    std::vector<Player *> &players = TheGame->GetActivePlayers();
    if (mPlayer && mPlayer->IsLocal()) {
        for (int i = 0; i < players.size(); i++) {
            Player *cur = players[i];
            if (cur && cur->GetTrackType() == kTrackVocals) {
                if (cur->GetTrackNum() != mTrackConfig.TrackNum() && cur->IsNet()) {
                    VocalTrack *track =
                        dynamic_cast<VocalTrack *>(cur->GetUser()->GetTrack());
                    if (track)
                        track->SetVocalStyle(mVocalStyleOverride);
                }
            }
        }
    }
    if (mDir) {
        if (mPlayer) {
            EnabledState estate = mPlayer->GetEnabledState();
            if (estate == kPlayerDisabled || estate == kPlayerDisconnected)
                return;
        }
        mDir->UpdateConfiguration();
        unk78 = mDir->mTrackRightX - mDir->mTrackLeftX;
        Symbol song = MetaPerformer::Current()->Song();
        int songID = TheSongMgr.GetSongIDFromShortName(song, true);
        BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(songID);
        // target compiled with ScrollSpeed returning double; cast forces frsp
        float speed = (float)(double)data->ScrollSpeed();
        unk74 = unk78 * speed / 16.8f;
        mDir->Find<RndAnimatable>("tambourine_preview.anim", true)->SetFrame(0, 1);
        RebuildHUD();
    }
}

void VocalTrack::RebuildHUD() {
    static bool sDump;
    for (int i = 0; i < 3; i++) {
        mNextScrollNote[i] = 0;
    }
    for (int i = 0; i < 2; i++) {
        mNextDeployZone[i] = 0;
    }
    for (int i = 0; i < 2; i++) {
        mCurLyricPhrase[i] = 0;
    }

    unk108 = 0;
    unk104 = 1;
    unk100 = 0;
    unkf4 = 0;
    unkf8 = 0;
    unkfc = 0;
    unk23c = mStaticDeployMarginX;
    unk240 = mStaticDeployMarginX;
    mLeadLyricShifts.clear();
    mHarmonyLyricShifts.clear();
    mDir->mLeadLyricScroller->DirtyLocalXfm().v.x = unk23c;
    mDir->mHarmonyLyricScroller->DirtyLocalXfm().v.x = unk240;
    unk2ac = unk23c;
    unk2b0 = unk240;
    unk294 = 0;
    unk298 = 0;
    ClearLyrics();
    ClearMarkers();
    ResetAllTubePlates();
    mTambourineGemPool->FreeUsedGems();
    VocalNoteList *notes = GetVocalNoteList(0);
    if (mPlayer) {
        const VocalPhrase *const &cur = mPlayer->CurrentPhrase();
        const VocalPhrase *next = mPlayer->GetNextPhraseMarker(cur);
        if (HasNetPlayer()) {
            unk70 = 0;
        } else {
            unk70 = 2;
        }
        if (mPlayer->AtFirstPhrase()) {
            mPhraseEndMs = 0;
            BuildPhrase(cur->unk0 + cur->unk4, next->unk0 + next->unk4);
        } else {
            std::vector<VocalPhrase> &phrases = notes->mPhrases;
            if (cur != &*phrases.end()) {
                const VocalPhrase *prev = &*phrases.begin();
                while (prev != &*phrases.end()) {
                    if (mPlayer->GetNextPhraseMarker(prev) == cur)
                        break;
                    prev++;
                }
                if (prev != &*phrases.end()) {
                    if (!IsScrolling()) {
                        mPhraseEndMs = prev->unk0;
                        BuildPhrase(
                            prev->unk0 + prev->unk4, cur->unk0 + cur->unk4
                        );
                    }
                    mPhraseEndMs = prev->unk0 + prev->unk4;
                    float curEnd = cur->unk0 + cur->unk4;
                    float endMs;
                    if (next == &*phrases.end()) {
                        endMs = TheSongDB->GetSongDurationMs();
                    } else {
                        endMs = next->unk0 + next->unk4;
                    }
                    BuildPhrase(curEnd, endMs);
                }
            }
        }
        if (mPlayer->InTambourinePhrase()) {
            mDir->SetTambourine(true);
        }
        unk208 = -1;
        if (mDir->Property(pitch_guides, true)->Sym() == harmonic) {
            int tonic =
                ((BandSongMetadata *)TheSongMgr.Data(TheSongMgr.GetSongIDFromShortName(
                     MetaPerformer::Current()->Song(), true
                 )))
                    ->VocalTonicNote();
            if (tonic != -1)
                unk208 = tonic + 60;
        }
        VocalHUDColor colors[3] = { kVocalColorInvalid,
                                    kVocalColorInvalid,
                                    kVocalColorInvalid };
        Hmx::Object *tubestyle = mDir->mTubeStyle;
        colors[0] = GetVocalHUDColor(tubestyle->Property("lead_color", true)->Sym());
        colors[1] = GetVocalHUDColor(tubestyle->Property("harmony_1_color", true)->Sym());
        colors[2] = GetVocalHUDColor(tubestyle->Property("harmony_2_color", true)->Sym());
        for (int i = 0; i < mPlayer->NumVocalParts(); i++) {
            mPlayer->mVocalParts[i]->unkc8 = colors[i];
        }
        mDir->SetVocalLineColors(colors);
        mDir->mStreakMeter->SetNumParts(mPlayer->NumVocalParts());
        float margin = mDir->mPitchDisplayMargin;
        mRangeShifts.clear();
        std::vector<RangeSection> &sections = TheSongDB->GetRangeSections();
        float prevMin = sections[0].unk8 - margin;
        float prevMax = margin + sections[0].unkc;
        float maxRange = mDir->mMinPitchRange;
        if (sDump) {
            MILO_LOG("Range Shift Data\n");
        }
        for (int i = 0; i < sections.size(); i++) {
            RangeSection &section = sections[i];
            float secMin = section.unk8;
            float secMax = section.unkc;
            if (!(secMax < secMin)) {
                float secIntro = section.unk4;
                RangeShift rs;
                rs.unk0 = TickToMs((float)section.unk0);
                rs.unk4 = prevMin;
                rs.unk8 = prevMax;
                rs.unkc = secMin - margin;
                rs.unk10 = secMax + margin;
                rs.unk14 = secIntro;
                mRangeShifts.push_back(rs);
                prevMin = section.unk8 - margin;
                prevMax = section.unkc + margin;
                float range = prevMax - prevMin;
                float *bigger = (maxRange < range) ? &range : &maxRange;
                maxRange = *bigger;
                if (sDump) {
                    MILO_LOG(
                        "[%d]\tstart ms: %.2f, intro ms: %.2f, min: %.1f -> %.1f, "
                        "max: %.1f -> %.1f\n",
                        i,
                        mRangeShifts.back().unk0,
                        mRangeShifts.back().unk14,
                        mRangeShifts.back().unk4,
                        mRangeShifts.back().unkc,
                        mRangeShifts.back().unk8,
                        mRangeShifts.back().unk10
                    );
                }
            }
        }
        if (maxRange > 0) {
            int idx = 0;
            std::deque<RangeShift>::iterator it = mRangeShifts.begin();
            std::deque<RangeShift>::iterator end = mRangeShifts.end();
            for (; it != end; ++it) {
                float diffFrom = it->unk4 + (maxRange - it->unk8);
                if (diffFrom > 0) {
                    diffFrom *= 0.5f;
                    it->unk4 -= diffFrom;
                    it->unk8 += diffFrom;
                }
                float diffTo = it->unkc + (maxRange - it->unk10);
                if (diffTo > 0) {
                    diffTo *= 0.5f;
                    it->unkc -= diffTo;
                    it->unk10 += diffTo;
                }
                if (sDump) {
                    MILO_LOG(
                        "[%d]\tstart ms: %.2f, intro ms: %.2f, min: %.1f -> %.1f, "
                        "max: %.1f -> %.1f\n",
                        idx++,
                        it->unk0,
                        it->unk14,
                        it->unk4,
                        it->unkc,
                        it->unk8,
                        it->unk10
                    );
                }
            }
        }
        if (mDir->mStreakMeter) {
            int parts = GetNumVocalParts();
            for (int i = 0; i < parts; i++) {
                bool active = false;
                VocalPart *part = mPlayer->mVocalParts[i];
                if (part && !part->InEmptyPhrase()) {
                    active = true;
                }
                mDir->mStreakMeter->SetPartActive(i, active);
            }
        }
        for (int i = 0; i < mPlayer->NumSingers(); i++) {
            if (mPlayer->mSingers[i]) {
                MicClientID id = mPlayer->mSingers[i]->GetMicClientID();
                if (id.mClientID != -1) {
                    PitchArrow *arrow = mDir->GetPitchArrow(id.mClientID);
                    if (arrow) {
                        arrow->ClearParticles();
                    }
                }
            }
        }
        mDir->RefreshCrowdRating(mLastRating, mLastRatingState);
        unk2ec = true;
    }
}

float VocalTrack::GetBottomDisplayPitch() const {
    if (mDir)
        return mDir->mLastMin;
    else
        return 0;
}

float VocalTrack::GetTopDisplayPitch() const {
    if (mDir)
        return mDir->mLastMax;
    else
        return 0;
}

VocalNoteList *VocalTrack::GetVocalNoteList(int part) {
    if (mAlternateNoteList[part])
        return mAlternateNoteList[part];
    else
        return TheSongDB->GetVocalNoteList(part);
}

void VocalTrack::SetAlternateNoteList(int part, VocalNoteList *notes) {
    MILO_ASSERT_RANGE(part, 0, 3, 0x53E);
    mAlternateNoteList[part] = notes;
}

void VocalTrack::HideCoda() {
    unk2ec = false;
    mDir->mBREGrp->SetShowing(false);
    mDir->mLeadBREGrp->SetShowing(false);
    mDir->mHarmonyBREGrp->SetShowing(false);
}

void VocalTrack::DumpLyricPlates(std::deque<LyricPlate *> &plates, bool lead) {
    MILO_LOG("Dumping %s lyric plates\n", lead ? "lead" : "harmony");
    int idx = 0;
    std::deque<LyricPlate *>::iterator it = plates.begin();
    std::deque<LyricPlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        LyricPlate *cur = *it;
        MILO_LOG(
            "[%d] %x (%.2f - %.2f) %s\n",
            idx,
            cur,
            !cur->mSyllables.empty() ? (cur->mSyllables.front()->mHighlightMs) / 1000.0f
                                     : -1.0f,
            cur->mInvalidateMs / 1000.0f,
            cur->mText->RawText().c_str()
        );
        if (cur->Empty()) {
            MILO_LOG("\t<empty>\n");
        } else {
            for (int i = 0; i < cur->mSyllables.size(); i++) {
                Lyric *curLyric = cur->mSyllables[i];
                MILO_LOG("\t[%d] %x", i, curLyric);
                if (curLyric) {
                    MILO_LOG(
                        " %s x:%.2f (%.2f - %.2f)\n",
                        curLyric->mText.c_str(),
                        curLyric->mBeginPos.x,
                        curLyric->mActiveMs / 1000.0f,
                        curLyric->mEndMs / 1000.0f
                    );
                } else
                    MILO_LOG("\n");
            }
        }
        idx++;
    }
    MILO_LOG("\n");
}

void VocalTrack::UpdateTambourineGems() {
    if (!mPlayer)
        return;
    RndMultiMesh *mesh = mDir->Find<RndMultiMesh>("tambourine_gems.mm", true);
    if (!mesh)
        return;
    mesh->Instances().clear();
    std::deque<TambourineGem *> &gems = mTambourineGemPool->mUsedGems;
    if (gems.size() == 0) {
        mDir->Find<RndPropAnim>("tambourine_preview.anim", true)->SetFrame(0.0f, 1.0f);
        return;
    }
    Transform t = mDir->mTambourineSmasher->LocalXfm();
    Multiply(t, mDir->mPitchBottomTrans->LocalXfm(), t);
    const VocalPhrase *cur = mPlayer->CurrentPhrase();
    mPlayer->GetNextPhraseMarker(cur);
    GetVocalNoteList(0);
    for (int i = 0; i != gems.size(); i++) {
        TambourineGem *gem = gems[i];
        int hit = gem->unk8;
        if (hit == 0) {
            t.v.x = unk78 * (gems[i]->unk0 / unk74);
            Transform worldXfm;
            Multiply(t, mDir->mScroller->WorldXfm(), worldXfm);
            if (hit == 0) {
                mesh->Instances().push_back(RndMultiMesh::Instance(worldXfm));
            }
        }
    }
}

void VocalTrack::PollLyricAnimations(
    std::deque<LyricPlate *> &plates, float ms, bool lead
) {
    if (mIntroPlaying)
        return;
    bool scrolling = IsScrolling();
    float plateMs;
    if (scrolling) {
        plateMs = unk2a8;
    } else if (lead) {
        plateMs = unk2ac;
    } else {
        plateMs = unk2b0;
    }
    while (!plates.empty() && !plates.front()->Empty()
           && ((scrolling
                && plates.front()->CurrentEndX(plateMs) < mDir->mTrackLeftX - unk78)
               || plates.front()->mInvalidateMs < ms)) {
        LyricPlate *cur = plates.front();
        if (sDumpLyricPlates) {
            TheDebug << MakeString(
                "recycling lyric plate at %.2f sec %s\n",
                ms / 1000.0f,
                cur->mText->RawText().c_str()
            );
            DumpLyricPlates(plates, !cur->mSyllables.empty());
        }
        plates.pop_front();
        cur->Reset();
        plates.push_back(cur);
    }
    if (TheGame->InRollback())
        ms = unk2a4;
    FOREACH (it, plates) {
        LyricPlate *cur = *it;
        if (cur->Empty())
            return;
        float startX = cur->CurrentStartX(plateMs);
        float endX = cur->CurrentEndX(plateMs);
        if (endX < mDir->mTrackLeftX) {
            cur->SetShowing(false);
        } else if (startX >= mDir->mTrackRightX) {
            cur->SetShowing(false);
            return;
        } else {
            cur->SetShowing(true);
        }
        if (!scrolling) {
            cur->mPastNow = endX < 2.0f * mStaticDeployMarginX + mDir->mNowBarX;
        }
        cur->Poll(ms);
    }
}

void VocalTrack::UpdateLyricZ() {
    bool leadDirty = false;
    float z;
    bool harmonyDirty = false;
    ObjPtr<VocalTrackDir> &_ref0 = mDir;
    _ref0->RecalculateLyricZ(&leadDirty, &harmonyDirty);
    if (leadDirty) {
        std::deque<LyricPlate *>::iterator leadEnd = mLyricsLead.end();
        std::deque<LyricPlate *>::iterator it = mLyricsLead.begin();
        for (; leadEnd != it; ++it) {
            LyricPlate *plate = *it;
            if (plate->mBaked) {
                float delta = 0.0f;
                for (unsigned int i = 0; i < plate->mSyllables.size(); i++) {
                    Lyric *lyric = plate->mSyllables[i];
                    if (lyric->PitchNote()) {
                        z = _ref0->unk694;
                    } else {
                        z = _ref0->unk69c;
                    }
                    if (delta == 0.0f) {
                        delta = z - lyric->mBeginPos.z;
                    } else {
                        float diff = Abs((z - lyric->mBeginPos.z) - delta);
                        if (diff > 0.01f) {
                            MILO_WARN(
                                "relative lyric placement changed in baked plate (lead)"
                            );
                        }
                    }
                    lyric->mBeginPos.z = z;
                }
                plate->mText->DirtyLocalXfm().v.z += delta;
            }
        }
    }
    if (harmonyDirty) {
        std::deque<LyricPlate *>::iterator harmonyEnd = mLyricsHarmony.end();
        for (std::deque<LyricPlate *>::iterator it = mLyricsHarmony.begin();
             it != harmonyEnd; ++it) {
            LyricPlate *plate = *it;
            if (plate->mBaked) {
                float delta = 0.0f;
                for (unsigned int i = 0; i < plate->mSyllables.size(); i++) {
                    Lyric *lyric = plate->mSyllables[i];
                    float z;
                    if (lyric->PitchNote()) {
                        z = _ref0->unk698;
                    } else {
                        z = _ref0->unk6a0;
                    }
                    if (delta == 0.0f) {
                        delta = z - lyric->mBeginPos.z;
                    } else {
                        float diff = Abs((z - lyric->mBeginPos.z) - delta);
                        if (diff > 0.01f) {
                            MILO_WARN(
                                "relative lyric placement changed in baked plate (harmony)"
                            );
                        }
                    }
                    lyric->mBeginPos.z = z;
                }
                plate->mText->DirtyLocalXfm().v.z += delta;
            }
        }
    }
}

void PrintLyricOneLine(const Lyric &);

void VocalTrack::UpdateScrolling(float ms) {
    static bool dumpLyrics;
    static bool dumpDeployVectors;
    static bool warnOnMarkerCreation;

    if (!mPlayer)
        return;
    if (mPlayer->IsGameOver())
        return;
    if (ms < 0.0f)
        return;
    float trackScale = unk74;
    float trackWidth = unk78;
    float lookAhead = trackScale * 64.0f + ms;
    float buildAhead =
        trackScale * ((mDir->mTrackLeftX - trackWidth) / trackWidth) + ms;
    if (mPlayer->IsNet()) {
        if (mPlayer->mEnabledState == kPlayerDisabled
            || mPlayer->mEnabledState == kPlayerDisconnected)
            return;
    }

    float sectionStart = FLT_MAX;
    float sectionEnd = -FLT_MAX;
    bool sectionOnly = mPlayer->SongSectionOnly(sectionStart, sectionEnd);
    if (sectionOnly && sectionEnd < lookAhead) {
        lookAhead = sectionEnd;
    }

    if (!mPlayer->InTambourinePhrase()) {
        for (int part = 0; part < mPlayer->NumVocalParts(); part++) {
            VocalNoteList *notes = GetVocalNoteList(part);
            if (!notes)
                continue;
            int idx = mNextScrollNote[part];
            while (idx < notes->mNotes.size()) {
                const VocalNote &n = notes->mNotes[idx];
                if (!n.mUnpitchedNote) {
                    mNextScrollNote[part] = idx;
                }
                if (sectionOnly && (n.mMs + n.mDurationMs) < sectionStart) {
                    if (idx == notes->mNotes.size() - 1) {
                        mNextScrollNote[part] = notes->mNotes.size();
                    }
                    idx++;
                    continue;
                }
                if ((n.mMs + n.mDurationMs) > buildAhead)
                    break;
                idx++;
            }
            if (part < 2) {
                int dz = mNextDeployZone[part];
                while (dz < notes->mFreestyleSections.size()
                       && notes->mFreestyleSections[dz].second <= buildAhead) {
                    mNextDeployZone[part] = dz;
                    dz++;
                }
            }
            int prepEnd = mNextScrollNote[part];
            while (prepEnd < notes->mNotes.size()
                   && notes->mNotes[prepEnd].mMs <= lookAhead) {
                prepEnd++;
            }
            PrepareNoteTubes(unk74, mNextScrollNote[part], prepEnd, part);
            mNextScrollNote[part] = prepEnd;
        }
    }

    int beat = unk108;
    while (true) {
        int tick = (int)BeatToTick((float)beat);
        float beatMs = TickToMs((float)tick);
        if (beatMs < buildAhead) {
            beat++;
            continue;
        }
        if (beatMs > lookAhead)
            break;
        if (WantBeatLines(tick)) {
            if (TheBeatMap->IsDownbeat(beat)) {
                CreateMarker("downbeat_marker.mesh", beatMs, warnOnMarkerCreation);
            } else {
                CreateMarker("beat_marker.mesh", beatMs, warnOnMarkerCreation);
            }
        }
        beat++;
    }
    unk108 = beat;

    int phraseIdx = unk104;
    VocalNoteList *leadNotes = GetVocalNoteList(0);
    while (phraseIdx < leadNotes->mPhrases.size()) {
        const VocalPhrase &ph = leadNotes->mPhrases[phraseIdx];
        float phMs = ph.unk0 + ph.unk4;
        if (phMs < buildAhead
            || (sectionOnly && phMs < (sectionStart - 100.0f))) {
            phraseIdx++;
            continue;
        }
        if (phMs > lookAhead || (sectionOnly && phMs > sectionEnd))
            break;
        CreateMarker("phrase_marker.mesh", phMs, warnOnMarkerCreation);
        phraseIdx++;
    }
    unk104 = phraseIdx;

    float oldRange = mDir->mLastMax - mDir->mLastMin;
    while (mRangeShifts.size() != 0
           && mRangeShifts.front().unk0 < ms - mRangeShifts.front().unk4) {
        RangeShift &rs = mRangeShifts.front();
        mDir->SetRange(rs.unk8, rs.unkc, unk208, false);
        mRangeShifts.pop_front();
    }
    if (mRangeShifts.size() != 0) {
        RangeShift &rs = mRangeShifts.front();
        if (rs.unk0 < ms) {
            float t = (ms - rs.unk0) / rs.unk4;
            t = Clamp<float>(0.0f, 1.0f, t);
            mDir->SetRange(
                t * (rs.unk10 - rs.unk8) + rs.unk8,
                t * (rs.unk14 - rs.unkc) + rs.unkc,
                unk208,
                false
            );
        }
    }
    float newRange = mDir->mLastMax - mDir->mLastMin;
    float rangeDelta = oldRange - newRange;
    if (rangeDelta < 0.0f)
        rangeDelta = -rangeDelta;
    if (rangeDelta > 0.1f) {
        for (int p = 0; p < 3; p++) {
            mNextScrollNote[p] = 0;
            if (p < 2) {
                mNextDeployZone[p] = 0;
                mCurLyricPhrase[p] = 0;
            }
        }
        ResetAllTubePlates();
        ClearLyrics();
    }

    if (!InTambourinePhrase()) {
        float lyricMs = TheGame->InRollback() ? unk2a4 : ms;
        for (int side = 0; side < 2; side++) {
            bool sideLead = side == 0;
            std::deque<LyricShift> &shifts =
                sideLead ? mLeadLyricShifts : mHarmonyLyricShifts;
            RndTransformable *scroller =
                (sideLead ? mDir->mLeadLyricScroller
                          : mDir->mHarmonyLyricScroller)
                    .Ptr();
            float &xPos = sideLead ? unk294 : unk298;
            float &shiftedX = sideLead ? unk2ac : unk2b0;
            while (shifts.size() != 0) {
                LyricShift &shift = shifts.front();
                float window = shift.unk8 ? mLyricShiftQuickMs : mLyricShiftMs;
                if (shift.unk4 >= (lyricMs - window))
                    break;
                xPos = shift.unk0;
                Vector3 pos(scroller->LocalXfm().v);
                pos.x = shift.unk0;
                scroller->SetLocalPos(pos);
                shiftedX = shift.unk0 + mDir->mNowBarX;
                shifts.pop_front();
            }
            if (shifts.size() != 0) {
                LyricShift &shift = shifts.front();
                float window = shift.unk8 ? mLyricShiftQuickMs : mLyricShiftMs;
                if (shift.unk4 < lyricMs) {
                    float t = (lyricMs - shift.unk4) / window;
                    t = Clamp<float>(0.0f, 1.0f, t);
                    float curX = t * (shift.unk0 - xPos) + xPos;
                    Vector3 pos(scroller->LocalXfm().v);
                    pos.x = curX;
                    scroller->SetLocalPos(pos);
                    shiftedX = curX + mDir->mNowBarX;
                }
            }
        }
    }

    if (!mPlayer->IsNet()) {
        mTambourineGemPool->FreeOldGems(ms - 250.0f);
        const std::vector<int> &tambGems =
            mPlayer->mTambourineManager.TambourineGems();
        int targetTick = (int)MsToTick(lookAhead);
        int gemIdx = unk100;
        while (gemIdx < tambGems.size() && tambGems[gemIdx] < targetTick) {
            float gemMs = TickToMs((float)tambGems[gemIdx]);
            mTambourineGemPool->NewGem(gemMs, gemIdx);
            gemIdx++;
        }
        unk100 = gemIdx;
    }

    UpdateLyricZ();

    int isolated = -1;
    int numParts = mPlayer->NumVocalParts();
    if (!mPlayer->InTambourinePhrase()) {
        isolated = mDir->unk6c4;
    }

    bool inPractice = !InTambourinePhrase();
    for (int part = 0; part < numParts; part++) {
        if (!(isolated == part || (isolated == -1 && part != 2)))
            continue;

        VocalNoteList *notes = GetVocalNoteList(part);
        bool wantLyrics = (notes != NULL);
        bool dirWant =
            (part != 0) ? (bool)mDir->mHarmLyrics : (bool)mDir->mLeadLyrics;
        if (wantLyrics != dirWant) {
            wantLyrics = dirWant;
            mDir->Reset();
        }
        if (!wantLyrics)
            continue;

        VocalNoteList *phraseNotes = (part != 2) ? notes : GetVocalNoteList(1);
        std::vector<VocalPhrase> &lyricPhrases = phraseNotes->mLyricPhrases;
        bool lead = (part == 0);

        RndGroup *grp = lead
            ? mDir->Find<RndGroup>("lyrics.grp", true)
            : mDir->Find<RndGroup>("lyrics_harmony.grp", true);
        bool staticLyrics = !IsScrolling();
        VocalNoteList *altNotes =
            (!lead && isolated < 1) ? GetVocalNoteList(2) : NULL;
        std::vector<std::pair<float, float> > &freestyles =
            notes->mFreestyleSections;
        ObjPtr<RndTransformable> *scrollerPtr = !staticLyrics
            ? &mDir->mScroller
            : (lead ? &mDir->mLeadLyricScroller
                    : &mDir->mHarmonyLyricScroller);
        RndTransformable *scroller = scrollerPtr->Ptr();

        int *itPPtr = lead ? &unkf4 : (part == 1 ? &unkf8 : &unkfc);
        VocalNote *itT = &notes->mNotes[*itPPtr];
        VocalNote *notesEnd = &notes->mNotes[notes->mNotes.size()];
        VocalNote *altIt =
            altNotes ? &altNotes->mNotes[unkfc] : notesEnd;
        VocalNote *altEnd =
            altNotes ? &altNotes->mNotes[altNotes->mNotes.size()] : notesEnd;
        if (itT == notesEnd && altIt == altEnd)
            continue;

        std::deque<LyricPlate *> &plates = lead ? mLyricsLead : mLyricsHarmony;

        float scrollerWidth =
            mDir->mTrackRightX - (lead ? unk2ac : unk2b0);
        float scrollingLastX = -1000.0f;
        float &lastLyricX = staticLyrics ? (lead ? unk23c : unk240) : scrollingLastX;
        Lyric *latest = GetLastLyric(plates);
        if (latest) {
            lastLyricX = latest->EndPos();
        }

        if (dumpDeployVectors) {
            MILO_WARN("deploy zones for part %d by song seconds\n", part);
            for (int i = 0; i < freestyles.size(); i++) {
                MILO_WARN(
                    "[%d] %.2f - %.2f\n",
                    i,
                    freestyles[i].first / 1000.0f,
                    freestyles[i].second / 1000.0f
                );
            }
            MILO_WARN("--------\n");
        }

        int *curPhPtr = &mCurLyricPhrase[std::min(part, 1)];
        int *curDeployPtr = &mNextDeployZone[std::min(part, 1)];
        int curDeploy = *curDeployPtr;
        for (;;) {
            if (!(*curPhPtr < lyricPhrases.size()))
                break;
            VocalPhrase &lyrPh = lyricPhrases[*curPhPtr];
            float phStartMs = TickToMs((float)lyrPh.unk8);
            float phEndMs = TickToMs((float)(lyrPh.unk8 + lyrPh.unkc));
            if (mPlayer) {
                int playerState = mPlayer->mEnabledState;
                if (playerState != kPlayerEnabled
                    && playerState != kPlayerBeingSaved
                    && playerState != kPlayerDroppingIn)
                    break;
            }
            if (staticLyrics) {
                bool tooWide = lastLyricX > scrollerWidth;
                bool highlightStarted =
                    (phStartMs - mMinPhraseHighlightMs) > 0.0f;
                if (!tooWide)
                    goto window_ok;
                if (!highlightStarted)
                    goto window_ok;
                break;
            } else if (phStartMs > lookAhead)
                break;
        window_ok:
            if (sectionOnly && phStartMs > (sectionEnd - 100.0f))
                break;

            bool isPast = phEndMs < (staticLyrics ? ms : buildAhead);
            if (sectionOnly && !isPast && phEndMs > sectionStart) {
                isPast = true;
                for (VocalNote *skipIt = itT; skipIt != notesEnd; skipIt++) {
                    if (skipIt->mMs > phEndMs)
                        break;
                    if (skipIt->mMs + skipIt->mDurationMs > sectionStart) {
                        isPast = false;
                    }
                }
                if (altNotes) {
                    for (VocalNote *skipAlt = altIt; skipAlt != altEnd; skipAlt++) {
                        if (skipAlt->mMs > phEndMs)
                            break;
                        if (skipAlt->mMs + skipAlt->mDurationMs > sectionStart) {
                            isPast = false;
                        }
                    }
                }
            }

            if (isPast) {
                while (itT != notesEnd && !(itT->mMs > phEndMs)) {
                    itT++;
                }
                if (altNotes) {
                    while (altIt != altEnd && !(altIt->mMs > phEndMs)) {
                        altIt++;
                    }
                    unkfc = (int)(altIt - &altNotes->mNotes[0]);
                }
                while (*curDeployPtr < freestyles.size()
                       && freestyles[*curDeployPtr].second < phEndMs) {
                    (*curDeployPtr)++;
                }
                (*curPhPtr)++;
                curDeploy = *curDeployPtr;
                continue;
            }

            LyricPlate *plate = GetNextLyricPlate(plates, lead);
            plate->HookUpParents(grp, scroller);
            if (staticLyrics) {
                plate->mInvalidateMs = phStartMs;
            }

            Lyric *staticFirst = NULL;
            Lyric *staticLast = NULL;
            float staticLeftX = lastLyricX;
            float staticY = lastLyricX;
            float tmpEndPos = lastLyricX;
            while (itT != notesEnd) {
                if (altNotes) {
                    VocalNote *curAlt = altIt;
                    while (curAlt != altEnd && curAlt->mMs < itT->mMs
                           && !(curAlt->mMs > phEndMs)) {
                        if (!curAlt->mBends && curAlt->mAllowCombine) {
                            if (itT->mAllowCombine && IdenticalLyric(*curAlt, *itT)) {
                            } else if (latest && latest->mVocalNotes.size()
                                       && IdenticalLyric(
                                           *curAlt,
                                           *latest->mVocalNotes[0]
                                       )) {
                            } else {
                                float altMs = curAlt->mMs;
                                const VocalNote *noteRef = curAlt;
                                Lyric *newLyric = CreateLyric(
                                    noteRef,
                                    altNotes->mNotes,
                                    lead,
                                    false,
                                    true
                                );
                                curAlt = (VocalNote *)noteRef;
                                if (newLyric) {
                                    bool deployHit = CheckDeploySections(
                                        newLyric,
                                        altMs,
                                        curDeploy,
                                        freestyles,
                                        staticLyrics,
                                        latest,
                                        tmpEndPos
                                    );
                                    ProcessStaticLyrics(
                                        staticLyrics,
                                        newLyric,
                                        staticLeftX,
                                        tmpEndPos,
                                        staticFirst,
                                        staticLast,
                                        staticY,
                                        deployHit,
                                        plate
                                    );
                                    plate->AddLyric(newLyric);
                                    latest = plate->LatestLyric();
                                    if (dumpLyrics) {
                                        MILO_WARN(
                                            "NEW EXTRA LYRIC: \"%s\" @ %d\n",
                                            newLyric->mText.c_str(),
                                            newLyric->StartTick()
                                        );
                                    }
                                }
                            }
                        }
                        curAlt++;
                    }
                    altIt = curAlt;
                    unkfc = (int)(altIt - &altNotes->mNotes[0]);
                }

                if (itT->mMs > phEndMs)
                    break;
                const VocalNote *noteRef = itT;
                Lyric *newLyric =
                    CreateLyric(noteRef, notes->mNotes, lead, false, false);
                itT = (VocalNote *)noteRef;
                if (newLyric) {
                    if (altNotes && !itT->mAllowCombine) {
                        delete newLyric;
                    } else {
                        bool deployHit = CheckDeploySections(
                            newLyric,
                            itT->mMs,
                            curDeploy,
                            freestyles,
                            staticLyrics,
                            latest,
                            tmpEndPos
                        );
                        ProcessStaticLyrics(
                            staticLyrics,
                            newLyric,
                            staticLeftX,
                            tmpEndPos,
                            staticFirst,
                            staticLast,
                            staticY,
                            deployHit,
                            plate
                        );
                        plate->AddLyric(newLyric);
                        latest = plate->LatestLyric();
                        if (dumpLyrics) {
                            MILO_WARN(
                                "NEW LYRIC: \"%s\" @ %d\n",
                                newLyric->mText.c_str(),
                                newLyric->StartTick()
                            );
                        }
                    }
                }
                itT++;
            }

            if (altNotes && itT == notesEnd) {
                VocalNote *curAlt = altIt;
                while (curAlt != altEnd && !(curAlt->mMs > phEndMs)) {
                    if (!curAlt->mBends && curAlt->mAllowCombine) {
                        if (latest && latest->mVocalNotes.size()
                            && IdenticalLyric(*curAlt, *latest->mVocalNotes[0])) {
                        } else {
                            float altMs = curAlt->mMs;
                            const VocalNote *noteRef = curAlt;
                            Lyric *newLyric = CreateLyric(
                                noteRef,
                                altNotes->mNotes,
                                lead,
                                false,
                                true
                            );
                            curAlt = (VocalNote *)noteRef;
                            if (newLyric) {
                                bool deployHit = CheckDeploySections(
                                    newLyric,
                                    altMs,
                                    curDeploy,
                                    freestyles,
                                    staticLyrics,
                                    latest,
                                    tmpEndPos
                                );
                                ProcessStaticLyrics(
                                    staticLyrics,
                                    newLyric,
                                    staticLeftX,
                                    tmpEndPos,
                                    staticFirst,
                                    staticLast,
                                    staticY,
                                    deployHit,
                                    plate
                                );
                                plate->AddLyric(newLyric);
                                latest = plate->LatestLyric();
                                if (dumpLyrics) {
                                    MILO_WARN(
                                        "NEW EXTRA LYRIC: \"%s\" @ %d\n",
                                        newLyric->mText.c_str(),
                                        newLyric->StartTick()
                                    );
                                }
                            }
                        }
                    }
                    curAlt++;
                }
                altIt = curAlt;
                unkfc = (int)(altIt - &altNotes->mNotes[0]);
            }

            if (staticLyrics && plates.size() != 0) {
                Lyric *latestNow = plate->LatestLyric();
                if (latestNow)
                    latestNow->SetChunkEnd(true);
            }
            (*curPhPtr)++;
        }
        *itPPtr = (int)(itT - &notes->mNotes[0]);

        int colorBase = (staticLyrics ? 8 : 0) | (lead ? 4 : 0);
        Hmx::Color activeColor = mDir->GetLyricColor(colorBase | 1);
        Hmx::Color nowColor = mDir->GetLyricColor(colorBase | 2);
        Hmx::Color pastColor = mDir->GetLyricColor(colorBase | 3);
        Hmx::Color previewColor = mDir->GetLyricColor(colorBase);
        Hmx::Color activePhonemeColor = mDir->GetLyricColor(colorBase | 0x11);
        Hmx::Color nowPhonemeColor = mDir->GetLyricColor(colorBase | 0x12);
        Hmx::Color pastPhonemeColor = mDir->GetLyricColor(colorBase | 0x13);
        Hmx::Color previewPhonemeColor = mDir->GetLyricColor(colorBase | 0x10);
        float previewAlpha = mDir->GetLyricAlpha(colorBase);
        float activeAlpha = mDir->GetLyricAlpha(colorBase | 1);
        float nowAlpha = mDir->GetLyricAlpha(colorBase | 2);
        float pastAlpha = mDir->GetLyricAlpha(colorBase | 3);
        Lyric *prevBakedLyric = GetLastBakedLyric(plates);
        for (std::deque<LyricPlate *>::iterator pit = plates.begin();
             pit != plates.end();
             ++pit) {
            LyricPlate *plate = *pit;
            if (plate->Empty())
                continue;
            if (plate->Baked())
                continue;
            plate->mBaked = true;
            if (staticLyrics) {
                plate->UpdateStaticTiming(mMinPhraseHighlightMs);
            }
            int phraseTick = (int)MsToTick(plate->mSyllables.front()->mActiveMs);
            int commonPhraseID = TheSongDB->GetCommonPhraseID(
                mTrackConfig.TrackNum(), phraseTick
            );
            bool spotlight = commonPhraseID != -1;
            plate->mActiveColor = spotlight ? activePhonemeColor : activeColor;
            plate->mNowColor = spotlight ? nowPhonemeColor : nowColor;
            plate->mPastColor = spotlight ? pastPhonemeColor : pastColor;
            plate->mPreviewColor = spotlight ? previewPhonemeColor : previewColor;
            plate->mActivePhonemeColor = plate->mActiveColor;
            plate->mNowPhonemeColor = plate->mNowColor;
            plate->mPastPhonemeColor = plate->mPastColor;
            plate->mPreviewPhonemeColor = plate->mPreviewColor;
            plate->mActiveColor.alpha = activeAlpha;
            plate->mNowColor.alpha = nowAlpha;
            plate->mPastColor.alpha = pastAlpha;
            plate->mPreviewColor.alpha = previewAlpha;
            plate->mActivePhonemeColor.alpha = activeAlpha;
            plate->mNowPhonemeColor.alpha = nowAlpha;
            plate->mPastPhonemeColor.alpha = pastAlpha;
            plate->mPreviewPhonemeColor.alpha = previewAlpha;

            for (std::vector<Lyric *>::iterator lit = plate->mSyllables.begin();
                 lit != plate->mSyllables.end();
                 ++lit) {
                Lyric *lyric = *lit;
                float lyricX;
                if (staticLyrics) {
                    lyricX = lastLyricX;
                    if (lyric->mDeployIdx != -1) {
                        if (lyricX < (0.01f + mStaticDeployMarginX)) {
                            lyricX -= mStaticDeployMarginX;
                        }
                        if (lyric->mDeployIdx < mNextDeployZone[std::min(part, 1)]) {
                            if (!sectionOnly) {
                                MILO_WARN(
                                    "lyric.mDeployIdx (%d) < mNextDeployZone (%d) for part %d\n",
                                    lyric->mDeployIdx,
                                    mNextDeployZone[std::min(part, 1)],
                                    part
                                );
                            }
                            lyric->mDeployIdx = -1;
                        } else {
                            float deployWidth = mStaticDeployZoneXSize;
                            lyricX += ((deployWidth + mStaticDeployBufferX)
                                       * (float)(lyric->mDeployIdx
                                                 - mNextDeployZone[std::min(part, 1)]))
                                + (deployWidth + mStaticDeployMarginX);
                        }
                    }
                } else {
                    float startMs = TickToMs((float)lyric->StartTick());
                    MsToTick(startMs - unk74);
                    lyricX = unk78 * (startMs / unk74);
                }
                Vector3 beginPos;
                beginPos.y = 0.0f;
                if (lyric->PitchNote()) {
                    beginPos.z = lead ? mDir->unk694 : mDir->unk698;
                } else {
                    beginPos.z = lead ? mDir->unk69c : mDir->unk6a0;
                }
                if (lyricX < lastLyricX) {
                    lyricX = lastLyricX;
                }
                beginPos.x = lyricX;
                lyric->mBeginPos = beginPos;
                plate->BakeLyric(lyric);
                lastLyricX = lyricX + lyric->Width();
                if (staticLyrics) {
                    std::deque<LyricShift> &shifts =
                        lead ? mLeadLyricShifts : mHarmonyLyricShifts;
                    if (prevBakedLyric && prevBakedLyric->mChunkEnd) {
                        float shiftStart = prevBakedLyric->mEndMs;
                        float prevWidth = prevBakedLyric->Width();
                        float earlyBase = lyric->mActiveMs - mLyricShiftMs;
                        float shiftBase =
                            mStaticDeployMarginX - prevBakedLyric->mBeginPos.x;
                        float earlyShift = earlyBase - mLyricShiftAnticipationMs;
                        float shiftX = shiftBase - prevWidth;
                        if (earlyShift < shiftStart)
                            shiftStart = earlyShift;
                        float minHighlight =
                            prevBakedLyric->mActiveMs + mMinLyricHighlightMs;
                        float *prevEnd = &prevBakedLyric->mEndMs;
                        if (minHighlight < prevBakedLyric->mEndMs)
                            prevEnd = &minHighlight;
                        if (shiftStart < *prevEnd)
                            shiftStart = *prevEnd;
                        bool fast = false;
                        float preview = lyric->mActiveMs - shiftStart;
                        if ((preview - mLyricShiftMs)
                            < mLyricShiftAnticipationMs) {
                            fast = true;
                        }
                        shifts.push_back(LyricShift(shiftStart, shiftX, fast));
                    }
                    if (*curDeployPtr < freestyles.size()
                        && lyric->mDeployIdx > -1) {
                        while (*curDeployPtr <= lyric->mDeployIdx) {
                            int deployDelta = lyric->mDeployIdx - *curDeployPtr;
                            std::pair<float, float> &freestyle =
                                freestyles[*curDeployPtr];
                            ConfigNoteTube(false, 2, std::min(part, 1), true, 1.0f);
                            HookupTubePlates(mNoteTube);
                            float tubeEndX = lyric->mBeginPos.x
                                - (2.0f * mStaticDeployMarginX);
                            float tubeX = (lyric->mBeginPos.x
                                           - mStaticDeployZoneXSize)
                                - mStaticDeployMarginX;
                            for (int i = 0; i < deployDelta; i++) {
                                float space =
                                    mStaticDeployZoneXSize + mStaticDeployBufferX;
                                tubeX -= space;
                                tubeEndX -= space;
                            }
                            if (deployDelta) {
                                float nextStart = freestyle.second + mLyricShiftMs;
                                if ((*curDeployPtr + 1) < freestyles.size()
                                    && freestyles[*curDeployPtr + 1].first < nextStart) {
                                    nextStart = freestyles[*curDeployPtr + 1].first;
                                }
                                shifts.push_back(
                                    LyricShift(
                                        nextStart,
                                        -tubeEndX - mStaticDeployBufferX
                                    )
                                );
                            } else {
                                shifts.push_back(
                                    LyricShift(freestyle.second, -tubeEndX)
                                );
                            }
                            bool inCoda = TheSongDB->IsInCoda(
                                MsToTickInt(freestyle.second)
                            );
                            float z;
                            RndGroup *parent;
                            float height;
                            if (part == 0) {
                                z = (mDir->mTrackBottomZ + mDir->mPitchBottomZ)
                                    * 0.5f;
                                parent = inCoda ? mDir->mLeadBREGrp
                                                : mDir->mLeadLyricScrollGroup;
                                height = mDir->mLeadLyricHeight * 0.5f;
                            } else {
                                z = (mDir->mTrackTopZ + mDir->mPitchTopZ) * 0.5f;
                                parent = inCoda ? mDir->mHarmonyBREGrp
                                                : mDir->mHarmonyLyricScrollGroup;
                                height = mDir->mHarmLyricHeight * 0.5f;
                            }
                            mNoteTube->SetPointPos(0, Vector3(0.0f, 0.0f, z));
                            mNoteTube->SetPointPos(
                                1, Vector3(tubeEndX - tubeX, 0.0f, z)
                            );
                            mNoteTube->unk_0x30 = height;
                            mNoteTube->SetBackParent(parent);
                            mNoteTube->SetXPos(tubeX);
                            mNoteTube->CreateMeshes();
                            mNoteTube->SetDeployTiming(
                                freestyle.first, freestyle.second
                            );
                            mNoteTube->BakePlates();
                            (*curDeployPtr)++;
                        }
                    }
                }
                prevBakedLyric = lyric;
            }
            plate->CheckSync();
        }

        if (staticLyrics && (int)lyricPhrases.size() == *curPhPtr
            && *curDeployPtr < freestyles.size()) {
            std::deque<LyricShift> &shifts =
                lead ? mLeadLyricShifts : mHarmonyLyricShifts;
            if (prevBakedLyric) {
                shifts.push_back(
                    LyricShift(
                        prevBakedLyric->mEndMs,
                        mStaticDeployMarginX
                            + (-lastLyricX - mStaticDeployBufferX)
                    )
                );
            }
            int codaTick = TheSongDB->GetCodaStartTick();
            while (*curDeployPtr < freestyles.size()) {
                const std::pair<float, float> *section = &freestyles[*curDeployPtr];
                float nextStart =
                    ((*curDeployPtr + 1) < freestyles.size())
                        ? freestyles[*curDeployPtr + 1].first
                        : -1.0f;
                if (codaTick != -1) {
                    float codaMs = TickToMs((float)codaTick);
                    if (section->first < codaMs && codaMs < section->second) {
                        std::pair<float, float> beforeCoda(
                            section->first, codaMs
                        );
                        std::pair<float, float> afterCoda(
                            codaMs, section->second
                        );
                        BuildStaticDeployZone(
                            std::min(part, 1), beforeCoda, codaMs, lastLyricX, shifts
                        );
                        section = &afterCoda;
                    }
                }
                BuildStaticDeployZone(
                    std::min(part, 1), *section, nextStart, lastLyricX, shifts
                );
                (*curDeployPtr)++;
            }
        }

    }

    dumpDeployVectors = false;
    if (IsScrolling() && !sectionOnly) {
        BuildScrollingDeployZones(lookAhead);
    }
    float invMsLeft = unk74
            * ((mDir->mTrackLeftX - mDir->mNowBarX) / unk78)
        + ms;
    float invMsRight = unk74
            * ((mDir->mTrackRightX - mDir->mNowBarX) / unk78)
        + ms;
    InvalidateMarkers(buildAhead);
    UpdateMarkerVisibility(invMsLeft, invMsRight);
    UpdateAllTubePlates(ms);
}

void VocalTrack::Poll(float f1) {
    bool gamebool = TheGame->InRollback();
    if (f1 < unk2a4 && !gamebool) {
        RebuildHUD();
    }
    float f6 = unk78 * -(f1 / unk74);
    mDir->mScroller->SetLocalPos(Vector3(f6, 0, 0));
    unk2a8 = f6 + mDir->mNowBarX;
    Track::Poll(f1);
    mDir->UpdatePartIsolation();
    mDir->SortArrowFx();
    UpdateScrolling(f1);
    UpdateTambourineGems();
    if (f1 > 0) {
        PollLyricAnimations(mLyricsLead, f1, true);
        PollLyricAnimations(mLyricsHarmony, f1, false);
    }
    PollKaraoke(f1);
    if (unk68) {
        const char *txt = MakeString("current: %i\n", mPlayer->PhraseScore());
        mDir->Find<RndText>("debug_score_current.txt", true)->SetText(txt);
    }
    if (!gamebool)
        unk2a4 = f1;
    if (mPlayer && unk2ec) {
        if (!mPlayer->CanDeployCoda()) {
            HideCoda();
        }
    }
}

void VocalTrack::PollKaraoke(float f1) {
    if (mPlayer) {
        int numSingers = mPlayer->NumSingers();
        // retail (this SKU) has no unk2e5 guard here — StartUpdateArrows/
        // UpdatePitchArrow/UpdateUnusedArrows run unconditionally, unlike
        // rb3-Wii's `if (!mRemoteBandVocals)` gate. Verified against target
        // disassembly for fn_82B70860 (PollKaraoke): no this+0x... load or
        // branch exists between the NumSingers() computation and the
        // StartUpdateArrows() call.
        StartUpdateArrows();
        for (int i = 0; i < numSingers; i++) {
            UpdatePitchArrow(f1, i);
        }
        UpdateUnusedArrows();
        float f7 = 0;
        for (int i = 0; i < 3; i++) {
            float clamped = Clamp<float>(0, 1, mPlayer->FramePhraseMeterFrac(i));
            int rating = mPlayer->CalculatePhraseRating(clamped);
            bool isHighRating = rating >= 4;
            mDir->mStreakMeter->SetPartPct(i, clamped, isHighRating);
            if (clamped > f7)
                f7 = clamped;
        }
        mDir->SetStreakPct(f7);
    }
}

bool VocalTrack::InTambourinePhrase() const {
    Player *p = GetPlayer();
    if (!p)
        return false;
    return p->InTambourinePhrase();
}

void VocalTrack::StartUpdateArrows() {
    for (int i = 0; i < 3; i++) {
        if (mDir->GetPitchArrow(i)) {
            mDir->GetPitchArrow(i)->unk18c = true;
        }
    }
}

void VocalTrack::UpdatePitchArrow(float ms, int singerIdx) {
    int phraseID =
        TheSongDB->GetCommonPhraseID(mTrackConfig.TrackNum(), MsToTickInt(ms));
    VocalPlayer *player = mPlayer;
    bool spotlight = phraseID != -1;
    bool enabled = player && player->GetEnabledState() == kPlayerEnabled;
    Singer *singer = player->mSingers[singerIdx];
    float pitchFrame = 0;
    int arrowIdx = singerIdx;
    if (singer) {
        arrowIdx = singer->GetMicClientID().mClientID;
    }
    PitchArrow *arrow = mDir->GetPitchArrow(arrowIdx);
    if (arrow) {
        int matchType = singer->GetFrameMatchType();
        bool inPhonemePhrase = matchType == 1;
        arrow->SetPitched(!inPhonemePhrase);
        arrow->SetSpotlight(spotlight);
        bool clampPitch = true;
        // Retail (this SKU) has NO gDebugSpew blocks anywhere in UpdatePitchArrow:
        // the target function never loads the gDebugSpew global and makes exactly
        // the 20 calls of the non-debug path (the rb3-Wii DEV decomp keeps them).
        if (enabled && inPhonemePhrase) {
            VocalPart *part = NULL;
            if (singer->mFrameAssignedPart > -1) {
                part = mPlayer->mVocalParts[singer->mFrameAssignedPart];
            }
            VocalHUDColor color = (VocalHUDColor)-1;
            if (part) {
                color = (VocalHUDColor)part->unkc8;
            }
            arrow->SetFrameScore(singer->mLastFrameMicEnergy, color, 0.0f);
        } else if (matchType != 0 || 0.0f == singer->mFrameMicPitch) {
            arrow->SetFrameScore(0.0f, (VocalHUDColor)-1, 0.0f);
        } else if (enabled && singer->mFrameTargetPitch > 0.0f) {
            VocalPart *part = NULL;
            float frameScore = singer->mFrameBestHitScore;
            if (singer->mFrameAssignedPart > -1) {
                part = mPlayer->mVocalParts[singer->mFrameAssignedPart];
            }
            VocalHUDColor color = (VocalHUDColor)-1;
            if (part) {
                color = (VocalHUDColor)part->unkc8;
            }
            pitchFrame = singer->mFrameTargetPitch - singer->mFrameMicPitch;
            float harmonyScore = GetHarmonyScore(singerIdx);
            arrow->SetFrameScore(frameScore, color, harmonyScore);
        } else {
            arrow->SetFrameScore(0.0f, (VocalHUDColor)-1, 0.0f);
            clampPitch = false;
        }
        if (singer->mFrameMicPitch > 0.0f) {
            // Retail copies the whole translation by value into a stack temp and
            // then writes v.z in BOTH arms (the target reads v.z back off the
            // temp at 0x68(r1)); the rb3-Wii DEV form extracts x/y/z into three
            // scalars and rebuilds a Vector3 temp at the call, which costs an
            // extra float-store trio.
            Vector3 v = arrow->LocalXfm().v;
            float pitchZ = mDir->PitchToZ(singer->mFrameMicPitch, clampPitch);
            if (std::fabs((pitchZ - v.z / mDir->mPitchTopZ) - mDir->mPitchBottomZ) >
                0.9f) {
                v.z = pitchZ;
            } else {
                v.z = pitchZ + mDir->mArrowSmoothing * (v.z - pitchZ);
            }
            arrow->SetLocalPos(v);
        }
        arrow->SetTiltDegrees(5.0f * pitchFrame);
        float volume = Clamp<float>(0, 1, 4.0f * singer->mLastFrameMicEnergy);
        if (singer->mFrameAssignedPart != -1) {
            volume = std::max<float>(volume, 0.5f);
        }
        arrow->SetVolume(volume);
        arrow->SetGhostFade(0.0f);
        arrow->SetSplit(false);
        if (mPlayer->Freestyling()) {
            mDir->Find<RndAnimatable>("vocal_feedback.anim", true)
                ->SetFrame(singer->mLastFrameMicEnergy, 1.0f);
        }
        arrow->unk18c = false;
    }
}

float VocalTrack::GetHarmonyScore(int singerIdx) {
    Singer *singer = mPlayer->mSingers[singerIdx];
    int numParts = mPlayer->mVocalParts.size();
    float frameScore = singer->mFrameBestHitScore;
    float harmonyScore = 0.0f;
    if (harmonyScore == frameScore)
        return harmonyScore;
    for (int part = 0; part < numParts; part++) {
        if (part != singer->mFrameAssignedPart) {
            Singer *candidate = mPlayer->mVocalParts[part]->GetBestSingerCandidate();
            if (candidate) {
                float tmp = frameScore * candidate->mFrameBestHitScore;
                tmp *= frameScore;
                tmp *= 1.2f;
                harmonyScore += Clamp<float>(0.0f, 1.0f, tmp);
            }
        }
    }
    return harmonyScore;
}

void VocalTrack::UpdateUnusedArrows() {
    for (int i = 0; i < 3; i++) {
        PitchArrow *arrow = mDir->GetPitchArrow(i);
        if (arrow && arrow->unk18c) {
            arrow->SetFrameScore(0.0f, (VocalHUDColor)-1, 0.0f);
            arrow->SetVolume(0.0f);
            arrow->unk18c = false;
        }
    }
}

void UpdateSyllableText(String &str, bool b2, bool &bref) {
    bref = false;
    if (b2 && !str.empty() && str.rindex(-1) == '-') {
        if (str.length() > 1) {
            str = str.substr(0, str.length() - 1);
            return;
        }
        str = "";
        return;
    }
    if (!str.empty() && str.rindex(-1) == '=') {
        str.rindex(-1) = '-';
        if (!b2)
            str += ' ';
    } else {
        str += ' ';
        if (b2)
            str += ' ';
        bref = true;
    }
}

void PrintLyricOneLine(const Lyric &lyric) {
    MILO_LOG("\t%3.2f\t(%6.2fms)\t", lyric.mBeginPos.x, lyric.mActiveMs);
    if (lyric.mDeployIdx > -1) {
        MILO_LOG("| ");
    }
    MILO_LOG("\"%s\"", lyric.mText.c_str());
    if (lyric.mChunkEnd) {
        MILO_LOG(" |");
    }
    MILO_LOG("\n");
}

bool VocalTrack::CheckDeploySections(
    Lyric *l1,
    float f2,
    int &i3,
    const std::vector<std::pair<float, float> > &pairs,
    bool b5,
    Lyric *l2,
    float &fref
) {
    bool ret = false;
    while (i3 < pairs.size() && pairs[i3].first < f2) {
        l1->SetAfterDeploy(i3);
        if (b5) {
            fref += mStaticDeployZoneXSize;
            if (l2)
                l2->SetChunkEnd(true);
        }
        ret = true;
        i3++;
    }
    return ret;
}

bool VocalTrack::IdenticalLyric(const VocalNote &n1, const VocalNote &n2) const {
    float f6 = Abs(n1.GetMs() - n2.GetMs());
    if (f6 == 0)
        return true;
    else if (f6 > mLyricOverlapWindowMs)
        return false;
    else if (n1.mText.length() != n2.mText.length())
        return false;
    else if (n1.mText == n2.mText)
        return true;
    else {
        String t1 = n1.mText;
        String t2 = n2.mText;
        t1.ToLower();
        t2.ToLower();
        return t1 == t2;
    }
}

// VocalNote::PlayableBy(int) const is declared in beatmatch/VocalNote.h in the
// real source; forward-declared here to avoid editing an out-of-scope header.
extern "C" bool PlayableBy__9VocalNoteCFi(const VocalNote *, int);

Lyric *VocalTrack::CreateLyric(
    const VocalNote *&note,
    const std::vector<VocalNote> &notes,
    bool b3,
    bool checkPlayable,
    bool b5
) {
    const VocalNote *firstNote = note;
    if (checkPlayable
        && !PlayableBy__9VocalNoteCFi(firstNote, mPlayer->GetSlot())) {
        return NULL;
    }
    if (mPlayer && mPlayer->GetEnabledStateAt(firstNote->mMs)) {
        return NULL;
    }
    String text(firstNote->mText);
    bool wordEnd = false;
    UpdateSyllableText(text, !IsScrolling(), wordEnd);
    Lyric *lyric = new Lyric(firstNote, b3, text, wordEnd);
    const VocalNote *cur = note + 1;
    // Retail reads the vector's finish pointer directly (`lwz r10, 0x4(notes)`).
    // Spelling it `&notes[0] + notes.size()` makes MSVC recompute
    // (finish-start)/0x34*0x34+start -- a subf/divw/mulli/add quartet emitted at
    // BOTH the loop preheader and the latch (+0x38 of code, +0x10 of frame).
    while (cur != notes.end()) {
        if (!cur->mBends)
            break;
        const VocalNote *pushed = cur;
        lyric->mVocalNotes.push_back(pushed);
        float endMs = cur->mMs + cur->mDurationMs;
        const float &maxMs = lyric->mEndMs < endMs ? endMs : lyric->mEndMs;
        lyric->mEndMs = maxMs;
        cur++;
        note = note + 1;
    }
    return lyric;
}

void TambourineGemPool::FreeOldGems(float oldTime) {
    while (!mUsedGems.empty() && mUsedGems.front()->unk0 < oldTime) {
        TambourineGem *g = mUsedGems.front();
        mFreeGems.push_back(g);
        g->unk8 = 2;
        mUsedGems.pop_front();
    }
}

void VocalTrack::BuildStaticDeployZone(
    int i1,
    const std::pair<float, float> &fpair,
    float f3,
    float &fref,
    std::deque<LyricShift> &shifts
) {
    ConfigNoteTube(false, 2, std::min(i1, 1), true, 1);
    HookupTubePlates(mNoteTube);
    float f10 = fref + mStaticDeployBufferX;
    fref = (f10 + mStaticDeployZoneXSize) - mStaticDeployMarginX;
    shifts.push_back(LyricShift(fpair.second, -fref));
    if (f3 != -1.0f) {
        float max = std::max<float>(mLyricShiftMs + fpair.second, f3);
        shifts.push_back(
            LyricShift(max, mStaticDeployMarginX - (fref + mStaticDeployBufferX))
        );
    }
    bool i6 = TheSongDB->IsInCoda(MsToTickInt(fpair.first));
    float f1;
    RndGroup *u4;
    float f2;
    if (i1 == 0) {
        f1 = mDir->mTrackBottomZ + mDir->mPitchBottomZ;
        f1 = f1 * 0.5f;
        u4 = i6 ? mDir->mLeadBREGrp : mDir->mLeadLyricScrollGroup;
        f2 = mDir->mLeadLyricHeight * 0.5f;
    } else {
        f1 = mDir->mTrackTopZ + mDir->mPitchTopZ;
        f1 = f1 * 0.5f;
        u4 = i6 ? mDir->mHarmonyBREGrp : mDir->mHarmonyLyricScrollGroup;
        f2 = mDir->mHarmLyricHeight * 0.5f;
    }
    mNoteTube->SetPointPos(0, Vector3(0, 0, f1));
    mNoteTube->SetPointPos(1, Vector3(fref - f10, 0, f1));
    mNoteTube->unk_0x30 = f2;
    mNoteTube->SetBackParent(u4);
    mNoteTube->SetXPos(f10);
    mNoteTube->CreateMeshes();
    mNoteTube->SetDeployTiming(fpair.first, fpair.second);
    mNoteTube->BakePlates();
    if (gDebugSpew)
        MILO_LOG("new final deploy section for part %d\n", i1);
}

void VocalTrack::BuildScrollingDeployZone(
    int part, const std::pair<float, float> &timing
) {
    float lastLyricX = 0.0f;
    float startMs = timing.first;
    float endMs = timing.second;
    ConfigNoteTube(false, 2, part, true, 1.0f);
    HookupTubePlates(mNoteTube);
    std::deque<LyricPlate *> &plates = part != 0 ? mLyricsHarmony : mLyricsLead;
    std::deque<LyricPlate *>::iterator it = plates.begin();
    std::deque<LyricPlate *>::iterator end = plates.end();
    for (; it != end; ++it) {
        LyricPlate *plate = *it;
        if (plate->GetBeginMs() > endMs)
            break;
        lastLyricX = plate->GetLastLyricXBeforeMS(startMs);
        if (it + 1 == end)
            break;
        if ((*(it + 1))->GetBeginMs() > endMs)
            break;
    }
    float xPos = unk78 * (startMs / unk74);
    float minX = lastLyricX + mStaticDeployBufferX;
    if (xPos < minX)
        xPos = minX;
    float height;
    float z;
    if (part == 0) {
        height = mDir->mLeadLyricHeight * 0.5f;
        z = (mDir->mPitchBottomZ + mDir->mTrackBottomZ) * 0.5f;
    } else {
        height = mDir->mHarmLyricHeight * 0.5f;
        z = (mDir->mPitchTopZ + mDir->mTrackTopZ) * 0.5f;
    }
    bool inCoda = TheSongDB->IsInCoda(MsToTickInt(startMs));
    mNoteTube->SetPointPos(0, Vector3(0, 0, z));
    mNoteTube->SetPointPos(1, Vector3(unk78 * (endMs / unk74) - xPos, 0, z));
    mNoteTube->unk_0x30 = height;
    mNoteTube->SetBackParent(inCoda ? mDir->mBREGrp : mDir->mPitchScrollGroup);
    mNoteTube->SetXPos(xPos);
    mNoteTube->CreateMeshes();
    mNoteTube->BakePlates();
}

void VocalTrack::BuildScrollingDeployZones(float ms) {
    int codaTick = TheSongDB->GetCodaStartTick();
    int numParts = std::min(2, (int)mPlayer->mVocalParts.size());
    int *deployIdx = &mNextDeployZone[0];
    for (int part = 0; part < numParts; deployIdx++, part++) {
        VocalNoteList *notes = GetVocalNoteList(part);
        while (*deployIdx < notes->mFreestyleSections.size()
               && notes->mFreestyleSections[*deployIdx].first < ms) {
            const std::pair<float, float> &section =
                notes->mFreestyleSections[*deployIdx];
            float codaMs;
            if (codaTick != -1 && section.first < (codaMs = TickToMs(codaTick))
                && codaMs < section.second) {
                std::pair<float, float> firstHalf(section.first, codaMs);
                std::pair<float, float> secondHalf(codaMs, section.second);
                BuildScrollingDeployZone(part, firstHalf);
                BuildScrollingDeployZone(part, secondHalf);
            } else {
                BuildScrollingDeployZone(part, section);
            }
            (*deployIdx)++;
        }
    }
}

void VocalTrack::PrepareNoteTubes(
    float windowDurationMs, int startNote, int &endNote, int line
) {
    static bool sDump;
    int curNote = startNote;
    VocalNoteList *notes = GetVocalNoteList(line);
    float alpha = 1.0f;
    if (!mPlayer->GetEnabledStateAt(1.0f)) {
        int dimPart = mDir->unk6c4;
        if (dimPart != -1 && dimPart != line) {
            alpha = mDir->mHiddenPartAlpha;
        }
    }
    if (curNote < endNote) {
        while (curNote < endNote) {
            VocalNote &firstNote = notes->mNotes[curNote];
            if (mPlayer && mPlayer->GetEnabledStateAt(firstNote.mMs)) {
                curNote++;
            } else {
                int phraseID = TheSongDB->GetCommonPhraseID(
                    mTrackConfig.TrackNum(), firstNote.mTick
                );
                bool spotlight = phraseID != -1;
                int combineNote = curNote + 1;
                VocalNote *cur = &firstNote;
                while (combineNote != notes->mNotes.size()) {
                    VocalNote &next = notes->mNotes[combineNote];
                    if (cur->mTick + cur->mDurationTicks - next.mTick == 0
                        && cur->mEndPitch == next.mBeginPitch
                        && (!mPlayer
                            || !mPlayer->GetEnabledStateAt(firstNote.mMs))) {
                        cur = &next;
                        combineNote++;
                    } else {
                        break;
                    }
                }
                float pitchRange = mDir->mLastMax - mDir->mLastMin;
                float zPerPitch = mDir->mPitchWindowHeight / pitchRange;
                ConfigNoteTube(
                    firstNote.mUnpitchedNote == 0,
                    combineNote - curNote + 1,
                    line,
                    false,
                    alpha
                );
                HookupTubePlates(mNoteTube);
                mNoteTube->unk_0x2C = spotlight;
                if (firstNote.mUnpitchedNote) {
                    mNoteTube->unk_0x30 = mDir->mPitchWindowHeight * 0.5f;
                } else {
                    mNoteTube->unk_0x34 = zPerPitch;
                    mNoteTube->unk_0x30 = 5.0f * zPerPitch;
                    float lo = mDir->unk6d8;
                    int glow = (int)((pitchRange - lo)
                                     / ((mDir->unk6dc - lo) * 0.25f));
                    int level;
                    if (glow > 3) {
                        level = 3;
                    } else {
                        level = glow & ~(glow >> 31);
                    }
                    mNoteTube->SetGlowLevel(level);
                }
                if (sDump) {
                    TheDebug << MakeString(
                        "LINE %d NOTE %d TIME %.2f PITCHES ",
                        line,
                        curNote,
                        firstNote.mMs / 1000.0f
                    );
                }
                float runX = 0;
                float prevX = -FLT_MAX;
                int pointIdx = 0;
                if (curNote != combineNote) {
                    while (curNote != combineNote) {
                        VocalNote &note = notes->mNotes[curNote];
                        bool unpitched = note.mUnpitchedNote;
                        int beginPitch = note.mBeginPitch;
                        float z = (mDir->mPitchTopZ + mDir->mPitchBottomZ) * 0.5f;
                        if (!unpitched) {
                            z = (float)(beginPitch - 60) * zPerPitch;
                        }
                        if (sDump) {
                            if (unpitched) {
                                TheDebug << MakeString("UNPITCHED ");
                            }
                            TheDebug << MakeString(
                                "tube pitch to z: %d -> %1.2f\n", beginPitch, z
                            );
                        }
                        float x = unk78 * (runX / windowDurationMs);
                        if (note.mUnpitchedNote == 0 && pointIdx == 0) {
                            x += 0.75f * zPerPitch;
                        }
                        float minX = 0.01f + prevX;
                        if (minX >= x)
                            x = minX;
                        mNoteTube->SetPointPos(pointIdx, Vector3(x, 0, z));
                        prevX = x;
                        pointIdx++;
                        runX += note.mDurationMs;
                        curNote++;
                    }
                }
                VocalNote &lastNote = notes->mNotes[curNote - 1];
                float lastZ = (mDir->mPitchTopZ + mDir->mPitchBottomZ) * 0.5f;
                if (!lastNote.mUnpitchedNote) {
                    lastZ = zPerPitch * (float)(lastNote.mEndPitch - 60);
                }
                float lastX = unk78 * (runX / windowDurationMs);
                if (!lastNote.mUnpitchedNote) {
                    float minX = (0.75f * zPerPitch - lastX);
                    minX = -minX;
                    float prevMin = 0.01f + prevX;
                    if (prevMin >= minX)
                        minX = prevMin;
                    lastX = minX;
                }
                mNoteTube->SetPointPos(pointIdx, Vector3(lastX, 0, lastZ));
                mNoteTube->mXPos = unk78 * (firstNote.mMs / windowDurationMs);
                mNoteTube->CreateMeshes();
            }
        }
        endNote = curNote;
    }
}

void VocalTrack::ProcessStaticLyrics(
    bool b1,
    Lyric *l2,
    float &f3,
    float &f4,
    Lyric *&l5,
    Lyric *&l6,
    float &f7,
    bool b8,
    LyricPlate *lp9
) {
    if (b1) {
        if (b8) {
            float v = f4;
            f3 = v;
            f7 = v;
            l5 = nullptr;
            l6 = nullptr;
        }
        float width = mDir->unk42c - mDir->mNowBarX;
        float halfWidth = 0.5f * width;
        f4 += lp9->EstimateLyricWidth(l2);
        float d2 = f4 - f3;
        if (l5 && !l6)
            l6 = l2;
        if (l5 && d2 > width) {
            l5->SetChunkEnd(true);
            l5 = nullptr;
            f3 = f7;
            d2 = f4 - f3;
            l6->SetAfterMidPhraseLyricShift(true);
            l6 = nullptr;
        }
        if (!l5 && d2 > halfWidth && l2->mWordEnd) {
            l5 = l2;
            f7 = f4;
        }
    }
}

void VocalTrack::Restart(VocalPlayer *player, float f1, float f2) {
    unk2a4 = -1.0f;
    mPlayer = player;
    mPhraseStartMs = 0;
    mPhraseEndMs = 0;
    mNextPhraseEndMs = 0;
    for (int i = 0; i < 3; i++)
        mNextScrollNote[i] = 0;
    for (int i = 0; i < 2; i++)
        mNextDeployZone[i] = 0;
    for (int i = 0; i < 2; i++)
        mCurLyricPhrase[i] = 0;
    unk108 = 0;
    unk104 = 1;
    unk100 = 0;
    unkf4 = 0;
    unkf8 = 0;
    unkfc = 0;
    mLeadLyricShifts.clear();
    mHarmonyLyricShifts.clear();
    unk23c = mStaticDeployMarginX;
    unk240 = unk23c;
    mDir->mLeadLyricScroller->DirtyLocalXfm().v.x = unk23c;
    mDir->mHarmonyLyricScroller->DirtyLocalXfm().v.x = unk240;
    unk2ac = unk23c;
    unk2b0 = unk240;
    mTambourineGemPool->FreeUsedGems();
    mTambourineGemPool->SetTambourineManager(&mPlayer->mTambourineManager);
    mDir->mBREGrp->SetShowing(true);
    mDir->mLeadBREGrp->SetShowing(true);
    mDir->mHarmonyBREGrp->SetShowing(true);
    unk2ec = true;
    UpdateVocalStyle();
}

void VocalTrack::HitTambourineGem(int id) {
    std::deque<TambourineGem *> &gems = mTambourineGemPool->mUsedGems;
    for (int i = 0; i != gems.size(); i++) {
        if (id == gems[i]->unk4) {
            gems[i]->unk8 = 1;
            gems[i];
            break;
        }
    }
    mDir->Tambourine(hit);
}

void VocalTrack::MissTambourineGem(int, bool b) {
    if (b) {
        static Symbol miss("miss");
        mDir->Tambourine(miss);
    }
}

void VocalTrack::OnPhraseComplete(float f1, float f2, int i3) {
    BuildPhrase(f1, f2);
    if (unk68) {
        const char *txt = MakeString("last: %i\n", i3);
        mDir->Find<RndText>("debug_score_current.txt", true)->SetText(txt);
    }
}

void VocalTrack::ClearLyrics() {
    if (sDumpLyricPlates) {
        MILO_WARN("clearing all lyric plates\n");
        DumpLyricPlates(mLyricsLead, true);
        DumpLyricPlates(mLyricsHarmony, false);
    }
    while (mLyricsLead.size() != 0) {
        RELEASE(mLyricsLead.front());
        mLyricsLead.pop_front();
    }
    while (mLyricsHarmony.size() != 0) {
        RELEASE(mLyricsHarmony.front());
        mLyricsHarmony.pop_front();
    }
}

void VocalTrack::BuildPhrase(float f1, float f2) {
    mPhraseStartMs = mPhraseEndMs;
    mPhraseEndMs = f1;
    mNextPhraseEndMs = f2;
}

void VocalTrack::PushGameplayOptions(VocalParam p, int id) {
    Track::PushGameplayOptions(p, id);
    mCharOptParam = p;
    mCharOptMicID = id;
}

int VocalTrack::IncrementVolume(int val) {
    // Retail lowers this as an if / else-if chain, NOT a switch: the three mic-gain
    // params are tested with three separate `cmpwi`+`beq` (2, 3, 4) instead of the
    // range compare (`cmpwi 4; ble`) that a switch's contiguous case labels produce.
    if (mCharOptParam == kVocalParamMic1Gain || mCharOptParam == kVocalParamMic2Gain
        || mCharOptParam == kVocalParamMic3Gain) {
        MILO_ASSERT(mCharOptMicID != -1, 0xE0F);
        if (val != 0) {
            TheProfileMgr.SetMicVol(
                mCharOptMicID, val + TheProfileMgr.GetMicVol(mCharOptMicID)
            );
            TheProfileMgr.UpdateMicLevels(mCharOptMicID);
        }
        return TheProfileMgr.GetMicVol(mCharOptMicID);
    } else if (mCharOptParam == kVocalParamMicVolume) {
        if (val != 0) {
            TheProfileMgr.SetVocalCueVolume(val + TheProfileMgr.GetVocalCueVolume());
        }
        return TheProfileMgr.GetVocalCueVolume();
    } else if (mCharOptParam == kVocalParamCueVolume) {
        if (val == 1) {
            TheProfileMgr.SetSynapseEnabled(true);
        } else if (val == -1) {
            TheProfileMgr.SetSynapseEnabled(false);
        }
        return TheProfileMgr.GetSynapseEnabled();
    } else {
        MILO_NOTIFY_ONCE(
            "trying to increment unimplemented vocal param %d", mCharOptParam
        );
        return 0;
    }
}

DataNode VocalTrack::OnGetDisplayMode(const DataArray *a) {
    if (IsScrolling()) {
        return "scrolling";
    } else
        return "static";
}

DataNode VocalTrack::OnSetDisplayMode(const DataArray *a) {
    if (a->Sym(2) == "static") {
        mVocalStyleOverride = kVocalStyleStatic;
        return a->Node(2);
    } else if (a->Sym(2) == "scrolling") {
        mVocalStyleOverride = kVocalStyleScrolling;
        return a->Node(2);
    } else
        return "unrecognized";
}

void VocalTrack::SetCanDeploy(bool can) {
    if (mDir->mPitchScrollGroup) {
        mDir->mPitchScrollGroup->SetShowing(can);
    }
    if (mDir->mLeadLyricScrollGroup) {
        mDir->mLeadLyricScrollGroup->SetShowing(can);
    }
    if (mDir->mHarmonyLyricScrollGroup) {
        mDir->mHarmonyLyricScrollGroup->SetShowing(can);
    }
}

int VocalTrack::GetNumVocalParts() {
    if (mPlayer)
        return mPlayer->NumVocalParts();
    else {
        MILO_NOTIFY_ONCE("invalid vocal player");
        return 0;
    }
}

// Retail VocalTrack::Handle is timer-OFF (frame 0xc0, no MessageTimer): the diff
// shows retail never constructs the Timer/Restart/sActive machinery here, so the
// default timer-off BEGIN_HANDLERS (ObjMacros.h) is the matching form. A prior
// override re-added the timer under the mistaken belief the fn_82B71AA4 String-dtor
// funclet needed the timer frame; the funclet in fact matches the timer-off frame.
BEGIN_HANDLERS(VocalTrack)
    HANDLE_ACTION(initialize, Init())
    HANDLE(set_display_mode, OnSetDisplayMode)
    HANDLE(display_mode, OnGetDisplayMode)
    HANDLE_ACTION(dump_plates, DumpAllPlates())
    HANDLE_EXPR(set_verbose_plates, sDumpPlateStates = _msg->Int(2))
    HANDLE_ACTION(reset_timing_data, ResetTimingData())
    HANDLE_SUPERCLASS(Track)
    HANDLE_CHECK(0xE6C)
END_HANDLERS

bool Performer::IsNet() const { return false; }

bool Player::InTambourinePhrase() const { return false; }

bool VocalPlayer::InTambourinePhrase() const { return mTambourineManager.unk60 > 0; }

VocalTrack::LyricShift::LyricShift(float f1, float f2) : unk0(f2), unk4(f1), unk8(0) {
    if (dumpLyricShifts) {
        MILO_LOG(
            "New LyricShift begin %.2f sec, end %.2f x, fast: %d\n",
            f1 / 1000.0f,
            f2,
            false
        );
    }
}

VocalTrack::LyricShift::LyricShift(float f1, float f2, bool fast)
    : unk0(f2), unk4(f1), unk8(fast) {
    if (dumpLyricShifts) {
        MILO_LOG(
            "New LyricShift begin %.2f sec, end %.2f x, fast: %d\n", f1 / 1000.0f, f2, unk8
        );
    }
}

// sw2 scatter-include (default/VocalTrack <- bandobj/BandWardrobe.cpp)
#define gRev gRev_BandWardrobe
#define gAltRev gAltRev_BandWardrobe
#include "bandobj/BandWardrobe.cpp"
#undef gRev
#undef gAltRev
