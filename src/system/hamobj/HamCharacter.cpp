#include "hamobj/HamCharacter.h"
#include "HamCharacter.h"
#include "utl/Str.h"
#include "HamRegulate.h"
#include "char/CharClip.h"
#include "char/CharEyes.h"
#include "char/CharFaceServo.h"
#include "char/CharLipSync.h"
#include "char/CharLipSyncDriver.h"
#include "char/CharServoBone.h"
#include "char/CharWeightable.h"
#include "char/Character.h"
#include "char/FileMerger.h"
#include "char/Waypoint.h"
#include "hamobj/HamDriver.h"
#include "hamobj/HamGameData.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "obj/Data.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Utl.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/Draw.h"
#include "rndobj/TexBlender.h"
#include "rndobj/Trans.h"
#include "synth/Sound.h"
#include "synth/Synth.h"
#include "obj/Task.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/Symbol.h"

#ifdef HX_NATIVE
bool HamCharacter::sLoadVO;
CharClip *HamCharacter::sSkeletonClips[HamCharacter::kNumSkeletons];
#endif

extern "C" char *_strlwr(char *);

namespace {
    const char *kCrewCardMeshName = "crew_card.mesh";
}

String mCampaignVO;

HamCharacter::HamCharacter()
    : mCampaignVOBank(0), mCampaignVODir(0), mFileMerger(0), mIsCampaignChar(0), mShowBox(0),
      mNeedsAcquirePose(1), mEyes(this), mGender(kHamFemale), mAnimationState(0), mPollWhenHidden(0),
      mTexBlendersActive(1), mIKEffectors(this), mBaseLipsyncOffset(0), mNeutralSkelDir(0),
      mSkeletonBones(0), mCrewCardMesh(nullptr), mUseCameraSkeleton(0) {
    mWaypoint = Hmx::Object::New<Waypoint>();
    mWaypoint->SetAngRadius(0);
    mWaypoint->SetRadius(36);
    mWaypoint->SetYRadius(36);
    mWaypoint->SetStrictRadiusDelta(0.01);
    const char *path = "";
    DataArray *cfg = SystemConfig("objects", "HamCharacter");
    if (cfg->FindData("skeleton_path", path, false) && *path != '\0') {
        FilePathTracker tracker(path);
        mNeutralSkelDir =
            DirLoader::LoadObjects("neutral_skeleton.milo", nullptr, nullptr);
        MILO_ASSERT(mNeutralSkelDir, 0x7D);
        mSkeletonBones =
            mNeutralSkelDir->Find<CharServoBone>("skeleton_bones.servo", true);
        MILO_ASSERT(mSkeletonBones, 0x7F);
    }
}

HamCharacter::~HamCharacter() {
    if (TheSynth) {
        TheSynth->RemovePlayHandler(this);
    }
#ifdef HX_NATIVE
    if (!ObjectDir::InDeleteObjects())
#endif
    delete mWaypoint;
}

BEGIN_HANDLERS(HamCharacter)
    HANDLE(configure_file_merger, OnConfigureFileMerger)
    HANDLE_ACTION(start_load, StartLoad(_msg->Int(2)))
    HANDLE(cam_teleport, OnCamTeleport)
    HANDLE(post_delete, OnPostDelete)
    HANDLE_ACTION(set_lipsync_offset, SetLipsyncOffset(_msg->Float(2)))
    HANDLE(sound_play, OnSoundPlay)
    HANDLE_ACTION(
        enable_facial_animation,
        EnableFacialAnimation(_msg->Obj<CharLipSync>(2), _msg->Float(3))
    )
    HANDLE_ACTION(set_blinking, SetBlinking(_msg->Int(2)))
    HANDLE_EXPR(crew_card_found, Find<RndMesh>(kCrewCardMeshName, false))
    HANDLE_ACTION(set_campaign_vo, SetCampaignVo(_msg->Str(2)))
    HANDLE_EXPR(get_campaign_vo_bank, mCampaignVOBank)
    HANDLE(toggle_interests_overlay, OnToggleInterestDebugOverlay)
    HANDLE_SUPERCLASS(Character)
END_HANDLERS

BEGIN_PROPSYNCS(HamCharacter)
    SYNC_PROP(outfit, mOutfit)
    SYNC_PROP(outfit_dir, mOutfitDir)
    SYNC_PROP(show_box, mShowBox)
    SYNC_PROP(gender, (int &)mGender)
    SYNC_PROP_SET(force_blink, true, if (_val.Int()) ForceBlink())
    SYNC_PROP_SET(enable_auto_blinks, true, EnableBlinks(_val.Int(), false))
    SYNC_PROP_SET(
        force_lookat,
        mEyes->GetCurrentInterest() ? Symbol(mEyes->GetCurrentInterest()->Name())
                                    : Symbol(),
        SetFocusInterest(_val.Sym(), 0)
    )
    SYNC_PROP(poll_when_hidden, mPollWhenHidden)
    SYNC_PROP_MODIFY(
        tex_blenders_active, mTexBlendersActive, SetTexBlendersActive(mTexBlendersActive)
    )
    SYNC_PROP_SET(
        crew_card_showing,
        mCrewCardMesh ? mCrewCardMesh->Showing() : false,
        if (mCrewCardMesh) mCrewCardMesh->SetShowing(_val.Int())
    )
    SYNC_PROP_SET(prop_0_showing, GetPropShowing(0), SetPropShowing(0, _val.Int()))
    SYNC_PROP_SET(prop_1_showing, GetPropShowing(1), SetPropShowing(1, _val.Int()))
    SYNC_PROP_SET(prop_2_showing, GetPropShowing(2), SetPropShowing(2, _val.Int()))
    SYNC_PROP_SET(prop_3_showing, GetPropShowing(3), SetPropShowing(3, _val.Int()))
    SYNC_SUPERCLASS(Character)
END_PROPSYNCS

BEGIN_SAVES(HamCharacter)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Character)
    bs << mOutfit;
    bs << mOutfitDir;
    bs << mShowBox;
    bs << mPollWhenHidden;
    bs << mTexBlendersActive;
END_SAVES

BEGIN_COPYS(HamCharacter)
    COPY_SUPERCLASS(Character)
    CREATE_COPY(HamCharacter)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mOutfit)
        COPY_MEMBER(mShowBox)
        COPY_MEMBER(mOutfitDir)
        COPY_MEMBER(mGender)
        COPY_MEMBER(mPollWhenHidden)
        COPY_MEMBER(mTexBlendersActive)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(HamCharacter)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

INIT_REVS(3, 0)

void HamCharacter::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    Character::PreLoad(bs);
    Reserve((mHashTable.UsedSize() + 20) * 2, mStringTable.UsedSize() + 0x1B8);
    bs.PushRev(packRevs(d.altRev, d.rev), this);
}

void HamCharacter::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
    Character::PostLoad(bs);
    if (gLoadingProxyFromDisk) {
        Symbol s;
        d >> s;
    } else {
        d >> mOutfit;
    }
    if (d.rev > 0) {
        d >> mOutfitDir;
    }
    if (d.rev > 1) {
        d >> mShowBox;
    }
    if (d.rev > 2) {
        d >> mPollWhenHidden;
        bool active;
        d >> active;
        SetTexBlendersActive(active);
    }
}

void HamCharacter::SyncObjects() {
    const char *meshes[2] = { "bone_pelvis.mesh", "spot_neck.mesh" };
    for (int i = 0; i < 2; i++) {
        RndTransformable *t = Find<RndTransformable>(meshes[i], false);
        if (t) {
            t->SetTransParent(this, false);
        }
    }
    if (mNeedsAcquirePose && BoneServo()) {
        mNeedsAcquirePose = false;
        BoneServo()->AcquirePose();
    }
    SetTexBlendersActive(mTexBlendersActive);
    Character::SyncObjects();
    if (Find<CharLipSyncDriver>("face.lipdrv", false)) {
        CharFaceServo *servo = Find<CharFaceServo>("face.faceservo", false);
        CharLipSyncDriver *lipDrv = Find<CharLipSyncDriver>("face.lipdrv", false);
        EnableFacialAnimation(lipDrv->LipSync(), 0);
        bool blinking = servo
            && (!servo->BlinkClipLeftName().Null()
                && !servo->BlinkClipRightName().Null());
        SetBlinking(blinking);
    }
    mCrewCardMesh = Find<RndMesh>(kCrewCardMeshName, false);
}

void HamCharacter::Draw() {
    if (!mShowing && !mTexBlendersActive) {
        for (ObjDirItr<RndTexBlender> it(this, true); it != nullptr; ++it) {
            it->DrawShowing();
        }
    }
    RndDrawable::Draw();
}

void HamCharacter::DrawShowing() {
    Character::DrawShowing();
    if (mShowBox) {
        mWaypoint->Highlight();
    }
}

void HamCharacter::Enter() {
    Character::Enter();
    if (BoneServo()) {
        BoneServo()->SetRegulateWaypoint(nullptr);
    }
    if (Regulator()) {
        Regulator()->SetWaypoint(nullptr);
    }
    mAnimationState = 0;
    TheSynth->AddPlayHandler(this);
}

void HamCharacter::Exit() {
    if (TheSynth) {
        TheSynth->RemovePlayHandler(this);
    }
    Character::Exit();
}

void HamCharacter::AddedObject(Hmx::Object *obj) {
    Character::AddedObject(obj);
    static Symbol HamIKEffector("HamIKEffector");
    Symbol className = obj->ClassName();
    if (streq(obj->Name(), "char.fm")) {
        mFileMerger = dynamic_cast<FileMerger *>(obj);
    } else if (streq(obj->Name(), "CharEyes.eyes")) {
        mEyes = dynamic_cast<CharEyes *>(obj);
    } else if (className == HamIKEffector) {
        mIKEffectors.push_back(dynamic_cast<CharWeightable *>(obj));
    }
}

void HamCharacter::RemovingObject(Hmx::Object *obj) {
    Character::RemovingObject(obj);
    if (obj == mFileMerger) {
        mFileMerger = nullptr;
    }
}

void HamCharacter::Init() {
    REGISTER_OBJ_FACTORY(HamCharacter);
    TheDebug.AddExitCallback(HamCharacter::Terminate);
    const char *path = "";
    DataArray *cfg = SystemConfig("objects", "HamCharacter");
    static Symbol CHARCLIP_SKELETONS("CHARCLIP_SKELETONS");
    DataArray *macro = DataGetMacro(CHARCLIP_SKELETONS);
    if (macro) {
        if (cfg->FindData("skeleton_path", path, false) && *path != '\0') {
            FilePathTracker tracker(path);
            int numSkels = macro->Size();
            MILO_ASSERT(numSkels == kNumSkeletons, 0x42);
            ObjectDir *clips =
                DirLoader::LoadObjects("skeleton_clips.milo", nullptr, nullptr);
            MILO_ASSERT(clips, 0x45);
#ifdef HX_NATIVE
            if (!clips) return;
#endif
            for (int i = 0; i < numSkels; i++) {
                Symbol s = macro->Sym(i);
                sSkeletonClips[i] =
                    clips->Find<CharClip>(MakeString("%s_skeleton", s), true);
                MILO_ASSERT(sSkeletonClips[i], 0x4C);
            }
        }
    }
}

void HamCharacter::Terminate() {
    for (int i = 0; i < kNumSkeletons; i++) {
        delete sSkeletonClips[i];
    }
}

String HamCharacter::GetCampaignVo() { return mCampaignVO; }

void HamCharacter::StartLoad(bool start) {
    if (!mFileMerger->StartLoad(start)) {
        SyncObjects();
    }
}

void HamCharacter::SetOutfit(Symbol outfit) { mOutfit = outfit; }
void HamCharacter::SetOutfitDir(Symbol outfitDir) { mOutfitDir = outfitDir; }

void HamCharacter::UnloadAll() {
    if (mFileMerger)
        mFileMerger->Clear();
}

String HamCharacter::GetCampaignVoMilo() {
    return MakeString("sfx/loc/eng/campaign/%s.milo", mCampaignVO);
}

void HamCharacter::SetTexBlendersActive(bool active) {
    mTexBlendersActive = active;
    for (ObjDirItr<RndTexBlender> it(this, true); it != nullptr; ++it) {
        it->SetShowing(active);
    }
}

bool HamCharacter::InClipTest() {
    if (TheLoadMgr.EditMode() && streq(Dir()->Name(), "clip_test")) {
        return true;
    } else
        return false;
}

void HamCharacter::SetIKEffectorWeights(float weight) {
    FOREACH (it, mIKEffectors) {
        CharWeightable *cw = *it;
        if (cw) {
            cw->SetWeight(weight);
        }
    }
}

void HamCharacter::ResyncLipSync(CharLipSync *sync) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (driver) {
        if (sync) {
            driver->SetLipSync(sync);
        }
        driver->Sync();
    }
}

void HamCharacter::PlayBaseViseme() {
    ObjectDir *visemeDir = Find<ObjectDir>("viseme", false);
    if (visemeDir) {
        CharFaceServo *servo = Find<CharFaceServo>("face.faceservo", false);
        if (servo) {
            servo->SetClips(visemeDir);
            CharClip *clip = servo->BaseClip();
            if (clip) {
                clip->PoseMeshes(this, clip->StartBeat());
            }
        }
    }
}

void HamCharacter::EnableFacialAnimation(CharLipSync *sync, float f2) {
    MILO_LOG(
        "HamCharacter::EnableFacialAnimation Name:%s lipsync name:%s\n",
        Name(),
        SafeName(sync)
    );
    ObjectDir *visemeDir = Find<ObjectDir>("viseme", false);
    if (visemeDir && !visemeDir->Find<CharClip>("Base", false)) {
        return;
    }
    mBaseLipsyncOffset = f2;
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (sync && driver) {
        if (!driver->SetLipSync(sync)) {
            driver->Sync();
        }
        driver->SetSongOffset(f2);
        String str(FileGetBase(sync->Name()));
        str += ".anim";
        RndAnimatable *anim = sync->Dir()->Find<RndAnimatable>(str.c_str(), false);
        if (anim) {
            static Symbol animate("animate");
            anim->Handle(Message(animate), true);
        }
    }
}

void HamCharacter::DisableFacialAnimation() {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (driver) {
        driver->SetLipSync(nullptr);
        driver->Sync();
    }
}

void HamCharacter::ResetFacialAnimation() {
    MILO_LOG("HamCharacter::ResetFacialAnimation() Name:%s\n", Name());
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (driver) {
        driver->ClearLipSync();
    }
}

void HamCharacter::SetBlinking(bool blinking) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    ObjectDir *visemeDir = Find<ObjectDir>("viseme", false);
    if (visemeDir) {
        CharFaceServo *servo = Find<CharFaceServo>("face.faceservo", false);
        if (servo) {
            servo->SetBlinkClipLeft(blinking ? "Blink" : "");
            servo->SetBlinkClipRight(blinking ? "Blink" : "");
            servo->SetClips(visemeDir);
        }
        if (driver && visemeDir->Find<CharClip>("Base", false)) {
            driver->SetClips(visemeDir);
        }
    }
}

void HamCharacter::BlendInFaceOverrides(float f1) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (driver) {
        driver->BlendInOverrides(f1);
    }
}

void HamCharacter::BlendOutFaceOverrides(float f1) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (driver) {
        driver->BlendOutOverrides(f1);
    }
}

void HamCharacter::SetLipsyncOffset(float offset) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (driver) {
        driver->SetSongOffset(mBaseLipsyncOffset + offset);
    }
}

void HamCharacter::SetFaceOverrideWeight(float weight) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (driver) {
        driver->SetOverrideWeight(weight);
    }
}

float HamCharacter::GetFaceOverrideWeight() {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    return driver ? driver->GetOverrideWeight() : 0;
}

void HamCharacter::SetUseCameraSkeleton(bool use) {
    mUseCameraSkeleton = use;
    if (mUseCameraSkeleton) {
        SetIKEffectorWeights(0);
    } else {
        SetIKEffectorWeights(1);
    }
}

Symbol HamCharacter::GetFaceOverrideClip() {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (driver && driver->OverrideClip()) {
        return driver->OverrideClip()->Name();
    } else
        return Symbol();
}

void HamCharacter::ResetFaceOverrideBlending() {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    if (driver) {
        driver->ResetOverrideBlend();
    }
}

void HamCharacter::SetCampaignVo(const char *cc) {
    mCampaignVO = cc;
    auto& _ref1 = mCampaignVOBank;
    RELEASE(_ref1);
    int hasVO = !mCampaignVO.empty();
    if (hasVO) {
        String milo = GetCampaignVoMilo();
        mCampaignVODir = DirLoader::LoadObjects(milo.c_str(), 0, 0);
        for (ObjDirItr<Hmx::Object> it(mCampaignVODir, false); it != nullptr; ++it) {
            if (it->Type() == "character_vo") {
                _ref1 = it;
                return;
            }
        }
    }
}

bool HamCharacter::IsLoading() {
    if (mFileMerger) {
        return mFileMerger->HasPendingFiles();
    } else
        return false;
}

HamRegulate *HamCharacter::Regulator() { return Find<HamRegulate>("song.hreg", false); }
HamDriver *HamCharacter::SongDriver() { return Find<HamDriver>("song.hdrv", false); }

int HamCharacter::SongAnimation() {
    CharClip *c = nullptr;
    if (Driver()) {
        c = Driver()->FirstClip();
#ifdef HX_NATIVE
        // On native, Driver() exists but may have no clips yet (PlayAnims
        // hasn't run). Fall through to SongDriver check so PlayAnims can fire.
        if (!c) { /* fall through */ }
        else
#endif
        MILO_ASSERT(c->Type() == "main", 0x3AB);
    }
    if (InClipTest() && (c && c->Dir()->Dir() != this)) {
        return c->Property("clip_skeleton_index", false)->Int();
    } else if (mUseCameraSkeleton || c) {
        return -1;
    } else if (SongDriver()) {
        c = SongDriver()->FirstClip();
        if (c) {
            MILO_ASSERT(c->Type() == "main", 0x3C8);
            return c->Property("clip_skeleton_index", false)->Int();
        }
    }
    return 0;
}

bool HamCharacter::GetPropShowing(int prop) {
    RndDrawable *d;
    auto _tmp0 = mShowableProps.size();
    return _tmp0 > prop && (d = mShowableProps[prop]) && d->Showing();
}

void HamCharacter::SetPropShowing(int prop, bool show) {
    if (mShowableProps.size() > prop) {
        if (mShowableProps[prop])
            mShowableProps[prop]->SetShowing(show);
    }
}

DataNode HamCharacter::OnConfigureFileMerger(DataArray *a) {
    FilePathTracker tracker(FileRoot());
    if (!mFileMerger) {
        return 0;
    } else {
        mNeedsAcquirePose = true;
        FilePath outfitPath = "";
        FilePath visemePath = "";
        FilePath voPath = "";
        mIsCampaignChar = !strstr(mOutfitDir.Str(), "dancer");
        if (!mOutfit.Null()) {
            const char *model = GetOutfitModel(mOutfit);
            outfitPath.Set(FilePath::Root().c_str(), model);
            Symbol charSym = GetOutfitCharacter(mOutfit);
            const char *viseme = GetCharacterViseme(charSym);
            visemePath.Set(FilePath::Root().c_str(), viseme);
            if (!mIsCampaignChar) {
                String vo = GetCampaignVo();
                if (!vo.empty()) {
                    voPath.Set(FilePath::Root().c_str(), GetCampaignVoMilo().c_str());
                } else {
                    voPath.Set(FilePath::Root().c_str(), "sfx/lipsynchelper.milo");
                }
                const char *localized = FileLocalize(voPath.c_str(), nullptr);
                voPath.Set(FilePath::Root().c_str(), localized);
            }
        }
        mFileMerger->Select("outfit", outfitPath, false);
        FileMerger::Merger *merger = mFileMerger->FindMerger("vo_bank", false);
        if (merger) {
            if (sLoadVO) {
                merger->SetSelected(voPath, false);
            } else {
                merger->SetSelected(gNullStr, false);
            }
        }
        FileMerger::Merger *visemeMerger = mFileMerger->FindMerger("viseme", false);
        if (visemeMerger) {
            visemeMerger->SetSelected(visemePath, false);
        }
        return 0;
    }
}

DataNode HamCharacter::OnPostDelete(DataArray *a) {
    Symbol s = a->Sym(2);
    if (s == "outfit") {
        ObjectDir *clipDir = Find<ObjectDir>("clips", false);
        if (clipDir) {
            mDriver->SetClips(clipDir);
        }
    }
    return 0;
}

DataNode HamCharacter::OnToggleInterestDebugOverlay(DataArray *a) {
    if (mEyes) {
        mEyes->ToggleInterestsDebugOverlay();
    }
    return 0;
}

DataNode HamCharacter::OnCamTeleport(DataArray *a) {
    mWaypoint->DirtyLocalXfm() = LocalXfm();
    if (Find<HamRegulate>("song.hreg", false)) {
        Find<HamRegulate>("song.hreg", false)->SetWaypoint(0);
    }
    return 0;
}

ObjectDir *HamCharacter::GetNeutralSkeleton() {
#ifdef HX_NATIVE
    // Skeleton blending requires mSkeletonBones (set via skeleton_path config).
    // If not configured, skip computation and return the dir/self fallback.
    if (!mSkeletonBones) {
        return mNeutralSkelDir ? mNeutralSkelDir : this;
    }
#endif
    int songAnim = SongAnimation();
#ifdef HX_NATIVE
    // On native, compute bones once up front. The goto zero_and_scale below
    // jumps past the else-branch's local declaration, which is UB on Clang
    // (bones is uninitialized). PPC/MSVC keeps the earlier local in the same
    // register so it "works" there, but we must hoist the cast for native.
    CharBones *bones = static_cast<CharBones *>(mSkeletonBones);
#endif
    if (songAnim != -1) {
        HamDriver *hamDriver = Find<HamDriver>("song.hdrv", false);
        if (hamDriver == nullptr || hamDriver->FirstClip() == nullptr) {
            CharClip *clip = Driver()->FirstPlayingClip();
#ifndef HX_NATIVE
            CharBones *bones = reinterpret_cast<CharBones *>((char *)mSkeletonBones + 0x10);
#endif
            if (clip == nullptr) {
                goto zero_and_scale;
            }
            bones->Zero();
            Driver()->SetClipWeightMap();
            std::map<CharClip *, float> clipMap(Driver()->mClipWeightMap);
            float totalWeight = 0.0f;
            for (std::map<CharClip *, float>::iterator it = clipMap.begin();
                 it != clipMap.end(); ++it) {
                CharClip *clipEntry = it->first;
                float weight = it->second;
                if (clipEntry != nullptr && weight > totalWeight) {
                    int skelIdx = clipEntry->Property("clip_skeleton_index", false)->Int(nullptr);
                    CharBones *skBones = mSkeletonBones ? static_cast<CharBones *>(mSkeletonBones) : nullptr;
                    sSkeletonClips[skelIdx]->ScaleAdd(*skBones, weight, totalWeight, totalWeight);
                }
            }
            mSkeletonBones->Poll();
        } else {
            hamDriver->SetClipWeightMap();
            std::map<CharClip *, float> clipMap(hamDriver->mClipTimingMap);
            if (clipMap.size() == 0) {
                clipMap.clear();
                return this;
            }
#ifndef HX_NATIVE
            CharBones *bones = static_cast<CharBones *>(mSkeletonBones);
#endif
            bones->Zero();
            for (std::map<CharClip *, float>::iterator it = clipMap.begin();
                 it != clipMap.end(); ++it) {
                if (it->first != nullptr) {
                    ApplyBlendedSkeletons(hamDriver, it->first, it->second);
                }
            }
            mSkeletonBones->Poll();
        }
    } else {
        CharClip *clip = Driver()->FirstClip();
        if (clip == nullptr) {
            return this;
        }
        if (clip->Flags() & 1) {
            return this;
        }
#ifndef HX_NATIVE
        CharBones *bones = reinterpret_cast<CharBones *>((char *)mSkeletonBones + 0x10);
#endif
zero_and_scale:
        bones->Zero();
        {
            CharBones *skBones = mSkeletonBones ? static_cast<CharBones *>(mSkeletonBones) : nullptr;
            sSkeletonClips[mGender == kHamFemale ? 1 : 0]->ScaleAdd(*skBones, 1.0f, 0.0f, 0.0f);
        }
        mSkeletonBones->Poll();
    }
    return mNeutralSkelDir;
}

void HamCharacter::SetFaceOverrideClip(Symbol clipName, bool notify) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    bool found = false;
    if (driver) {
        if (clipName.Null()) {
            found = true;
            driver->mOverrideClip = nullptr;
        } else {
            ObjectDir *clipDir = driver->mOverrideOptions;
            if (!clipDir) {
                clipDir = driver->mClips;
            }
            for (ObjDirItr<CharClip> it(clipDir, false); it != nullptr; ++it) {
                if (clipName == it->Name()) {
                    found = true;
                    driver->mOverrideClip = it;
                }
            }
            if (!found && notify) {
                MILO_NOTIFY(MakeString(
                    "HamCharacter::SetFaceOverrideClip couldn't find clip named %s for %s",
                    clipName.Str(),
                    Name()
                ));
                return;
            }
        }
    }
    if (found)
        return;
    if (!notify)
        return;
    MILO_NOTIFY(MakeString("HamCharacter::SetFaceOverrideClip couldnt find lip sync driver for %s", Name()));
}

void HamCharacter::BlendInFaceOverrideClip(Symbol clipName, float blendIn, float blendOut) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("face.lipdrv", false);
    bool found = false;
    if (driver) {
        if (clipName.Null()) {
            found = true;
            driver->mOverrideClip = nullptr;
        } else {
            ObjectDir *clipDir = driver->mOverrideOptions;
            if (!clipDir) {
                clipDir = driver->mClips;
            }
            for (ObjDirItr<CharClip> it(clipDir, false); it != nullptr; ++it) {
                if (Symbol(clipName) == Symbol(it->Name())) {
                    found = true;
                    driver->BlendInOverrideClip(it, blendIn, blendOut);
                }
            }
            if (!found) {
                TheDebug.Notify(MakeString(
                    "HamCharacter::SetFaceOverrideClip couldn't find clip named %s for %s",
                    clipName.Str(),
                    Name()
                ));
                return;
            }
        }
    }
    if (found)
        return;
    TheDebug.Notify(MakeString("HamCharacter::SetFaceOverrideClip couldnt find lip sync driver for %s", (char*)Name()));
}

DataNode HamCharacter::OnSoundPlay(const DataArray *a) {
    const DataNode &val = const_cast<DataArray *>(a)->Node(2).Evaluate();
    if (val.Type() == kDataObject) {
        Hmx::Object *obj = val.UncheckedObj();
        if (!obj) return DataNode(0);
        Sound *sound = dynamic_cast<Sound *>(obj);
        if (sound) {
            if (mOutfit.Str()[0] == '\0') {
#ifndef HX_NATIVE
                MILO_NOTIFY(
                    "HamCharacter::OnSoundPlay: No outfit specified for character %s. "
                    "Not going to play lipsync\n",
                    (char *)Name()
                );
#endif
                return DataNode(0);
            }
            Symbol outfitChar = GetOutfitCharacter(mOutfit, true);
            StackString<128> soundName(sound->Name());
            unsigned int pos = soundName.find(outfitChar.Str());
            if (pos != (unsigned int)-1) {
                CharLipSync *lipSync = CharLipSync::FindLipSyncForSound(sound);
                if (lipSync) {
                    TheDebug << MakeString(
                        "HamCharacter: found lipsync [%s] to play for sound [%s]\n",
                        lipSync->Name(),
                        sound->Name()
                    );
                    float seconds = TheTaskMgr.Seconds(TaskMgr::kRealTime);
                    EnableFacialAnimation(lipSync, -seconds);
                }
            }
        }
    }
    return DataNode(0);
}

#ifndef HX_NATIVE
// PPC codegen variant — body uses Set() instead of initializer list
QuatXfm::QuatXfm(const Transform &t) : v(t.v) { q.Set(t.m); }
#endif

#ifndef HX_NATIVE
void HamCharacter::Poll() {
    int songAnim = SongAnimation();
    auto& _ref0 = mDriver;
    if (songAnim == -1 || InClipTest()) {
        if (_ref0) _ref0->SetWeight(1.0f);
    } else {
        if (_ref0) _ref0->SetWeight(0.0f);
    }

    bool wasShowing = mShowing;
    if (!wasShowing && mPollWhenHidden) {
        SetShowing(true);
    }
    Character::Poll();
    SetShowing(wasShowing);

    RndTransformable *boneProp = Find<RndTransformable>("bone_prop0.mesh", false);
    if (boneProp) {
        RndTransformable *spotProp = Find<RndTransformable>("spot_prop0.mesh", false);
        if (spotProp) {
            float blendWeight = 0.0f;
            int songAnim2 = SongAnimation();
            if (songAnim2 == -1) {
                blendWeight = 1.0f;
                if (_ref0->First()) {
                    blendWeight = _ref0->EvaluateFlags(2);
                }
            }

            QuatXfm boneXfm(boneProp->WorldXfm());
            QuatXfm spotXfm(spotProp->WorldXfm());

            Vector3 interpPos;
            Interp(spotXfm.v, boneXfm.v, blendWeight, interpPos);
            Hmx::Quat interpRot;
            Interp(spotXfm.q, boneXfm.q, blendWeight, interpRot);

            Transform result;
            result.v = interpPos;
            MakeRotMatrix(interpRot, result.m);
            boneProp->SetWorldXfm(result);
        }
    }

    RndMat *mat = Find<RndMat>("robot_face.mat", false);
    if (!mat) return;

    CharLipSyncDriver *lipDrv = Find<CharLipSyncDriver>("face.lipdrv", false);
    const char *clipName = "base";
    CharLipSync::PlayBack *pb = lipDrv->GetPlayBack();
    if (pb) {
        float maxWeight = 0.0f;
        for (int i = 0; i < (int)pb->mWeights.size(); i++) {
            CharLipSync::PlayBack::Weight &w = pb->mWeights[i];
            if (!w.mClip) continue;
            float curWeight = w.mCurWeight;
            float newMax = (maxWeight >= curWeight) ? maxWeight : curWeight;
            if (newMax != maxWeight) {
                clipName = w.mClip->Name();
            }
            maxWeight = newMax;
        }
    }

    char texName[256];
    strcpy(texName, clipName);
    _strlwr(texName);
    strcat(texName, ".tex");

    RndTex *tex = Find<RndTex>(texName, false);
    if (!tex) {
        tex = Find<RndTex>("base.tex", false);
    }
    if (tex) {
        mat->SetDiffuseTex(tex);
    } else {
        MILO_NOTIFY_ONCE("%s could not find viseme texture %s", PathName(this), texName);
    }
}
#else
void HamCharacter::Poll() {
    int songAnim = SongAnimation();
    if (songAnim == -1 || InClipTest()) {
        if (mDriver) mDriver->SetWeight(1.0f);
    } else {
        if (mDriver) mDriver->SetWeight(0.0f);
    }

    // On native, always force Showing(true) during poll so RndDir::Poll()
    // runs child pollables (CharDriver, etc.) and animations advance.
    // On Xbox, DTA scripts manage visibility; on native we skip that flow.
    bool wasShowing = mShowing;
    if (!wasShowing) {
        SetShowing(true);
    }
    Character::Poll();
    SetShowing(wasShowing);
}
#endif

void HamCharacter::ApplyBlendedSkeletons(
    HamDriver *driver, CharClip *clip, float weight
) {
    if (clip->NumBlendSamples() != 0) {
        std::list<HamDriver::Layer *> &layers = driver->Layers().mLayers;
        for (std::list<HamDriver::Layer *>::iterator it = layers.begin();
             it != layers.end();
             ++it) {
            HamDriver::LayerClip *layerClip;
            if ((*it)->FirstClip() == clip && (*it)->mWeight == weight
                && (layerClip = dynamic_cast<HamDriver::LayerClip *>(*it))
                       != nullptr) {
                float beat = (TheTaskMgr.Beat() - layerClip->mClipBeat)
                    + clip->StartBeat();
                CharBones *bones =
                    mSkeletonBones
                        ? static_cast<CharBones *>(mSkeletonBones)
                        : nullptr;
                clip->ApplyBlendedSkeletons(
                    sSkeletonClips, *bones, beat, weight
                );
                return;
            }
        }
    }
    int skelIndex = clip->Property("clip_skeleton_index", false)->Int();
    CharBones *bones =
        mSkeletonBones ? static_cast<CharBones *>(mSkeletonBones) : nullptr;
    sSkeletonClips[skelIndex]->ScaleAdd(*bones, weight, 0.0f, 0.0f);
}

template class StackString<128>;
