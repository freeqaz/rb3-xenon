#include "bandobj/BandCharacter.h"
#include "obj/ObjMacros.h"
#include "decomp.h"
#include <cstdlib>
#ifdef HX_NATIVE
#include <cstdio>
#include <cmath>
#include <vector>
#include <list>
#include <algorithm>
#include "rndobj/Mesh.h"
#include "rndobj/Dir.h"
#endif
#include "bandobj/BandHeadShaper.h"
#include "bandobj/BandWardrobe.h"
#include "char/CharCollide.h"
#include "char/CharServoBone.h"
#include "char/CharClipDriver.h"
#include "char/CharClipGroup.h"
#include "char/CharFaceServo.h"
#include "char/CharInterest.h"
#include "char/CharMeshCacheMgr.h"
#include "char/CharUtl.h"
#include "math/Rand.h"
#include "math/Rot.h"
#include "obj/Task.h"
#include "obj/Utl.h"
#include "utl/Loader.h"
#include "rndobj/Cam.h"
#include "rndobj/Env.h"
#include "rndobj/Utl.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"

// Retail folded this TU's COMDATs into BandCharacter's .text span
// (BandTerminate lands at 0x8227AFC0, inside 0x8227AFA0-0x8227C6D0).
#include "bandobj/Band.cpp"

INIT_REVS(BandCharacter)

ObjectDir *sBoneMergeDir;
ObjectDir *sOutfitDir;
ObjectDir *sResourceDir;
ObjectDir *sCharSharedDir;
ObjectDir *sInstrumentDir;
ObjectDir *sInstResourceDir;
ObjectDir *sToDir;

const char *BandIntensityString(int num) {
    if (num != 0) {
        int intensity = num & 0x7F000;
        switch (intensity) {
        case 0x1000:
            return "realtime_idle";
        case 0x2000:
            return "idle";
        case 0x4000:
            return "idle_intense";
        case 0x8000:
            return "play_mellow";
        case 0x10000:
            return "play_normal";
        case 0x20000:
            return "play_intense";
        case 0x40000:
            return "play_solo";
        default:
            MILO_FAIL("Bad intensity %d!", intensity);
            break;
        }
    }
    return "";
}

void BandCharacter::Init() { Register(); }
void BandCharacter::Terminate() {}

BandCharacter::BandCharacter()
    : mPlayFlags(0), unk454(this, 0), mAddDriver(0), mFaceDriver(0), mForceNextGroup(0),
      mForceVertical(1), mOutfitDir(this, 0), mInstDir(this, 0), mTempo("medium"),
      mFileMerger(0), mHeadLookAt(this, 0), mNeckLookAt(this, 0), mEyes(this, 0),
      unk574(0), mTestPrefab(this, 0), mGenre("rocker"), mDrumVenue("small_club"),
      mTestTourEndingVenue(0), mInstrumentType("none"), unk594(this, 0), mInCloset(0),
      unk5a1(0), unk5a2(0), unk5a3(0), mSingalongWeight(this, 0),
      unk5b0(this, kObjListNoNull), unk5c0(this, kObjListNoNull),
      unk5d0(this, kObjListNoNull), unk5e0(this, kObjListNoNull),
      unk5f0(this, kObjListNoNull), unk600(this, kObjListNoNull),
      unk610(this, kObjListNoNull), unk620(this, kObjListNoNull),
      unk630(this, kObjListNoNull), unk640(this, kObjListNoNull),
      unk650(this, kObjListNoNull), unk660(this, kObjListNoNull),
      unk670(this, kObjListNoNull), unk680(this, 0), unk68c(this, 0), unk698(this, 0),
      unk6a4(this, 0), unk6b0(this, 0), mUseMicStandClips(0), unk6bd(1), unk6c0(this, 0),
      mInTourEnding(0), unk6ec(0), unk738(0), unk73c(this, kObjListNoNull),
      unk74c(this, kObjListNoNull) {
    mGroupName[0] = 0;
    mOverrideGroup[0] = 0;
    mFaceGroupName[0] = 0;
    mOverlay = RndOverlay::Find("char_status", true);
    unk734 = Hmx::Object::New<Waypoint>();
    unk734->SetRadius(2.0f);
    unk734->SetStrictRadiusDelta(5.0f);
    unk734->SetAngRadius(0.17453292f);
    unk734->SetStrictAngDelta(0.2617994f);
#ifdef HX_NATIVE
    unk6d8 = 0;
    mNativeReboundOnce = 0;
    mNativeReboundQuiet = 0;
    mNativeReboundBody = 0;
#endif
}

#pragma push
#pragma dont_inline on
BandCharacter::~BandCharacter() {
    TheRnd.CompressTextureCancel(this);
    delete unk734;
}
#pragma pop

void BandCharacter::AddedObject(Hmx::Object *o) {
    Character::AddedObject(o);
    if (streq(o->Name(), "main_add.drv"))
        mAddDriver = dynamic_cast<CharDriver *>(o);
    if (streq(o->Name(), "expression.drv"))
        mFaceDriver = dynamic_cast<CharDriver *>(o);
    else if (streq(o->Name(), "head.lookat"))
        mHeadLookAt = dynamic_cast<CharLookAt *>(o);
    else if (streq(o->Name(), "neck.lookat"))
        mNeckLookAt = dynamic_cast<CharLookAt *>(o);
    else if (streq(o->Name(), "FileMerger.fm"))
        mFileMerger = dynamic_cast<FileMerger *>(o);
    else if (streq(o->Name(), "outfit"))
        mOutfitDir = dynamic_cast<Character *>(o);
    else if (streq(o->Name(), "instrument"))
        mInstDir = dynamic_cast<Character *>(o);
    else if (streq(o->Name(), "CharEyes.eyes"))
        mEyes = dynamic_cast<CharEyes *>(o);
    else if (streq(o->Name(), "singalong.weight"))
        mSingalongWeight = dynamic_cast<CharWeightSetter *>(o);
    else
        AddObject(o);
}

void BandCharacter::RemovingObject(Hmx::Object *o) {
    Character::RemovingObject(o);
    if (o == mAddDriver)
        mAddDriver = 0;
    else if (o == mFileMerger)
        mFileMerger = 0;
}

void BandCharacter::Replace(Hmx::Object *from, Hmx::Object *to) {
    // NOTE: the dc3-lineage Hmx::Object/Character object model has no virtual
    // object-level Replace(Hmx::Object*, Hmx::Object*) (it uses an ObjRef ring +
    // Replace(ObjRef*, Hmx::Object*)); RB3-Wii's BandCharDesc::Replace /
    // Character::Replace base calls have no dc3 equivalent. The ref fix-up is
    // handled by the ring machinery, so only the BandCharacter-specific prefab
    // bookkeeping is ported here.
    if (from == mTestPrefab) {
        mTestPrefab = dynamic_cast<BandCharDesc *>(to);
        if (mTestPrefab)
            CopyCharDesc(mTestPrefab);
    }
}

void BandCharacter::Enter() {
    OnRestoreCategories(0);
    mForceVertical = true;
    mForceNextGroup = false;
    unk574 = false;
    unk5a2 = false;
    unk5a3 = false;
    unk594 = 0;
    mGroupName[0] = 0;
    mPlayFlags &= 0x300000;
    mOverrideGroup[0] = 0;
    mFaceGroupName[0] = 0;
    mFrozen = false;
    Character::Enter();
    SetState("", mPlayFlags, 2, false, false);
    SetHeadLookatWeight(0);
    unk6c0 = 0;
    if (mDriver) {
        Message msg("get_matching_dude");
        DataNode handled = HandleType(msg);
        if (handled.Type() == kDataObject) {
            unk6c0 = handled.Obj<BandCharacter>();
            if (unk6c0) {
                unk6c0->unk6c0 = this;
                CharClip *clip = unk6c0->Driver()->FirstPlayingClip();
                if (clip)
                    MakeMRU(this, clip);
            }
        }
    }
}

void BandCharacter::Exit() { Character::Exit(); }

// Retail uses FUNCTION-LOCAL static Symbols here, not the file-scope ones: the
// target (fn_8227C9F8) opens with two guard-bit tests (0x1, 0x2) against a
// single guard word, each calling Symbol::Symbol(const char*) on first use.
// That makes the function 41 instructions — big enough that /Ob2 declines to
// inline it, which is why SyncObjects/DrawShowing call it out of line with a
// `bl` + bool test instead of expanding the compare. Keep the statics.
bool BandCharacter::InVignetteOrCloset() const {
    static Symbol shell("shell");
    static Symbol vignette("vignette");
    Symbol cliptype = mDriver->ClipType();
    bool ret = false;
    if (cliptype == shell || cliptype == vignette)
        ret = true;
    return ret;
}

DECOMP_FORCEACTIVE(BandCharacter, "BandCharacter.no_anim")

CharClipDriver *BandCharacter::PlayMainClip(int i, bool b) {
    static DataNode &noAnim = DataVariable("BandCharacter.no_anim" + 14);
    if (noAnim.Int())
        return 0;
    if (mGroupName[0] == 0 || !unk454)
        return 0;
    else {
        ObjectDir *clipdir = unk454->ClipDir();
        if (!clipdir)
            return 0;
        else {
            CharClipGroup *grp = clipdir->Find<CharClipGroup>(mGroupName, false);
            if (!grp) {
                MILO_NOTIFY_ONCE(
                    "%s could not find group %s in %s\n",
                    PathName(this),
                    mGroupName,
                    PathName(clipdir)
                );
                return 0;
            } else {
                bool invorc = InVignetteOrCloset();
                int mask = mPlayFlags;
                if (invorc) {
                    mask = mGender == "male" ? 0x20 : 0x40;
                } else if (streq(mGroupName, "realtime_idle")) {
                    mask = mask & 0xFFF80FFF | 0x1000;
                }
                CharClip *clp = 0;
                if (mUseMicStandClips
                    || mInstrumentType == keyboard && ((i & 0xF) != 2) && !b) {
                    CharClip *firstclip = unk454->FirstClip();
                    if (firstclip) {
                        if (firstclip->InGroup(grp)) {
                            i = i & 0xfffffff0U | 4;
                            clp = firstclip;
                        }
                    }
                }
                if (!clp)
                    clp = grp->GetClip(mask);
                if (!clp && invorc && mask == 0x40) {
                    MILO_NOTIFY_ONCE(
                        "%s no female vignette clip in %s, using male",
                        PathName(this),
                        PathName(grp)
                    );
                    mask = 0x20;
                    clp = grp->GetClip(0x20);
                }
                if (!clp) {
                    MILO_NOTIFY_ONCE(
                        "%s no clip w. flags %s in %s",
                        PathName(this),
                        FlagString(mask),
                        PathName(grp)
                    );
                    return 0;
                } else {
                    if (invorc)
                        clp->SetFlags(clp->Flags() | 0xF);
                    else {
                        bool hasDriver = AddDriverClipDir();
                        if (hasDriver) {
                            int imask = 1;
                            if ((i & 0xF) == 2)
                                imask = 2;
                            CharDriver *drv;
                            if ((CharDriver *)unk454 == mDriver)
                                drv = mAddDriver;
                            else
                                drv = mDriver;
                            CharClip *stillclip =
                                drv->ClipDir()->Find<CharClip>("still", false);
                            if (stillclip)
                                drv->Play(stillclip, imask, -1.0f, 1e+30f, 0.0f);
                            else
                                MILO_NOTIFY_ONCE(
                                    "%s could not find still clip", PathName(drv)
                                );
                        }
                    }
                    CharClipDriver *played = unk454->Play(clp, i, -1.0f, 1e+30f, 0.0f);
                    if ((i & 0xF) == 2)
                        mTeleported = true;
                    if (played) {
                        MakeMRU(unk6c0, clp);
                    }
                    return played;
                }
            }
        }
    }
}

void BandCharacter::MakeMRU(BandCharacter *bchar, CharClip *clip) {
    MILO_ASSERT(clip, 0x1A1);
    if (bchar && bchar->Driver()->ClipDir()) {
        CharClip *clip2 =
            bchar->Driver()->ClipDir()->Find<CharClip>(clip->Name(), false);
        if (clip2)
            clip2->MakeMRU();
    }
}

void BandCharacter::PlayFaceClip() {
    if (mFaceDriver) {
        CharClipGroup *grp =
            mFaceDriver->ClipDir()->Find<CharClipGroup>(mFaceGroupName, false);
        if (!grp) {
            MILO_WARN(
                "Could not find CharClipGroup %s in %s\n",
                mFaceGroupName,
                PathName(mDriver->ClipDir())
            );
        } else
            mFaceDriver->Play(grp->GetClip(), 4, -1.0f, 1e+30f, 0.0f);
    }
}

bool BandCharacter::AllowOverride(const char *cc) {
    if (mInstrumentType == "mic") {
        if (!streq(cc, "stand") && !streq(cc, "closeup") && !streq(cc, "extreme_closeup")
            && !streq(cc, "")) {
            return false;
        }
    }
    return true;
}

void BandCharacter::Poll() {
    START_AUTO_TIMER("cc_poll");
    if (unk5a2) {
        Teleport(unk594);
        unk5a2 = false;
    }
    if (unk5a3) {
        const char *name;
        if (mOverrideGroup[0] != 0)
            name = mOverrideGroup;
        else
            name = mInstrumentType == drum ? "sit" : "stand";
        SetState(name, mPlayFlags, 2, false, true);
        unk5a3 = false;
    }

    // Eye interest polling - update head/neck lookat targets
    if (mEyes) {
        RndTransformable *interest = mEyes->GetCurrentInterest();
        if (interest) {
            Transform xfm = interest->WorldXfm();
            if (mHeadLookAt) {
                mHeadLookAt->GetDest()->SetWorldXfm(xfm);
            }
            if (mNeckLookAt) {
                mNeckLookAt->GetDest()->SetWorldXfm(xfm);
            }
        }
    }

    // Edit mode starvation handling - clear driver if clip near end.
    // rb3-Wii dev-build only: retail 360 has no unk6d8 member (see BandCharacter.h).
#ifdef HX_NATIVE
    if (LOADMGR_EDITMODE && unk6d8 < 0.0f && TheTaskMgr.DeltaSeconds() > 0.0f
        && Dir() != this) {
        if (mDriver && mDriver->FirstPlaying()) {
            float startBeat = mDriver->FirstPlaying()->GetClip()->StartBeat();
            float lengthBeats = mDriver->FirstPlaying()->GetClip()->LengthBeats();
            if (mDriver->FirstPlaying()->mBeat < -(0.1f * lengthBeats - startBeat)) {
                mDriver->Clear();
                if (mAddDriver) {
                    mAddDriver->Clear();
                }
            }
        }
    }

    unk6d8 = TheTaskMgr.DeltaSeconds();
#endif

    if (!mFrozen) {
        // Force vertical orientation.
        // (dc3 RndTransformable uses an mDirty bool rather than rb3-Wii's
        // DirtyCache* mCache; DirtyLocalXfm() marks dirty and returns the
        // mutable local transform — same net effect as the mCache force-dirty.)
        if (mForceVertical) {
            MakeVertical(DirtyLocalXfm().m);
        }

        // Expression driver handling
        if (unk454) {
            CharClip *clip = unk454->FirstPlayingClip();
            if (clip && (clip->PlayFlags() & 0xF0) == 0x10) {
                mForceNextGroup = true;
            }
            if (unk454->Starved()) {
                PlayMainClip(4, false);
            }
        }

        // Save and force showing state
        bool wasShowing = Showing();
        SetShowing(true);
        if (Showing()) {
            // Update singalong weight
            if (unk6b0) {
                unsigned int showWeight = 0.0f;
                if (wasShowing && MinLod() < 1) {
                    showWeight = 1;
                }
                unk6b0->SetWeight((float)showWeight);
            }

            // Sync outfit character state
            if (mOutfitDir) {
                mOutfitDir->SetTeleported(mTeleported);
                mOutfitDir->SetMinLod(MinLod());
            }

            // Sync instrument character state
            if (mInstDir) {
                mInstDir->SetTeleported(mTeleported);
                mInstDir->SetMinLod(MinLod());
            }

#ifdef HX_NATIVE
            // wave-07 BAND_ANIM_PROBE: trace the per-frame band animation chain to
            // find WHERE the on-stage band skeleton fails to move. Env-gated, default
            // OFF. BAND_ANIM_PROBE=<substr> matches a member by its dir name (or "*").
            // Captures: driver presence, the playing clip, and a named bone's worldPos
            // BEFORE vs AFTER Character::Poll() (the actual skeleton-drive sweep).
            const char *banimEnv = getenv("BAND_ANIM_PROBE");
            bool banim = false;
            RndTransformable *probeBone = nullptr;
            Vector3 bonePre(0, 0, 0);
            if (banimEnv) {
                const char *myName = Name() ? Name() : "?";
                if (banimEnv[0] == '*' || (myName && strstr(myName, banimEnv)))
                    banim = true;
            }
            if (banim) {
                const char *bn = getenv("BAND_ANIM_BONE");
                if (!bn || !bn[0]) bn = "bone_R-upperArm.mesh";
                probeBone = Find<RndTransformable>(bn, false);
                if (!probeBone) probeBone = Find<RndTransformable>("bone_pelvis.mesh", false);
                if (probeBone) bonePre = probeBone->WorldXfm().v;
            }
#endif

            // Poll base character
            Character::Poll();

#ifdef HX_NATIVE
            if (banim) {
                static int frameCt = 0;
                // throttle: print at most every ~30 frames to keep logs readable
                bool emit = (frameCt++ % 30) == 0;
                if (emit) {
                    const char *myName = Name() ? Name() : "?";
                    CharDriver *drv = mDriver;
                    CharClipDriver *fp = drv ? drv->FirstPlaying() : nullptr;
                    CharClip *clip = drv ? drv->FirstPlayingClip() : nullptr;
                    CharDriver *u454 = unk454;
                    CharClip *u454clip = u454 ? u454->FirstPlayingClip() : nullptr;
                    Vector3 bonePost(0, 0, 0);
                    if (probeBone) bonePost = probeBone->WorldXfm().v;
                    float moved = 0.0f;
                    {
                        Vector3 d;
                        Subtract(bonePost, bonePre, d);
                        moved = Length(d);
                    }
                    fprintf(stderr,
                        "[BAND_ANIM] member='%s' grp='%s' mDriver=%p clipType='%s' "
                        "FirstPlaying=%p clip='%s' | unk454=%p u454clip='%s' bones=%p | "
                        "bonePtr=%p "
                        "bone='%s' pre=(%.4f,%.4f,%.4f) post=(%.4f,%.4f,%.4f) moved=%.6f\n",
                        myName, mGroupName[0] ? mGroupName : "(none)", (void *)drv,
                        drv ? drv->ClipType().Str() : "?", (void *)fp,
                        clip ? (clip->Name() ? clip->Name() : "?") : "(none)",
                        (void *)u454,
                        u454clip ? (u454clip->Name() ? u454clip->Name() : "?") : "(none)",
                        drv ? (void *)drv->GetBones() : nullptr,
                        (void *)probeBone,
                        probeBone ? (probeBone->Name() ? probeBone->Name() : "?") : "(null)",
                        bonePre.x, bonePre.y, bonePre.z,
                        bonePost.x, bonePost.y, bonePost.z, moved);
                }
            }

            // wave-08: now that Character::Poll() has posed the per-member skeleton
            // for THIS frame (the animated bones are live), repoint the outfit skin
            // meshes onto them. Runs once per member (mNativeReboundOnce); retries
            // each Poll until the moving instance is reachable. Must come AFTER
            // Character::Poll() (skeleton posed) and BEFORE the outfit meshes draw.
            RebindOutfitBonesToOwnSkeleton();
#endif

            // Poll child characters
            if (mOutfitDir) {
                mOutfitDir->Poll();
            }
            if (mInstDir) {
                mInstDir->Poll();
            }
        } else {
            mTeleported = true;
        }
        SetShowing(wasShowing);
    }

    UpdateOverlay();
    CalcBoundingSphere();

    // Check current clip for vignette/mic_body status
    unk574 = false;
    if (mDriver) {
        CharClip *clip = mDriver->FirstPlayingClip();
        if (clip) {
            if (clip->Type() == vignette) {
                unk574 = true;
            }
            if (clip->Type() == mic_body) {
                if (unk680) {
                    unk680->SetShowing(clip->Flags() & 0x8000000);
                }
            }
        }
    }

    // Update mesh visibility based on vignette state
    if (unk68c) {
        unk68c->SetShowing(!unk574);
    }
    if (unk698) {
        unk698->SetShowing(!unk574);
    }
    if (unk6a4) {
        unk6a4->SetShowing(!unk574);
    }
}

void BandCharacter::CalcBoundingSphere() {
    mBounding.Zero();
    Sphere s48(Vector3(0, 0, 5.0f), 45.0f);
    Multiply(s48, mSphereBase->WorldXfm(), s48);
    mBounding.GrowToContain(s48);
    if (mInstDir) {
        Sphere s58;
        mInstDir->MakeWorldSphere(s58, false);
        mBounding.GrowToContain(s58);
    }
    Transform tf38;
    FastInvert(mSphereBase->WorldXfm(), tf38);
    Multiply(mBounding, tf38, s48);
    SetSphere(s48);
}

bool BandCharacter::ValidateInterest(CharInterest *ci, ObjectDir *dir) {
    if (!ci)
        return false;
    if (dir) {
        if (dir == this || ci->Dir() == this) {
            if (ci->CategoryFlags() & 0x200)
                return false;
        }
        const DataNode *prop = dir->Property("lookat_cameras", false);
        if (prop && (ci->CategoryFlags() & 1) && !prop->Int())
            return false;
    }
    return true;
}

bool BandCharacter::SetFocusInterest(CharInterest *ci, int i) {
    if (mEyes)
        return mEyes->SetFocusInterest(ci, i);
    else
        return Character::SetFocusInterest(ci, i);
}

void BandCharacter::SetInterestFilterFlags(int i) {
    if (mEyes)
        mEyes->SetInterestFilterFlags(i);
    else
        Character::SetInterestFilterFlags(i);
}

void BandCharacter::ClearInterestFilterFlags() {
    if (mEyes)
        mEyes->ClearInterestFilterFlags();
    else
        Character::ClearInterestFilterFlags();
}

DataNode BandCharacter::OnToggleInterestDebugOverlay(DataArray *da) {
    if (mEyes)
        mEyes->ToggleInterestsDebugOverlay();
    return DataNode(0);
}

struct FlagPair {
    int flag;
    const char *str;
};

const char *BandCharacter::FlagString(int flags) {
    static FlagPair pairs[7] = {
        { 0x1000, "IR|" }, { 0x2000, "I|" },   { 0x4000, "II|" },  { 0x8000, "PM|" },
        { 0x10000, "P|" }, { 0x20000, "PI|" }, { 0x40000, "PS|" },
    };
    char buf[256];
    buf[0] = 0;
    for (unsigned int i = 0; i < 7; i++) {
        if (flags & pairs[i].flag) {
            strcat(buf, pairs[i].str);
            flags &= ~(pairs[i].flag);
        }
    }
    if (flags != 0 || buf[0] == 0)
        strcat(buf, MakeString("0x%x", flags));
    else
        buf[strlen(buf) - 1] = 0;
    return MakeString(buf);
}

void BandCharacter::UpdateOverlay() {
    if (mOverlay->Showing()) {
        *mOverlay << Name() << "- " << mInstrumentType << ": " << mGroupName << " "
                  << FlagString(mPlayFlags & 0x7F000);
        CharClipDriver *firstplaying = mDriver->FirstPlaying();
        if (firstplaying) {
            if (AddDriverClipDir()) {
                *mOverlay << " " << SafeName(firstplaying->GetClip());
                CharClipDriver *firstaddplaying = mAddDriver->FirstPlaying();
                if (firstaddplaying) {
                    *mOverlay << "/" << SafeName(firstaddplaying->GetClip()) << " "
                              << FlagString(firstaddplaying->GetClip()->Flags() & 0x7F000)
                              << " ";
                    *mOverlay << " "
                              << CharClip::BeatAlignString(firstaddplaying->mPlayFlags);
                    *mOverlay << MakeString(
                        " %.2f %.2f",
                        (float)std::fmod(TheTaskMgr.Beat(), 1.0f),
                        (float)std::fmod(firstaddplaying->mBeat, 1.0f)
                    );
                } else {
                    *mOverlay << " "
                              << FlagString(firstplaying->GetClip()->Flags() & 0x7F000);
                    *mOverlay << " "
                              << CharClip::BeatAlignString(firstplaying->mPlayFlags);
                    *mOverlay << MakeString(
                        " %.2f %.2f",
                        (float)std::fmod(TheTaskMgr.Beat(), 1.0f),
                        (float)std::fmod(firstplaying->mBeat, 1.0f)
                    );
                }
            } else {
                *mOverlay << " " << SafeName(firstplaying->GetClip()) << " "
                          << FlagString(firstplaying->GetClip()->Flags() & 0x7F000);
                *mOverlay << " " << CharClip::BeatAlignString(firstplaying->mPlayFlags);
                *mOverlay << MakeString(
                    " %.2f %.2f",
                    (float)std::fmod(TheTaskMgr.Beat(), 1.0f),
                    (float)std::fmod(firstplaying->mBeat, 1.0f)
                );
            }
        }
        *mOverlay << "\n";
    }
}

void BandCharacter::RemoveDrawAndPoll(Character *c) {
    if (c) {
        c->SyncObjects();
        VectorRemove(mDraws, c);
        VectorRemove(mPolls, c);
    }
}

#ifdef HX_NATIVE
// wave-08: rebind this band member's outfit skin meshes from the static shared
// char/main/skeleton magnet onto the member's OWN animated per-member skeleton.
//
// GROUND TRUTH (wave-07 BAND_ANIM_PROBE, built+measured): at Poll time
// Find<RndTransformable>("bone_R-upperArm.mesh") from THIS BandCharacter resolves
// to the LIVE per-member skeleton bone (e.g. player0 0x..429c00) which MOVES
// 100-187u/frame (a real venue clip is playing). The outfit skin meshes, however,
// are bound at parse/merge time to a DIFFERENT, STATIC shared magnet
// (char/main/skeleton.milo, 0x..924ec0, worldPos (7.4,-0.8,57.5)). So the band
// renders static AND the female (trackjacket) flings (her female-authored
// inverse-bind offset lands on the male-bind static magnet -> skinPos 19.8u).
//
// FIX: for each outfit skin bone, look up its animated counterpart BY NAME and
// SetBone(b, own, /*calcOffset=*/false) — keeping the authored gender-correct
// offset while repointing to the moving instance. This fixes BOTH the static band
// (the bone now animates) AND the female fling (her female offset now composes
// against her female-posed per-member bone -> skinPos ~0). Runs once per member,
// after the per-member skeleton is live (guarded by mNativeReboundOnce). Only
// touches bones whose Find resolves to a DIFFERENT instance than the bound one
// (own != bound) so already-correct binds are left alone.
//
// SUPERSEDES the wave-06 renderer SKEL_REBAKE (which rebakes against the static
// magnet and would CONFLICT): each rebound mesh sets RndMesh::mNativeBonesRebound,
// which the renderer's rebake AND fling-clamp both skip (the clamp would freeze a
// now-correctly-animating arm). The clamp stays live for crowd/extras + any
// dynamic hair/face bones we don't rebind.
//
// DEFAULT-ON, TORSO-SCOPED (opt-out RB3_NO_SKEL_REBIND=1) — see the wave-08 finding.
// The rebind repoints the outfit skin meshes from the static shared char/main/
// skeleton magnet onto the member's OWN animated per-member skeleton bone (resolved
// by name via Find, which at Poll time returns the LIVE moving instance), so the
// band ANIMATES: the OUTFIT-bound bone_R-upperArm worldPos goes from byte-identical-
// static to 744+ distinct values, up to ~200u/frame (MEASURED), and the female
// trackjacket stops flinging (skin-to-bone delta 50-65u limb extent, clean — was a
// ~20u static bind mismatch before).
//
// SCOPE = TORSO CLOTHING ONLY (trackjacket / vestdenim / plaidshirt / shred + _skin).
// The high-bone head/hands/face meshes are DELIBERATELY NOT rebound: their long-thin
// geometry (hair strands, fingers) shards when skinned to the animated bone, because
// the animated per-member bone's rotation BASIS differs from the static magnet the
// authored offsets were baked against (bone ORIGINS map correctly — translation
// delta <65u — but a basis mismatch flings vertices far from the bone origin into
// thin radiating shards; calcOffset=true shards too, since the skeleton is already
// animating when first reachable so there is no rest frame to re-bake). The compact
// torso/arm clothing has no such long-thin geometry, so it rebinds CLEANLY and
// animates. Head/hands stay coherent-static via the wave-06 rebake (which still runs
// on the non-rebound meshes). Full-scope rebind (incl. head/hands) is available for
// study via RB3_SKEL_REBIND_FULL=1 (it animates the whole body but shards thin geo).
// Opt-out the whole rebind with RB3_NO_SKEL_REBIND=1 (-> wave-06 coherent static).
void BandCharacter::RebindOutfitBonesToOwnSkeleton() {
    static int sDisabled = -1;
    if (sDisabled < 0) sDisabled = getenv("RB3_NO_SKEL_REBIND") ? 1 : 0;
    if (sDisabled) return;
    if (mNativeReboundOnce) return; // fully rebound: never scan again

    bool probe = getenv("SKEL_REBIND_PROBE") != 0;
    int meshes = 0, slots = 0, reboundBones = 0, reboundMeshes = 0;
    int sawAnimated = 0;  // bones whose Find result differs from the bound magnet
    int gotBodyMesh = 0;  // rebound at least one high-bone (>=20) body/face mesh

    // Collect every skinned mesh the band member draws. The face/hand/tongue/teeth
    // skin meshes live in mOutfitDir's hashtable (reached by ObjDirItr), but the
    // BODY clothing meshes (trackjacket / vestdenim / plaidshirt / shred + _skin.N)
    // are merged resources with an EMPTY dir name — NOT in any hashtable — and are
    // only reachable by walking the dir's DRAW tree (mDraws -> RndGroup patch.grp ->
    // nested meshes), via the engine-native RndDrawable::ListDrawChildren recursion.
    std::vector<RndMesh *> targets;
    // OUTFIT scope only: `this` (member props) + mOutfitDir (clothing/face/hands).
    // mInstDir (guitar / mic / drums) is DELIBERATELY excluded — instruments attach
    // to specific hand/prop bones, not the gender skeleton; rebinding their bones to
    // animated character bones distorts the prop (thin radiating shards).
    Character *drawChars[2] = { this, (Character *)mOutfitDir };
    // worklist of drawables to expand (start with each dir's top draw list)
    std::vector<RndDrawable *> work;
    for (int d = 0; d < 2; d++) {
        Character *dc = drawChars[d];
        if (!dc) continue;
        RndDir *dd = dc;
        // (1) hashtable objects (face/hands/etc.)
        for (ObjDirItr<RndMesh> mit(dd, true); mit != 0; ++mit) {
            RndMesh *m = mit;
            if (m && m->NumBones() != 0 &&
                std::find(targets.begin(), targets.end(), m) == targets.end())
                targets.push_back(m);
        }
        // (2) seed the draw-tree walk from the dir's own draw list
        for (std::vector<RndDrawable *>::iterator it = dd->mDraws.begin();
             it != dd->mDraws.end(); ++it)
            work.push_back(*it);
        // (3) seed from every LOD's draw group + trans group — this is where the
        // BODY CLOTHING (trackjacket / vestdenim / plaidshirt / shred + _skin.N)
        // actually lives (Character::DrawLodOrShadow draws curLod->Group()). It is
        // NOT in mDraws, so without this the female torso mesh is never reached.
        for (int li = 0; li < dc->mLods.size(); li++) {
            if (dc->mLods[li].Group()) work.push_back(dc->mLods[li].Group());
            if (dc->mLods[li].TransGroup()) work.push_back(dc->mLods[li].TransGroup());
        }
    }
    // (3) recurse the draw tree (groups -> nested clothing meshes). Bounded by a
    // visited set so a cyclic/shared group reference cannot loop forever.
    std::vector<RndDrawable *> visited;
    while (!work.empty()) {
        RndDrawable *dr = work.back();
        work.pop_back();
        if (!dr) continue;
        if (std::find(visited.begin(), visited.end(), dr) != visited.end()) continue;
        visited.push_back(dr);
        RndMesh *m = dynamic_cast<RndMesh *>(dr);
        if (m) {
            if (m->NumBones() != 0 &&
                std::find(targets.begin(), targets.end(), m) == targets.end())
                targets.push_back(m);
        }
        std::list<RndDrawable *> kids;
        dr->ListDrawChildren(kids);
        for (std::list<RndDrawable *>::iterator k = kids.begin(); k != kids.end(); ++k)
            if (*k && std::find(visited.begin(), visited.end(), *k) == visited.end())
                work.push_back(*k);
    }

    // TORSO-CLOTHING-ONLY by default (the clean scope — see header comment). Rebind
    // only the body clothing meshes (which have compact geometry and rebind without
    // shards), skipping the high-bone head/hands/face whose long-thin geometry shards
    // under the rotation-basis mismatch. RB3_SKEL_REBIND_FULL=1 rebinds everything
    // (animates the whole body but shards thin geo — for study only).
    static int sTorsoOnly = -1;
    if (sTorsoOnly < 0) sTorsoOnly = getenv("RB3_SKEL_REBIND_FULL") ? 0 : 1;
    for (std::vector<RndMesh *>::iterator mi = targets.begin();
         mi != targets.end(); ++mi) {
        RndMesh *mesh = *mi;
        if (mesh->mNativeBonesRebound) continue; // already rebound: don't re-touch
        if (sTorsoOnly) {
            const char *mn = mesh->Name();
            bool torso = mn && (strstr(mn, "trackjacket") || strstr(mn, "vestdenim") ||
                                strstr(mn, "plaidshirt") || strstr(mn, "shred"));
            if (!torso) continue;
        }
        meshes++;
        if (probe && meshes <= 16) {
            fprintf(stderr, "[SKEL_REBIND]   mesh='%s' numBones=%d\n",
                    mesh->Name() ? mesh->Name() : "?", mesh->NumBones());
        }
        int meshRebound = 0;
        for (int b = 0; b < mesh->NumBones(); b++) {
            RndTransformable *bound = mesh->BoneTransAt(b);
            if (!bound || !bound->Name()) continue;
            slots++;
            RndTransformable *own = Find<RndTransformable>(bound->Name(), false);
            if (!own) continue;
            if (own == bound) continue; // already bound to resolvable instance
            sawAnimated++;
            static int sCalc = -1;
            if (sCalc < 0) sCalc = getenv("RB3_SKEL_REBIND_CALCOFF") ? 1 : 0;
            mesh->SetBone(b, own, sCalc != 0);
            reboundBones++;
            meshRebound++;
            if (probe && reboundBones <= 8) {
                fprintf(stderr,
                    "[SKEL_REBIND] member='%s' mesh='%s' bone='%s' magnet=%p -> own=%p\n",
                    Name() ? Name() : "?", mesh->Name() ? mesh->Name() : "?",
                    bound->Name(), (void *)bound, (void *)own);
            }
        }
        if (meshRebound > 0) {
            mesh->mNativeBonesRebound = true; // renderer: skip rebake + clamp
            reboundMeshes++;
            // Latch gate: any torso clothing mesh (>=11 bones) counts as "body
            // caught". Some torso meshes (vestdenim_resource=18, shred_resource=19,
            // trackjacket_skin.2=11) are <20 bones, so a >=20 gate could miss a
            // member; >=11 covers every torso outfit mesh while still excluding
            // low-bone props/instruments.
            if (mesh->NumBones() >= 11) gotBodyMesh = 1;
            // Optional post-rebind verification (gated SKEL_REBIND_SKINPOS=1). The
            // TRUE skinning-correctness metric is |skinWorld - boneWorld| — how far a
            // bone's composed skin places its vertices from the bone itself. For clean
            // skinning this is bounded by limb/joint extent (~40-65u, MEASURED); a
            // broken bind would fling it to hundreds/thousands. NOTE: a skinned mesh's
            // own WorldXfm is identity (the renderer convention — the palette already
            // carries world space), so a "mesh-local" (skin * inv(meshWorld)) measure
            // is NOT a bind-mismatch — it just reads back the character's world
            // position (~hundreds of u from origin) and is misleading; use the
            // bone-relative delta below.
            if (getenv("SKEL_REBIND_SKINPOS")) {
                float worst = 0.f;
                const char *worstBone = "?";
                for (int b2 = 0; b2 < mesh->NumBones(); b2++) {
                    RndTransformable *bt = mesh->BoneTransAt(b2);
                    if (!bt) continue;
                    Transform skin;
                    Multiply(mesh->BoneOffsetAt(b2), bt->WorldXfm(), skin);
                    Vector3 d;
                    Subtract(skin.v, bt->WorldXfm().v, d);
                    float dd = d.x * d.x + d.y * d.y + d.z * d.z;
                    if (dd > worst) {
                        worst = dd;
                        worstBone = bt->Name() ? bt->Name() : "?";
                    }
                }
                fprintf(stderr,
                    "[SKEL_REBIND_SKINPOS] member='%s' mesh='%s' worstBone='%s' "
                    "skinToBoneDelta=%.3fu (clean<~65u limb extent; fling=hundreds)\n",
                    Name() ? Name() : "?", mesh->Name() ? mesh->Name() : "?",
                    worstBone, std::sqrt(worst));
            }
        }
    }

    // LATCH only once the rebind is COMPLETE. The body clothing + face/hands skin
    // meshes (>=20 bones) can become reachable a few frames AFTER the hair props,
    // so an early latch on the FIRST rebound bone would freeze before the body is
    // caught (regression: female torso/arm left flung). Strategy: keep scanning each
    // Poll; whenever a scan rebinds something new, reset the quiet counter; once we
    // have rebound a high-bone body/face mesh AND a later scan finds nothing new for
    // several consecutive Polls, latch and stop scanning. Meshes already rebound are
    // skipped above, so re-scans only cost the dir/draw-tree walk (bounded).
    if (gotBodyMesh) mNativeReboundBody = 1;
    if (reboundBones > 0) {
        mNativeReboundQuiet = 0;
    } else {
        mNativeReboundQuiet++;
        // Only latch after the body was caught AND a sustained quiet period (late
        // LOD pieces — shoes / pants / accessories — stream in a second or more
        // after the torso, so a short quiet window would latch before they bind and
        // leave them to the fling-clamp). ~90 quiet Polls (>1s) covers the streaming
        // tail. Fallback long grace for members with no >=20-bone outfit (low-LOD /
        // instrument-only dirs) so scanning still stops. The per-mesh
        // mNativeBonesRebound skip keeps the re-scan cost bounded meanwhile.
        if ((mNativeReboundBody && mNativeReboundQuiet >= 90) ||
            mNativeReboundQuiet >= 600)
            mNativeReboundOnce = 1;
    }

    if (probe && (meshes > 0 || reboundBones > 0)) {
        fprintf(stderr,
            "[SKEL_REBIND] member='%s' meshes=%d slots=%d reboundBones=%d "
            "reboundMeshes=%d body=%d quiet=%d latched=%d\n",
            Name() ? Name() : "?", meshes, slots, reboundBones, reboundMeshes,
            mNativeReboundBody, mNativeReboundQuiet, mNativeReboundOnce);
    }
}
#endif

#pragma push
#pragma pool_data off
void BandCharacter::SyncObjects() {
    unk6b0 = Find<CharWeightable>("lod0.weight", false);
    static const char *bones[8] = { "bone_pelvis.mesh", "bone_prop0.mesh",
                                    "bone_prop1.mesh",  "bone_prop2.mesh",
                                    "bone_prop3.mesh",  "spot_neck.mesh",
                                    "spot_navel.mesh",  "bone_mic_stand_bottom.mesh" };
#ifdef HX_NATIVE
    // `bones` has no null sentinel: the matched loop walks until `*ptr == 0`,
    // relying on the static datum *after* the 8-element array being zero on the
    // Wii image. Under clang LP64 that adjacent storage is arbitrary, so the
    // loop reads bones[8] (OOB) as a garbage non-null pointer and crashes in
    // Find(). Bound the walk to the array's 8 known elements (same iteration set).
    for (const char **ptr = bones; ptr != bones + 8; ptr++) {
        RndTransformable *t = Find<RndTransformable>(*ptr, false);
#else
    for (int i = 0; bones[i] != 0; i++) {
        RndTransformable *t = Find<RndTransformable>(bones[i], false);
#endif
        if (t)
            t->SetTransParent(this, false);
    }
    SetDeformation();
    RndMat *feetmat = Find<RndMat>("feet_socks_skin.mat", false);
    if (feetmat) {
        RndMat *legmat = mOutfitDir->Find<RndMat>("legs_socks_swap.mat", false);
        if (legmat)
            feetmat->Copy(legmat, kCopyDeep);
        else {
            RndMat *skinmat = Find<RndMat>("feet_skin.mat", false);
            if (skinmat)
                feetmat->Copy(skinmat, kCopyDeep);
        }
    }

    unk5e0.sort(ByRadius());
    //   iVar4 = *(int *)(this + 0x5e4);
    //   if ((iVar4 != 0) && (*(int *)(iVar4 + 4) != 0)) {
    //     piVar12 = *(int **)(iVar4 + 8);
    //     for (piVar3 = (int *)piVar12[2]; piVar11 = piVar3, piVar3 != piVar12; piVar3 =
    //     (int *)piVar3[2 ])
    //     {
    //       for (; piVar11 != piVar12; piVar11 = (int *)piVar11[1]) {
    //         iVar4 = *piVar11;
    //         iVar5 = *(int *)piVar11[1];
    //         if (*(float *)(iVar5 + 0x178) <= *(float *)(iVar4 + 0x178)) break;
    //         *piVar11 = iVar5;
    //         *(int *)piVar11[1] = iVar4;
    //       }
    //     }
    //   }

    for (ObjPtrList<CharHair>::iterator it = unk5f0.begin();
         it != unk5f0.end();
         ++it) {
        (*it)->Hookup(unk5e0);
    }
    Character::SyncObjects();
#ifdef HX_NATIVE
    // SKEL_REBIND (wave-06): DIAGNOSTIC ONLY, default OFF. A per-member skin-bone
    // rebind onto whatever Find<RndTransformable>(boneName) resolves to in THIS
    // member's dir tree. PROVEN A NO-OP this wave: Find from the BandCharacter dir
    // returns the SAME shared char/main/skeleton.milo magnet the outfit meshes are
    // already bound to (SKEL_REBIND_PROBE: reboundDiff=0 same=4; only ONE
    // bone_R-upperArm instance reachable in the member subtree). There is no live,
    // per-member, female-posed skeleton to rebind to — the char pose pipeline
    // (CharUtlFindBoneTrans -> dir->Find) also resolves to that one magnet, which is
    // STATIC (never animated). So the faithful "rebind to own live skeleton" path is
    // not reachable without a deep, crowd-affecting loader un-share. The shipped fix
    // is the renderer-side static-pose offset rebake (Rnd_Wgpu_RB3.cpp SKEL_REBAKE).
    // This block stays purely as a probe (enable with SET_SKEL_REBIND=1). The Wii
    // path is byte-identical (HX_NATIVE).
    {
        static int sRebind = -1;
        if (sRebind < 0) sRebind = getenv("SET_SKEL_REBIND") ? 1 : 0;
        bool probe = getenv("SKEL_REBIND_PROBE") != 0;
        if (sRebind || probe) {
        int meshes = 0, rebound = 0, same = 0, nullown = 0, slots = 0, logged = 0;
        for (ObjDirItr<RndMesh> mit(this, true); mit != 0; ++mit) {
            RndMesh *mesh = mit;
            if (!mesh || mesh->NumBones() == 0) continue;
            meshes++;
            for (int b = 0; b < mesh->NumBones(); b++) {
                RndTransformable *bound = mesh->BoneTransAt(b);
                if (!bound || !bound->Name()) continue;
                slots++;
                RndTransformable *own = Find<RndTransformable>(bound->Name(), false);
                if (!own) { nullown++; continue; }
                if (own != bound) {
                    if (probe && logged < 6) {
                        fprintf(stderr,
                            "[SKEL_REBIND] member='%s' mesh='%s' bone='%s' bound=%p own=%p REBIND\n",
                            Name() ? Name() : "?", mesh->Name() ? mesh->Name() : "?",
                            bound->Name(), (void *)bound, (void *)own);
                        logged++;
                    }
                    if (sRebind) mesh->SetBone(b, own, false);
                    rebound++;
                } else {
                    same++;
                }
            }
        }
        // Probe whether any per-member skeleton instance distinct from the bound
        // magnet exists in this member's subtree (for the key arm bone).
        if (probe && meshes > 0) {
            int distinct = 0; void *seen[16]; int ns = 0;
            for (ObjDirItr<RndTransformable> tit(this, true); tit != 0; ++tit) {
                RndTransformable *t = tit;
                if (!t || !t->Name() || strstr(t->Name(), "bone_R-upperArm") == 0)
                    continue;
                bool dup = false;
                for (int k = 0; k < ns; k++) if (seen[k] == (void *)t) dup = true;
                if (!dup && ns < 16) { seen[ns++] = (void *)t; distinct++;
                    ObjectDir *d = t->Dir();
                    fprintf(stderr,
                        "[SKEL_REBIND]   upperArm instance=%p dirFile='%s'\n", (void *)t,
                        (d && !d->mStoredFile.empty()) ? d->mStoredFile.c_str() : "-");
                }
            }
            fprintf(stderr,
                "[SKEL_REBIND]   distinct upperArm instances in member subtree=%d\n",
                distinct);
        }
        if (probe) {
            fprintf(stderr,
                "[SKEL_REBIND] member='%s' skinMeshes=%d slots=%d reboundDiff=%d same=%d nullOwn=%d\n",
                Name() ? Name() : "?", meshes, slots, rebound, same, nullown);
        }
        } // if (sRebind || probe)
    }
#endif
    for (ObjPtrList<CharBoneOffset>::iterator it = unk640.begin();
         it != unk640.end();
         ++it) {
        (*it)->ApplyToLocal();
        mOutfitDir->RemoveFromPoll(*it);
    }
    RemoveDrawAndPoll(mOutfitDir);
    RemoveDrawAndPoll(mInstDir);
    if (!mInCloset) {
        for (ObjPtrList<OutfitConfig>::iterator it = unk620.begin();
             it != unk620.end();
             ++it) {
            (*it)->CompressTextures();
        }
        while (!unk610.empty()) {
            RndMeshDeform *df = unk610.front();
            if (!df->Mesh())
                MILO_FAIL("BandCharacter::SyncObjects() - character missing mesh data.");
            df->Mesh()->SetKeepMeshData(false);
            delete df;
        }
        while (!unk600.empty()) {
            delete unk600.front();
        }
        for (ObjPtrList<CharCollide>::iterator it = unk5e0.begin();
             it != unk5e0.end();
             ++it) {
            (*it)->ClearMesh();
        }
    }
    CharMeshHide::HideAll(unk5b0, mDriver->ClipType() == "vignette" ? 0x2000 : 0);
    if (InVignetteOrCloset()) {
        CharClipDriver *first = mDriver->FirstPlaying();
        if (first && mGroupName[0] != 0) {
            int mask = mGender == "male" ? 0x20 : 0x40;
            CharClipDriver *fp = mDriver->FirstPlaying();
            if (!(fp->GetClip()->Flags() & mask)) {
                float frame = fp->GetClip()->BeatToFrame(fp->mBeat);
                CharClipDriver *result = PlayMainClip(2, false);
                if (result) {
                    result->mBeat = result->GetClip()->FrameToBeat(frame);
                }
            }
        }
    }
    const char *eyedfname =
        mGender == "male" ? "eyesdeform_male.anim" : "eyesdeform_female.anim";
    RndPropAnim *panim = Find<RndPropAnim>(eyedfname, false);
    if (panim) {
        panim->SetFrame(mHead.mEye, 1.0f);
    } else
        MILO_NOTIFY_ONCE(
            "Can't find eye settings prop anim %s. This is required to set range of motion and lid tracking for each eye shape.",
            eyedfname
        );
}
#pragma pop

float sDrawOrder = -1.0f;

template <void (CharDriver::*Fn)(Symbol)>
__declspec(noinline) void _outline_SetClipType(CharDriver *_obj, Symbol s) {
    (_obj->*Fn)(s);
}

void BandCharacter::SetClipTypes(Symbol s1, Symbol s2) {
    if (mDriver) {
        _outline_SetClipType<&CharDriver::SetClipType>(mDriver, s2);
        if (BoneServo()) {
            BoneServo()->SetClipType(s1);
        }
    }
}

SAVE_OBJ(BandCharacter, 0x3EB)

BEGIN_LOADS(BandCharacter)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void BandCharacter::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(8, 0);
    Character::PreLoad(bs);
    int hashsize = (mHashTable.UsedSize() + 20) * 2;
    int strsize = mStringTable.UsedSize();
    Reserve(hashsize, strsize + 440);
}

void BandCharacter::PostLoad(BinStream &bs) {
    Character::PostLoad(bs);
    if (gLoadingProxyFromDisk) {
        BandCharDescTest test;
        test.Load(bs);
    } else
        BandCharDesc::Load(bs);
    bs >> mPlayFlags;
    bs >> mTempo;
    if (gRev < 6) {
        if (gRev < 4) {
            int i;
            bs >> i;
            if (gRev < 3) {
                Symbol s;
                bs >> s;
            }
        }
        Symbol s;
        bs >> s;
    }
    if (gRev > 6)
        bs >> mDrumVenue;
    if (gRev != 0)
        mTestPrefab.Load(bs, true, BandCharDesc::GetPrefabs());
    if (gRev > 1 && gRev < 5) {
        bool b;
        bs >> b;
    }
    if (gRev > 7) {
        if (gLoadingProxyFromDisk) {
            Symbol s;
            bs >> s;
        } else
            bs >> mInstrumentType;
    }
}

BEGIN_COPYS(BandCharacter)
    COPY_SUPERCLASS(Character)
    COPY_SUPERCLASS(BandCharDesc)
    CREATE_COPY(BandCharacter)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPlayFlags)
        COPY_MEMBER(mTempo)
        COPY_MEMBER(mDrumVenue)
        COPY_MEMBER(mTestPrefab)
        COPY_MEMBER(mInstrumentType)
    END_COPYING_MEMBERS
END_COPYS

void BandCharacter::CollideList(const Segment &seg, std::list<Collision> &colls) {
    if (CollideSphere(seg)) {
        if (IsProxy())
            RndDrawable::CollideList(seg, colls);
        else {
            if (mOutfitDir)
                mOutfitDir->RndDir::CollideListSubParts(seg, colls);
            if (mInstDir)
                mInstDir->RndDir::CollideListSubParts(seg, colls);
            RndDir::CollideList(seg, colls);
        }
    }
}

RndDrawable *BandCharacter::CollideShowing(const Segment &s, float &f, Plane &pl) {
    if (mOutfitDir->CollideShowing(s, f, pl))
        return this;
    else
        return RndDir::CollideShowing(s, f, pl);
}

void BandCharacter::DrawShowing() {
    if (!unk6bd || !IsLoading()) {
        auto _tmp0 = DataVariable("bandcharacter.show_spheres").Int();
        if (_tmp0) {
            Sphere debugSphere(Vector3(0.0f, 0.0f, 5.0f), 45.0f);
            Multiply(debugSphere, mSphereBase->WorldXfm(), debugSphere);
            Hmx::Color red(1.0f, 0.0f, 0.0f, 1.0f);
            UtilDrawSphere(debugSphere.center, debugSphere.radius, red, 0);
            if (mInstDir) {
                Sphere instSphere;
                mInstDir->MakeWorldSphere(instSphere, false);
                Hmx::Color green(0.0f, 1.0f, 0.0f, 1.0f);
                UtilDrawSphere(instSphere.center, instSphere.GetRadius(), green, 0);
            }
            Hmx::Color blue(0.0f, 0.0f, 1.0f, 1.0f);
            UtilDrawSphere(mBounding.center, mBounding.GetRadius(), blue, 0);
        }
        Character::DrawShowing();
        static const DataNode &n = DataVariable("bandcharacter.show_slot");
        if (n.Int()) {
            const Transform &headxfm = CharUtlFindBoneTrans("bone_head", this)->WorldXfm();
            Vector3 headPos;
            headPos.x = headxfm.v.x;
            headPos.y = headxfm.v.y;
            headPos.z = headxfm.v.z + 6.0f;
            Vector2 screenPos;
            float depth = RndCam::Current()->WorldToScreen(headPos, screenPos);
            if (depth > 0.0f) {
                const char *dirName = Name();
                BandWardrobe::TargetNames *targetNames;
                int _tmp7 = strlen(dirName);
                int charPos = dirName[_tmp7 - 1] - '0';
                if (InVignetteOrCloset()) {
                    targetNames = &TheBandWardrobe->mVignetteNames;
                } else {
                    targetNames = &TheBandWardrobe->mVenueNames;
                }
                Symbol nameSym(Name());
                int slot = 0;
                if (targetNames->names[0] != nameSym) {
                    slot = 1;
                    if (targetNames->names[1] != nameSym) {
                        slot = 2;
                        if (targetNames->names[2] != nameSym) {
                            slot = 3;
                            if (targetNames->names[3] != nameSym) {
                                slot = 4;
                            }
                        }
                    }
                }
                const char *text = MakeString("slot%d pos%d", slot, charPos);
                screenPos.x *= (float)TheRnd.Width();
                screenPos.y *= (float)TheRnd.Height();
                Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
                Vector2 &end = TheRnd.DrawString(text, screenPos, white, false);
                screenPos.x = -(0.5f * (end.x - screenPos.x) - screenPos.x);
                Hmx::Color white2(1.0f, 1.0f, 1.0f, 1.0f);
                TheRnd.DrawString(text, screenPos, white2, true);
            }
        }
    }
}

void BandCharacter::Teleport(Waypoint *way) {
    Character::Teleport(way);
    unk594 = way;
    if (mOutfitDir)
        mOutfitDir->SetTeleported(true);
}

void BandCharacter::SetTempoGenreVenue(Symbol s1, Symbol s2, const char *cc) {
    mTempo = s1;
    mGenre = s2;
    mDrumVenue = NameToDrumVenue(cc);
    if (strstr(cc, "big_club"))
        mTestTourEndingVenue = "big_club";
    else if (strstr(cc, "arena"))
        mTestTourEndingVenue = "arena";
    else if (strstr(cc, "festival"))
        mTestTourEndingVenue = "festival";
}

void BandCharacter::DrawLodOrShadowMode(int i, DrawMode mode) {
    Character::DrawLodOrShadow(i, mode);
    if (mode == kCharDrawTranslucent) {
        mOutfitDir->DrawLodOrShadow(i, mode);
        if (!unk574)
            mInstDir->DrawLodOrShadow(i, mode);
    } else {
        if (!unk574)
            mInstDir->DrawLodOrShadow(i, mode);
        mOutfitDir->DrawLodOrShadow(i, mode);
    }
}

void BandCharacter::DrawLodOrShadow(int i, Character::DrawMode mode) {
    RndEnvironTracker tracker(mEnv, &WorldXfm().v);
    mInstDir->SetEnv(nullptr);
    mOutfitDir->SetEnv(nullptr);
    if (mode & 5) {
        DrawLodOrShadowMode(i, (DrawMode)(mode & 0xfffffffd));
    }
    if (mode & 2) {
        DrawLodOrShadowMode(i, (DrawMode)2);
    }
}

float BandCharacter::ComputeScreenSize(RndCam *cam) {
    if (mOutfitDir)
        return mOutfitDir->ComputeScreenSize(cam);
    else
        return 0;
}

bool BandCharacter::IsLoading() {
    if (mCompressedTextureIDs.size() != 0)
        return true;
    if (mFileMerger)
        return !mFileMerger->mFilesPending.empty();
    return false;
}

void BandCharacter::StartLoad(bool b1, bool b2, bool b3) {
    bool b4 = false;
    bool &_ref0 = mInCloset;
    if (!_ref0) {
        if (b2 || (unk224 & 7))
            b4 = true;
    }
    unk5a1 = b4;
    bool bvar1 = _ref0;
    _ref0 = b2;
    if (bvar1 && !mInCloset)
        b3 = true;
    if (!IsLoading() || !unk6bd || b3) {
        b4 = false;
        if (unk5a1 || b3)
            b4 = true;
        unk6bd = b4;
    }

    if (!mFileMerger->StartLoad(b1) && (_ref0 || (bvar1 && !_ref0))) {
        mFileMerger->Select("blank", FilePath(""), true);
        mFileMerger->StartLoad(b1);
    }
}

#pragma push
#pragma pool_data off
#pragma dont_inline on
void BandCharacter::AddObject(Hmx::Object *o) {
    static Symbol ikScale("CharIKScale");
    static Symbol ikHand("CharIKHand");
    static Symbol collide("CharCollide");
    static Symbol charCuff("CharCuff");
    static Symbol charHair("CharHair");
    static Symbol meshDeform("MeshDeform");
    static Symbol charBoneOffset("CharBoneOffset");
    static Symbol outfitConfig("OutfitConfig");
    static Symbol charMeshHide("CharMeshHide");
    static Symbol ikMidi("CharIKMidi");
    static Symbol cdMidi("CharDriverMidi");
    static Symbol khMidi("CharKeyHandMidi");
    Symbol name = o->ClassName();
    if (name == ikScale)
        unk5c0.push_back(dynamic_cast<CharIKScale *>(o));
    else if (name == ikHand)
        unk5d0.push_back(dynamic_cast<CharIKHand *>(o));
    else if (name == collide)
        unk5e0.push_back(dynamic_cast<CharCollide *>(o));
    else if (name == charHair) {
        CharHair *h = dynamic_cast<CharHair *>(o);
        h->SetManagedHookup(true);
        unk5f0.push_back(h);
    } else if (name == charCuff)
        unk600.push_back(dynamic_cast<CharCuff *>(o));
    else if (name == meshDeform) {
        RndMeshDeform *md = dynamic_cast<RndMeshDeform *>(o);
        if (!md->Mesh())
            MILO_FAIL("RndMeshDeform(%s) has no deform mesh.", md->Name());
        unk610.push_back(md);
    } else if (name == outfitConfig) {
        OutfitConfig *cfg = dynamic_cast<OutfitConfig *>(o);
        unk738 |= cfg->OverlayFlags();
        unk620.push_back(cfg);
    } else if (name == charBoneOffset)
        unk640.push_back(dynamic_cast<CharBoneOffset *>(o));
    else if (name == charMeshHide)
        unk5b0.push_back(dynamic_cast<CharMeshHide *>(o));
    else if (name == ikMidi)
        unk650.push_back(dynamic_cast<CharIKMidi *>(o));
    else if (name == cdMidi)
        unk660.push_back(dynamic_cast<CharDriverMidi *>(o));
    else if (name == khMidi)
        unk670.push_back(dynamic_cast<CharKeyHandMidi *>(o));
}
#pragma pop

void BandCharacter::AddOverlays(BandPatchMesh &mesh) {
    for (ObjPtrList<OutfitConfig>::iterator it = unk620.begin();
         it != unk620.end();
         ++it) {
        for (ObjVector<OutfitConfig::Overlay>::iterator oit = (*it)->mOverlays.begin();
             oit != (*it)->mOverlays.end();
             ++oit) {
            if ((*oit).mCategory & mesh.mCategory) {
                mesh.ConstructQuad((*oit).mTexture);
            }
        }
    }
}

void BandCharacter::DeformHead(SyncMeshCB *cb) {
    if (mOutfitDir) {
        RndMesh *mesh = mOutfitDir->Find<RndMesh>("head.mesh", false);
        if (mesh) {
            BandHeadShaper shaper;
            if (!shaper.Start(this, mGender, mesh, cb, false))
                return;
            else
                mHead.SetShape(shaper);
        }
    }
}

void BandCharacter::SyncOutfitConfig(OutfitConfig *cfg) {
    char buf[256];
    strcpy(buf, cfg->Name());
    char *dot = strchr(buf, '.');
    MILO_ASSERT(dot, 0x5EA);
    int colors[7];
    *dot = 0;
    Symbol sym(buf);
    if (sym == eyes) {
        colors[3] = 0;
        colors[4] = 0;
        colors[5] = 0;
        colors[3] = mHead.mEyeColor;
        cfg->SetColors(&colors[3]);
    } else if (sym == skin || sym == heads) {
        colors[0] = 0;
        colors[1] = 0;
        colors[2] = 0;
        colors[0] = mSkinColor;
        cfg->SetColors(colors);
    } else {
        BandCharDesc::OutfitPiece *piece = mOutfit.GetPiece(sym);
        if (piece)
            cfg->SetColors(piece->mColors);
        else {
            BandCharDesc::OutfitPiece *instpiece = mInstruments.GetPiece(sym);
            if (instpiece)
                cfg->SetColors(instpiece->mColors);
            else
                cfg->Recompose();
        }
    }
    if (sym == skin) {
        OutfitConfig::SetSkinTextures(this, mOutfitDir, this);
        if (unk738) {
            cfg->RecomposePatches(unk738);
            unk738 = 0;
        }
    }
}

void BandCharacter::SetDeformation() {
#ifdef HX_NATIVE
    { static int g=-1; if(g<0)g=getenv("RB3_NO_DEFORM")?1:0; if(g)return; }
#endif
    CharClip *clip = BandCharDesc::GetDeformClip(mGender);
    if (clip) {
        CharBonesMeshes meshes;
        meshes.SetName("tmp_bones", this);
        clip->StuffBones(meshes);
        clip->ScaleDown(meshes, 0);
        clip->ScaleAdd(meshes, 1, 0, 0);
        meshes.PoseMeshes();
        for (ObjPtrList<CharIKScale>::iterator it = unk5c0.begin();
             it != unk5c0.end();
             ++it) {
            (*it)->CaptureBefore();
        }
        CharMeshCacheMgr *mgr = new CharMeshCacheMgr();
        mgr->Disable(!mInCloset);
        for (ObjPtrList<RndMesh>::iterator it = unk73c.begin();
             it != unk73c.end();
             ++it) {
            mgr->SyncMesh(*it, 0xBF);
        }
        DeformHead(mgr);
        for (ObjPtrList<CharCuff>::iterator it = unk600.begin();
             it != unk600.end();
             ++it) {
            (*it)->Deform(mgr, mFileMerger);
        }
        unk73c.clear();
        mgr->StuffMeshes(unk73c);
        clip->ScaleDown(meshes, 0);
        float weights[18];
        ComputeDeformWeights(weights);
        for (int i = 0; i < 18; i++) {
            clip->ScaleAdd(meshes, weights[i], i, 0);
        }
        meshes.PoseMeshes();
        for (ObjPtrList<RndMeshDeform>::iterator it = unk610.begin();
             it != unk610.end();
             ++it) {
            // Retail emits a single `extrwi r5, r11, 1, 30` (logical bit
            // extract), not `srawi`+`clrlwi` — so the shift is UNSIGNED here.
            (*it)->Reskin(mgr, ((unsigned int)unk224 >> 1) & 1);
        }
        for (ObjPtrList<CharCollide>::iterator it = unk5e0.begin();
             it != unk5e0.end();
             ++it) {
            CharCollide *col = *it;
            RndMesh *colmesh = col->mMesh;
            if (colmesh && mgr->HasMesh(colmesh)) {
                col->Deform();
            }
        }
        for (ObjPtrList<CharIKScale>::iterator it = unk5c0.begin();
             it != unk5c0.end();
             ++it) {
            (*it)->CaptureAfter();
        }
        for (ObjPtrList<CharIKHand>::iterator it = unk5d0.begin();
             it != unk5d0.end();
             ++it) {
            (*it)->MeasureLengths();
        }
        for (ObjPtrList<OutfitConfig>::iterator it = unk620.begin();
             it != unk620.end();
             ++it) {
            SyncOutfitConfig(*it);
            (*it)->ApplyAO(mgr);
        }
        delete mgr;
        unk224 &= 0xfffffffd; // i think this might be a bitfield
    }
}

void BandCharacter::PlayGroup(
    const char *cc, bool b, int i, float f, TaskUnits u, Symbol s
) {
    if (mOverrideGroup[0] != 0 && AllowOverride(cc)) {
        cc = mOverrideGroup;
        f = 0;
    }
    if (*cc) {
        bool b528 = mForceNextGroup;
        bool b3 = false;
        unk5a3 = false;
        mForceNextGroup = false;
        b3 = (b | b528) || f != 0;
        CharClipDriver *driver = SetState(cc, mPlayFlags, i, b3, true);
        if (driver) {
            mFrozen = false;
            driver->SetBeatOffset(f, u, s);
        }
        if (BoneServo()->mRegulate && !mTeleported) {
            Teleport(BoneServo()->mRegulate);
        }
    }
}

CharClipDriver *
BandCharacter::SetState(const char *cc, int playFlags, int mask, bool b4, bool b5) {
    if (!streq(mGroupName, cc)) {
        strcpy(mGroupName, cc);
        b4 = true;
    }
    CharDriver *oldDriver = unk454;
    mPlayFlags = playFlags;
    if (AddDriverClipDir() && streq(mGroupName, "realtime_idle")
        && (mPlayFlags & 0x38000)) {
        unk454 = mAddDriver;
    } else {
        unk454 = mDriver;
    }
    if (!b4 && unk454) {
        CharClip *clip = unk454->FirstPlayingClip();
        b4 = true;
        bool rej = unk454 != oldDriver || !clip;
        if (!rej) {
            if ((mPlayFlags & clip->Flags()) == mPlayFlags)
                b4 = false;
        }
    }
    if (b4)
        return PlayMainClip(mask, b5);
    return 0;
}

CharLipSyncDriver *BandCharacter::GetLipSyncDriver() {
    return Find<CharLipSyncDriver>("song.lipdrv", false);
}

DECOMP_FORCEACTIVE(
    BandCharacter,
    "BandCharacter::SetFaceOverrideClip couldn't find clip named %s for %s\n",
    "BandCharacter::SetFaceOverrideClip couldnt find  lip sync driver for %s\n",
    "!mFileMerger->IsLoading()",
    "head"
)

void BandCharacter::SetHeadLookatWeight(float f) {
    if (mHeadLookAt) {
        mHeadLookAt->SetWeight(f);
        if (mNeckLookAt)
            mNeckLookAt->SetWeight(f * 0.5f);
    }
}

bool BandCharacter::SetPrefab(BandCharDesc *desc) {
    mTestPrefab = desc;
    if (mTestPrefab)
        CopyCharDesc(mTestPrefab);
    return unk224;
}

void BandCharacter::ClearDircuts() { mDircuts.clear(); }

bool BandCharacter::AddDircut(Symbol s1, Symbol s2, int i) {
    Symbol animinst = BandCharDesc::GetAnimInstrument(mInstrumentType);
    FilePath fp;
    bool ismale = mGender != "female";
    int mask = 0x8000;
    if (!ismale)
        mask = 0x4000;
    if (i & mask) {
        fp.Set(
            FileRoot(),
            MakeString("char/main/anim/%s/dircut/%s/%s_%s.milo", animinst, mGender, s1, s2)
        );
    } else {
        fp.Set(
            FileRoot(),
            MakeString("char/main/anim/%s/dircut/%s/%s.milo", animinst, mGender, s1)
        );
    }
    return AddDircut(fp);
}

bool BandCharacter::AddDircut(const FilePath &f) {
    MILO_ASSERT(!f.empty(), 0x794);
    for (std::list<String>::iterator it = mDircuts.begin(); it != mDircuts.end(); ++it) {
        if ((const String &)f == *it) {
            return true;
        }
    }
    unsigned int mergerSize = mFileMerger->mMergers.size();
    int start = mFileMerger->FindMergerIndex("directed_cut_0", true);
    unsigned int maxNum = mergerSize - start;
    if (mDircuts.size() >= maxNum)
        return false;
    mDircuts.push_back(f);
    return true;
}

void BandCharacter::SetDircuts() {
    int start = mFileMerger->FindMergerIndex("directed_cut_0", true);
    int maxNum = mFileMerger->mMergers.size() - start;
    MILO_ASSERT(maxNum < 32, 0x7AE);
    int slots[32];
    int i = 0;
    for (int j = 0; j < maxNum; j++) {
        slots[j] = j + start;
    }
    for (std::list<String>::iterator it = mDircuts.begin(); it != mDircuts.end();
         ++it, ++i) {
        const String &str = *it;
        int idx;
        for (idx = i; idx < maxNum; idx++) {
            const FileMerger::Merger &cur = mFileMerger->mMergers[slots[idx]];
            if (cur.mSelected == str || cur.mLoaded == str || cur.loading == str) {
                int tmp = slots[idx];
                slots[idx] = slots[i];
                slots[i] = tmp;
                break;
            }
        }
        if (idx == maxNum) {
            const char *cstr = str.c_str();
            FilePath fp;
            fp.Set(FilePath::Root().c_str(), cstr);
            FileMerger::Merger &cur = mFileMerger->mMergers[slots[i]];
            cur.mSelected = fp;
            cur.mForceReload = false;
        }
    }
    for (; i < maxNum; i++) {
        FilePath fp;
        fp.Set(FilePath::Root().c_str(), "");
        FileMerger::Merger &cur = mFileMerger->mMergers[slots[i]];
        cur.mSelected = fp;
        cur.mForceReload = false;
    }
}

int BandCharacter::GetShotFlags(Symbol s) {
    BandCharDesc::CharInstrumentType ty =
        BandCharDesc::GetInstrumentFromSym(mInstrumentType);
    if (ty >= BandCharDesc::kNumInstruments)
        return 0;
    else {
        DataArray *arr = BandWardrobe::GetGroupArray(ty);
        for (int i = 0; i < arr->Size(); i++) {
            bool symEq = (strcmp(arr->Array(i)->Sym(0).Str(), s.Str()) == 0);
            if (symEq) {
                return arr->Array(i)->Int(1);
            }
        }
    }
    return 0;
}

void BandCharacter::SetContext(Symbol s) {
    CharWeightable *w = Find<CharWeightable>("venue.weight", false);
    if (w)
        w->SetWeight(s == "venue");
    CharWeightable *cw = Find<CharWeightable>("closet.weight", false);
    if (cw)
        cw->SetWeight(s == "closet");
    mOverrideGroup[0] = 0;
    int hideallint = 0;
    if (s == "vignette") {
        SetClipTypes(s, s);
        hideallint = 0x2000;
        mDriver->SetBlendWidth(1.0f);
    } else if (s == "closet") {
        SetClipTypes("shell", "shell");
        mDriver->SetBlendWidth(2.0f);
    } else if (s == "venue") {
        ObjectDir *clipsdir = Find<ObjectDir>("body_clips", true);
        mDriver->SetClips(clipsdir);
        mDriver->SetBlendWidth(1.0f);
        switch (BandCharDesc::GetInstrumentFromSym(mInstrumentType)) {
        case kGuitar:
        case kBass:
            SetClipTypes("guitar_all", "guitar_body");
            break;
        case kDrum:
            SetClipTypes("drum_all", "drum_body");
            break;
        case kMic:
            SetClipTypes("mic_body", "mic_body");
            break;
        case kKeyboard:
            SetClipTypes("keyboard_all", "keyboard_body");
            break;
        default:
            break;
        }
        HandleType(on_set_instrument_clip_types_msg);
    } else {
        MILO_WARN("%s illegal context %s", PathName(this), s);
    }
    CharMeshHide::HideAll(unk5b0, hideallint);
}

void ReplaceSubdir(ObjectDir *d1, ObjectDir *d2) {
    for (int i = 0; i < d1->SubDirs().size(); i++) {
        ObjDirPtr<ObjectDir> dPtr(d1->SubDirs()[i].Ptr());
        d1->RemoveSubDir(dPtr);
    }
    ObjDirPtr<ObjectDir> dPtr(d2);
    d1->AppendSubDir(dPtr);
}

void BandCharacter::SetVisemes() {
    ObjectDir *visemedir = Find<ObjectDir>("visemes", false);
    if (visemedir) {
        ObjectDir *viseme = BandHeadShaper::GetViseme(mGender, false);
        if (viseme)
            ReplaceSubdir(visemedir, viseme);
        CharLipSyncDriver *lsdriver = Find<CharLipSyncDriver>("song.lipdrv", false);
        if (lsdriver)
            lsdriver->SetClips(visemedir);
        CharFaceServo *servo = Find<CharFaceServo>("face.faceservo", false);
        if (servo)
            servo->SetClips(visemedir);
    }
    ObjectDir *vignettedir = Find<ObjectDir>("vignette_visemes", false);
    if (vignettedir) {
        ObjectDir *viseme = BandHeadShaper::GetViseme(mGender, true);
        if (viseme)
            ReplaceSubdir(vignettedir, viseme);
        CharLipSyncDriver *lsdriver = Find<CharLipSyncDriver>("vignette.lipdrv", false);
        if (lsdriver)
            lsdriver->SetClips(vignettedir);
    }
}

void BandCharacter::SetGroupName(const char *name) { strcpy(mGroupName, name); }

OutfitConfig *BandCharacter::GetOutfitConfig(const char *cc) {
    ObjectDir *pObjectDir;
    if (strcmp(cc, "guitar.cfg") == 0 || strcmp(cc, "bass.cfg") == 0
        || strcmp(cc, "drum.cfg") == 0 || strcmp(cc, "mic.cfg") == 0
        || strcmp(cc, "keyboard.cfg") == 0) {
        pObjectDir = mInstDir;
    } else
        pObjectDir = mOutfitDir;
    MILO_ASSERT(pObjectDir, 0x8AD);
    return pObjectDir->Find<OutfitConfig>(cc, false);
}

RndTex *BandCharacter::GetPatchTex(Patch &patch) {
    static Message get_patch_tex("get_patch_tex", DataNode(0), DataNode(0));
    get_patch_tex[0] = DataNode(patch.mTexture);
    {
        DataNode meshNameNode(patch.mMeshName);
        get_patch_tex[1] = meshNameNode;
    }
    const DataNode &handled = HandleType(get_patch_tex);
    if (handled.Type() == kDataUnhandled || !handled.Obj<RndTex>()) {
        if (!mPrefab.Null()) {
            return Find<RndTex>(MakeString("prefab_art%02d.tex", patch.mTexture), false);
        } else
            // Retail X360 has no LOADMGR_EDITMODE "patchtest.tex" arm (rb3-Wii
            // DEV-build addition), same as GetBandLogo above.
            return 0;
    }
    return handled.Obj<RndTex>();
}

RndMesh *BandCharacter::GetPatchMesh(Patch &patch) {
    ObjectDir *dir = this;
    if (patch.mCategory & 0x2E00) {
        dir = mInstDir;
    }
    return dir->Find<RndMesh>(patch.mMeshName.c_str(), false);
}

RndTex *BandCharacter::GetBandLogo() {
    // Retail X360 has NO LOADMGR_EDITMODE / GetNullTexture arm here — that is a
    // rb3-Wii DEV-build addition. Retail also uses a FUNCTION-LOCAL static
    // Message (guard bit + Symbol temp + atexit in the target) rather than the
    // file-scope ::get_band_logo_msg from utl/Messages.h.
    static Message get_band_logo_msg("get_band_logo");
    RndTex *ret;
    DataNode handled = HandleType(get_band_logo_msg);
    if (handled.Type() == kDataObject) {
        ret = handled.Obj<RndTex>();
    } else
        ret = 0;
    return ret;
}

void BandCharacter::Compress(RndTex *tex, bool b) {
    tex->Compress((RndTex::AlphaCompress)b);
}

void BandCharacter::TextureCompressed(int i) {
    std::list<int>::iterator it;
    for (it = mCompressedTextureIDs.begin();
         it != mCompressedTextureIDs.end() && *it != i;
         ++it)
        ;
    if (it == mCompressedTextureIDs.end())
        MILO_WARN("%s Could not find compress texture id %d\n", PathName(this), i);
    else
        mCompressedTextureIDs.erase(it);
}

void BandCharacter::RecomposePatches(BandCharDesc *desc, int i) {
    CopyCharDesc(desc);
    if (!mInCloset) {
        unk224 |= 1;
        StartLoad(true, mInCloset, true);
    } else {
        for (ObjPtrList<OutfitConfig>::iterator it = unk620.begin();
             it != unk620.end();
             ++it) {
            (*it)->RecomposePatches(i);
        }
    }
}

void BandCharacter::SetInstrumentType(Symbol s) {
    if (s != mInstrumentType) {
        mInstrumentType = s;
        SetChanged(8);
    }
}

void BandCharacter::ClearGroup() {
    SetState("", mPlayFlags, 1, false, false);
    mGroupName[0] = 0;
    if (mDriver)
        mDriver->Clear();
    if (mAddDriver)
        mAddDriver->Clear();
}

void BandCharacter::MiloReload() { StartLoad(false, mInCloset, false); }

void BandCharacter::SetLipSync(CharLipSync *sync) {
    CharLipSyncDriver *driver = Find<CharLipSyncDriver>("song.lipdrv", false);
    if (driver) {
        driver->mSongOwner = 0;
        driver->SetLipSync(sync);
    }
}

void BandCharacter::SetSongOwner(CharLipSyncDriver *driver) {
    CharLipSyncDriver *drvr = Find<CharLipSyncDriver>("song.lipdrv", false);
    if (drvr) {
        drvr->mSongOwner = driver;
        drvr->SetLipSync(Find<CharLipSync>("blinktrack.lipsync", false));
        drvr->mSongOffset = RandomFloat(0, 1000.0f);
    }
}

void BandCharacter::SetSingalong(float f) {
    if (mSingalongWeight)
        mSingalongWeight->SetWeight(f);
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(BandCharacter)
    HANDLE_EXPR(get_play_flags, mPlayFlags)
    HANDLE(play_group, OnPlayGroup)
    HANDLE(group_override, OnGroupOverride)
    HANDLE(change_face_group, OnChangeFaceGroup)
    HANDLE_ACTION(clear_group, ClearGroup())
    HANDLE(set_play, OnSetPlay)
    HANDLE_ACTION(
        start_load,
        StartLoad(_msg->Int(2), _msg->Size() > 3 ? _msg->Int(3) : mInCloset, false)
    )
    HANDLE_EXPR(is_loading, IsLoading())
    HANDLE_EXPR(flag_string, FlagString(_msg->Int(2)))
    HANDLE(cam_teleport, OnCamTeleport)
    HANDLE(closet_teleport, OnClosetTeleport)
    HANDLE(install_filter, OnInstallFilter)
    HANDLE(pre_clear, OnPreClear)
    HANDLE(copy_prefab, OnCopyPrefab)
    HANDLE(save_prefab, OnSavePrefab)
    HANDLE(set_file_merger, OnSetFileMerger)
    HANDLE_EXPR(list_dircuts, OnListDircuts())
    HANDLE(load_dircut, OnLoadDircut)
    HANDLE_ACTION(set_context, SetContext(_msg->Sym(2)))
    HANDLE_ACTION(save_from_closet, SavePrefabFromCloset())
    HANDLE_ACTION(set_singalong, SetSingalong(_msg->Float(2)))
    HANDLE(on_post_merge, OnPostMerge)
    HANDLE(hide_categories, OnHideCategories)
    HANDLE(restore_categories, OnRestoreCategories)
    HANDLE_ACTION(game_over, GameOver())
#ifdef MILO_DEBUG
    HANDLE(toggle_interests_overlay, OnToggleInterestDebugOverlay)
#endif
    HANDLE(list_drum_venues, OnListDrumVenues)
    HANDLE(portrait_begin, OnPortraitBegin)
    HANDLE(portrait_end, OnPortraitEnd)
    HANDLE_SUPERCLASS(BandCharDesc)
    HANDLE_SUPERCLASS(Character)
    HANDLE_CHECK(0x9A6)
END_HANDLERS
#pragma pop

void BandCharacter::GameOver() {
    for (ObjPtrList<CharIKMidi>::iterator it = unk650.begin();
         it != unk650.end();
         ++it) {
        CharIKMidi *cur = *it;
        cur->Handle(Message("game_over"), true);
    }
    for (ObjPtrList<CharDriverMidi>::iterator it = unk660.begin();
         it != unk660.end();
         ++it) {
        CharDriverMidi *cur = *it;
        cur->Handle(Message("game_over"), true);
    }
    for (ObjPtrList<CharKeyHandMidi>::iterator it = unk670.begin();
         it != unk670.end();
         ++it) {
        CharKeyHandMidi *cur = *it;
        cur->Handle(Message("game_over"), true);
    }
}

DataNode BandCharacter::ListAnimGroups(int mask) {
    BandCharDesc::CharInstrumentType instType =
        BandCharDesc::GetInstrumentFromSym(mInstrumentType);
    if (BandCharDesc::kNumInstruments <= instType) {
        DataArray *arr = new DataArray(1);
        arr->Node(0) = Symbol();
        DataNode ret(arr, kDataArray);
        arr->Release();
        return DataNode(ret);
    }
    DataArray *groups = BandWardrobe::GetGroupArray(instType);
    int count = 1;
    for (int i = 0; i < groups->Size(); i++) {
        int _tmp1 = groups->Array(i)->Int(1);
        int flags = _tmp1 & mask;
        if ((mask & 0xFF) == (flags & 0xFF) && (flags & 0x3F00))
            count++;
    }
    DataArray *result = new DataArray(count);
    int idx = 1;
    result->Node(0) = Symbol();
    for (int i = 0; i < groups->Size(); i++) {
        int _tmp2 = groups->Array(i)->Int(1);
        int flags = _tmp2 & mask;
        if ((mask & 0xFF) == (flags & 0xFF) && (flags & 0x3F00)) {
            result->Node(idx++) = groups->Array(i)->Sym(0);
        }
    }
    DataNode ret(result, kDataArray);
    result->Release();
    return DataNode(ret);
}

DataNode BandCharacter::OnListDircuts() {
    int mask = 0x3E00;
    if (mGender == "female") {
        if (mGenre == "banger")
            mask |= 0x4;
        else if (mGenre == "dramatic")
            mask |= 0x2;
        else if (mGenre == "rocker")
            mask |= 0x1;
        else if (mGenre == "spazz")
            mask |= 8;
    } else {
        if (mGenre == "banger")
            mask |= 0x40;
        else if (mGenre == "dramatic")
            mask |= 0x20;
        else if (mGenre == "rocker")
            mask |= 0x10;
        else if (mGenre == "spazz")
            mask |= 0x80;
    }
    return ListAnimGroups(mask);
}

DataNode BandCharacter::OnLoadDircut(DataArray *da) {
    Symbol sym = da->Sym(2);
    if (sym == "") {
        return DataNode(0);
    } else {
        ClearDircuts();
        return DataNode(AddDircut(sym, mGenre, GetShotFlags(sym)));
    }
}

DataNode BandCharacter::OnListDrumVenues(DataArray *da) {
    DataArrayPtr ptr;
    ptr->Resize(4);
    for (int i = 0; i < 4; i++) {
        ptr->Node(i) = Symbol(sDrumVenueMappings[i * 2]);
    }
    return DataNode(ptr);
}

DataNode BandCharacter::OnPlayGroup(DataArray *da) {
    bool b6 = false;
    if (da->Size() > 3)
        b6 = da->Int(3);
    bool b1 = false;
    if (da->Size() > 4)
        b1 = da->Int(4);
    float f7 = 0;
    int i5 = 0;
    Symbol s;
    if (da->Size() > 5) {
        f7 = da->Float(5);
        i5 = da->Int(6);
        s = da->Sym(7);
    }
    int i3 = b1 ? 1 : 2;
    PlayGroup(da->Str(2), b6, i3, f7, (TaskUnits)i5, s);
    return DataNode(0);
}

DataNode BandCharacter::OnGroupOverride(DataArray *da) {
    strcpy(mOverrideGroup, da->Str(2));
    mForceNextGroup = true;
    return DataNode(0);
}

DataNode BandCharacter::OnSetPlay(DataArray *da) {
    SetState(mGroupName, mPlayFlags & 0xFFF80FFF | da->Int(2), 3, false, false);
    return DataNode(0);
}

DataNode BandCharacter::OnClosetTeleport(DataArray *da) {
    unk734->DirtyLocalXfm() = LocalXfm();
    Teleport(unk734);
    unk5a2 = false;
    return DataNode(0);
}

DataNode BandCharacter::OnCamTeleport(DataArray *da) {
    if (da->Int(2)) {
        Teleport(unk594);
    } else {
        Waypoint *w = unk594;
        Teleport(0);
        unk594 = w;
        unk5a2 = false;
    }
    return DataNode(0);
}

DataNode BandCharacter::OnChangeFaceGroup(DataArray *da) {
    if (!mFaceDriver || !mFaceDriver->ClipDir())
        return DataNode(0);
    else if (strcmp(mFaceGroupName, da->Str(2)) != 0) {
        strcpy(mFaceGroupName, da->Str(2));
        PlayFaceClip();
    }
    return DataNode(0);
}

void ReplaceRefs(Hmx::Object *theirs, Hmx::Object *mine) {
    MILO_ASSERT(mine, 0xA72);
#ifdef HX_NATIVE
    // V23 (salvage V33 re-apply): reallocation-safe index-based rewrite.
    // The matched fork caches raw std::vector<ObjRef*> iterators into
    // theirs->Refs(), walks them in reverse (it[-1]), and after each
    // ref->Replace() resets it = end(). ref->Replace() ERASES the replaced
    // ObjRef from theirs->mRefs, which under libstdc++ can REALLOCATE the
    // vector and invalidate both cached iterators → the next --it; it[-1]
    // dereferences a dangling iterator → SIGSEGV (fault @ +0x30). MWCC/PPC
    // tolerated the stale iterator; clang-LP64 does not. Semantics identical
    // (reverse walk over current refs; replace any whose owner.Dir() is in
    // {sOutfitDir, sResourceDir, sToDir}).
    while (true) {
        const std::vector<ObjRef *> &refs = theirs->Refs();
        int sz = (int)refs.size();
        bool replaced = false;
        for (int i = sz - 1; i >= 0; --i) {
            // Re-read size each iter — outer Replace() may have shortened it.
            if (i >= (int)theirs->Refs().size()) continue;
            ObjRef *ref = theirs->Refs()[i];
            if (!ref) continue;
            ref->RefOwner();
            if (ref->RefOwner() != NULL) {
                ObjectDir *dir = ref->RefOwner()->Dir();
                bool match =
                    (dir == sOutfitDir) || (dir == sResourceDir) || (dir == sToDir);
                if (match && theirs != mine) {
                    ref->Replace(theirs, mine);
                    replaced = true;
                    break;  // refs may have reallocated; restart from new end
                }
            }
        }
        if (!replaced) break;
    }
#else
    // dc3 lineage stores object refs as an ObjRef ring (begin()/end()) rather
    // than rb3-Wii's std::vector<ObjRef*> mRefs, and ObjRef::Replace takes a
    // single target (the ref already points at `theirs`). Walk the ring, and on
    // each repoint restart from the new head (the ring mutates under us).
    bool changed = true;
    while (changed) {
        changed = false;
        for (ObjRef::iterator it = theirs->Refs().begin();
             it != theirs->Refs().end();
             ++it) {
            ObjRef *ref = it;
            if (ref->RefOwner() != NULL) {
                ObjectDir *dir = ref->RefOwner()->Dir();
                bool match =
                    (dir == sOutfitDir) || (dir == sResourceDir) || (dir == sToDir);
                if (match && theirs != mine) {
                    ref->Replace(mine);
                    changed = true;
                    break;
                }
            }
        }
    }
#endif
}

// just here temporarily until we match the corresponding funcs these strings belong to
DECOMP_FORCEACTIVE(
    BandCharacter,
    "Mesh",
    "%s is being merged into",
    "mine->Dir() == this",
    "bone_",
    "exo_",
    "world.wind",
    "instruments can only have one subdir, which is the resource or colorpalettes.milo",
    "bone_pelvis.mesh",
    "outfits can only have one subdir, which is the resource"
)

MergeFilter::Action
BandCharacter::Filter(Hmx::Object *o1, Hmx::Object *o2, ObjectDir *dir) {
    static Symbol meshName("Mesh");
    static Symbol AmbientOcclusion("AmbientOcclusion");
    static Symbol CharWeightSetter("CharWeightSetter");
    if (o2 == mInstDir) {
        Character *character = dynamic_cast<Character *>(o1);
        mInstDir->CopyBoundingSphere(character);
        mInstDir->RepointSphereBase(this);
    }
    else if (!o2 && o1->ClassName() == AmbientOcclusion)
        return kIgnore;
    if (!o2 && o1->ClassName() == CharWeightSetter)
        return kKeep;
    if (o1->ClassName() == "OutfitConfig") {
        if (o2) {
            MILO_NOTIFY_ONCE("%s is being merged into", PathName(o2));
        }
        unk630.push_back(dynamic_cast<OutfitConfig *>(o1));
    }
    if (o1->Dir() == sCharSharedDir) {
        Hmx::Object *mine = Find<Hmx::Object>(o1->Name(), true);
        MILO_ASSERT(mine->Dir() == this, 0xAB8);
        ::ReplaceRefs(o1, mine);
        return kIgnore;
    }
    if (o1->Dir() == sInstrumentDir || o1->Dir() == sInstResourceDir) {
        RndTransformable *rt = dynamic_cast<RndTransformable *>(o1);
        if (rt) {
            Hmx::Object *found = FindObject(o1->Name(), false);
            if (found) {
                if (rt->TransParent()) {
                    dynamic_cast<RndTransformable *>(found)->SetLocalXfm(rt->LocalXfm());
                }
                ::ReplaceRefs(o1, found);
                return kIgnore;
            }
        }
    }
    if (!(o1->Dir() == sOutfitDir || o1->Dir() == sResourceDir || o1->Dir() == sToDir)) {
        if (o1->Dir() == sBoneMergeDir) {
            RndTransformable *rt = dynamic_cast<RndTransformable *>(o1);
            if (rt) {
                Hmx::Object *found = FindObject(o1->Name(), false);
                if (found)
                    ::ReplaceRefs(o1, found);
            }
        }
        return kIgnore;
    }
    if (strnicmp(o1->Name(), "bone_", 5) == 0) {
        RndTransformable *rt = dynamic_cast<RndTransformable *>(o1);
        if (rt) {
            if (rt->TransParent()) {
                if (strnicmp(rt->TransParent()->Name(), "bone_", 5) == 0 || strnicmp(rt->TransParent()->Name(), "exo_", 4) == 0)
                    return kMerge;
            }
            return kKeep;
        }
    }
    Action action = mFileMerger->MergeAction(o1, o2, dir);
    if (sDrawOrder != -1.0f && o1->ClassName() == meshName) {
        RndMesh *mesh = dynamic_cast<RndMesh *>(o1);
        if (mesh->GetOrder() == 0.0f)
            mesh->SetOrder(5.0f + sDrawOrder);
    }
    if (!o2 && dir != this && action <= kReplace) {
        AddObject(o1);
    }
    return action;
}

MergeFilter::SubdirAction BandCharacter::FilterSubdir(ObjectDir *o1, ObjectDir *toDir) {
#ifdef HX_NATIVE
    // Native load-order fix (char textures rendering white). A shared external
    // resource milo (its own file on disk, e.g. char/main/shared/colorpalettes.milo
    // — the base skin/cloth texture palette referenced by every character) is loaded
    // ONCE and referenced as a subdir by many milos via share=true. The matched
    // action for such a subdir under mSubdirs=kAllSubdirs is kMerge, which MOVES
    // (SetName) its texture objects into THIS character dir, draining the shared
    // instance. On Wii each referencing milo finishes its atomic load (its materials
    // resolving textures against the still-intact subdir) before any merge drains it.
    // On native the loader advances one state per poll, so a concurrent character
    // merge drains the shared palette mid-load of a sibling milo — that sibling's
    // materials then resolve their RndTex ObjPtrs to null (the "couldn't find
    // dummy_torso.tex" cascade) and render with the white fallback texture. Keep an
    // external shared resource subdir as a REFERENCE (kReplace appends it as a subdir
    // of the character) instead of draining it: the palette stays intact, every
    // character's materials resolve their textures through the kept shared subdir,
    // regardless of native load interleaving. Scoped to subdirs that are their own
    // on-disk milo (non-empty stored file). Guarded so the Wii-matched path below is
    // byte-identical to the original.
    //
    // NOTE (mixed-gender band-member skinning deformation): the deform is NOT fixed
    // here, and it is NOT caused by this shim. ROOT CAUSE (2026-06-06, hard-evidenced,
    // engine BAND_DRAW_PROBE / SKEL_LOAD_PROBE / INSTALL_PROBE): all four band outfit
    // meshes bind to ONE shared char/main/skeleton_unshared.milo instance (parent==nil
    // root) at the MALE bind. That shared root is established by NAME RESOLUTION, not by
    // this merge: each char RESOURCE milo (vocal/viseme/guitar/..._resource.milo) lists
    // `char/main/skeleton.milo` as a share=true non-inlined subdir, so the FIRST loader
    // creates it and every subsequent reference (DirLoader::Find) shares it
    // (ObjectDir::LoadSubDir, Dir.cpp). The per-member main.milo skeleton DOES load
    // fresh per member (kInlineCached, 4 distinct instances) but the OUTFIT meshes never
    // bind to it — they bind to the shared skeleton.milo root. The female member
    // (player1, trackjacket: inverse-binds baked for the FEMALE bind) therefore lands on
    // the male-bind shared skeleton and flings ~20u (skinPos=(19.8,3.8,0.4)). PROVEN
    // dead-ends: scoping this shim to kInlineNever palettes (outfits kMerge) — band
    // still binds the shared root, female still flung; full shim-off (retail kMerge) —
    // same shared root + white textures; pruning char_shared's `../skeleton.milo`
    // subdir — strips ALL outfit bones (they had already consolidated onto it). The
    // faithful fix must un-share `char/main/skeleton.milo` for the band at the
    // name-resolution / share layer (broad, high-risk; would also touch the crowd) AND
    // pose each per-member skeleton to its outfit's gender bind (skeleton_unshared.milo
    // is itself male-bind; the gender pose comes from the outfit/clip). The renderer
    // fling clamp (RB3_NO_SKIN_CLAMP) is the shipped fix. See
    // docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md.
    MergeFilter::Action act =
        DefaultSubdirAction(o1, (Subdirs)mFileMerger->mFilesPending.front()->mSubdirs);
    if (act == MergeFilter::kMerge && o1 && !o1->mStoredFile.empty()) {
        act = MergeFilter::kReplace;
    }
    return act;
#else
    return DefaultSubdirAction(o1, (Subdirs)mFileMerger->mFilesPending.front()->mSubdirs);
#endif
}

DataNode BandCharacter::OnInstallFilter(DataArray *da) {
    sBoneMergeDir = 0;
    sOutfitDir = da->Obj<ObjectDir>(2);
    sToDir = da->Obj<ObjectDir>(3);
    sInstrumentDir = da->Obj<ObjectDir>(4);
    Symbol sym = da->Sym(5);
    ObjectDir *boneMeshDir;
    sResourceDir = 0;
    int inSession = 0;
    sCharSharedDir = 0;
    boneMeshDir = 0;
    if (BandCharDesc::GetInstrumentFromSym(sym) < BandCharDesc::kNumInstruments) {
        if (mInstDir) {
            inSession = 1;
        }
    }
    if (inSession) {
        Sphere s = mInstDir->GetSphere();
        s.radius = 0.0f;
        mInstDir->SetSphere(s);
    }
    sDrawOrder = -1.0f;
    const char *bodyparts[] = { "hair",      "glasses",  "facehair", "earrings",
                                "piercings", "eyebrows", "wrist",    "torso",
                                "head",      "legs",     "feet",     "rings",
                                "hands",     0 };
    for (int i = 0; bodyparts[i] != 0; i++) {
        if (strcmp(sym.Str(), bodyparts[i]) == 0) {
            sDrawOrder = (i + 1) * 10;
            break;
        }
    }
    mFileMerger->mFilter = this;
    if (Hmx::Object *pelvis = FindObject("bone_pelvis.mesh", false)) {
        boneMeshDir = pelvis->Dir();
    }
    sInstResourceDir = 0;
    if (sInstrumentDir && sInstrumentDir->SubDirs().size() != 0) {
        if (sOutfitDir->SubDirs().size() > 1) {
            MILO_WARN("instruments can only have one subdir, which is the "
                      "resource or colorpalettes.milo");
        }
        ObjectDir *instSubdir = sInstrumentDir->SubDirs()[0];
        if (instSubdir != boneMeshDir) {
            sInstResourceDir = instSubdir;
        }
    }
    if (sOutfitDir) {
        RndTransformable *xfm = dynamic_cast<RndTransformable *>(
            sOutfitDir->FindObject("bone_pelvis.mesh", false)
        );
        if (xfm) {
            sBoneMergeDir = xfm->Dir();
        }
        Hmx::Object *feetObj = sOutfitDir->FindObject("feet_skin.mat", false);
        if (feetObj) {
            sCharSharedDir = feetObj->Dir();
        }
        if (sOutfitDir->SubDirs().size() != 0) {
            if (sOutfitDir->SubDirs().size() > 1) {
                MILO_WARN("outfits can only have one subdir, which is the resource");
            }
            ObjectDir *outfitSubdir = sOutfitDir->SubDirs()[0];
            if (outfitSubdir != sBoneMergeDir && outfitSubdir != boneMeshDir) {
                sResourceDir = outfitSubdir;
            }
        }
    }
    return DataNode(0);
}

DataNode BandCharacter::OnPreClear(DataArray *da) {
    Symbol sym = da->Sym(2);
    FileMerger *fm = da->Obj<FileMerger>(3);
    static Symbol ocn("OutfitConfig");
    FileMerger::Merger *m = fm->FindMerger(sym, true);
    ObjPtrList<Hmx::Object> &objs = m->mLoadedObjects;
    while (!objs.empty()) {
        Hmx::Object *obj = objs.front();
        if (obj->ClassName() == ocn) {
            unk738 |= dynamic_cast<OutfitConfig *>(obj)->OverlayFlags();
        }
        delete obj;
    }
    return DataNode(0);
}

void BandCharacter::SavePrefabFromCloset() { MILO_ASSERT(0, 0xB95); }

DataNode BandCharacter::OnSavePrefab(DataArray *da) {
    if (mTestPrefab)
        mTestPrefab->CopyCharDesc(this);
    return DataNode(0);
}

DataNode BandCharacter::OnCopyPrefab(DataArray *da) {
    if (mTestPrefab)
        CopyCharDesc(mTestPrefab);
    return DataNode(0);
}

DataNode BandCharacter::OnSetFileMerger(DataArray *da) {
    FilePathTracker tracker(FileRoot());
    SetVisemes();
    unk224 &= 0xfffffff2;
    if (!mFileMerger)
        return DataNode(0);
    FilePath fp70;
    if (!mPrefab.Null())
        fp70.SetRoot(MakeString("char/main/prefab/%s.milo", mPrefab));
    mFileMerger->Select("prefab", fp70, unk5a1);
    const char *bodyparts[14] = { "head",     "eyebrows", "torso",     "legs", "hands",
                                  "wrist",    "rings",    "feet",      "hair", "facehair",
                                  "earrings", "glasses",  "piercings", 0 };
    for (int i = 0; bodyparts[i] != 0; i++) {
        FilePath fp7c;
        MakeOutfitPath(bodyparts[i], fp7c);
        mFileMerger->Select(bodyparts[i], fp7c, unk5a1);
    }
    for (int i = 0; i < 5; i++) {
        mFileMerger->Select(BandCharDesc::GetInstrumentSym(i), FilePath(0), false);
    }
    FilePath fp88("");
    FilePath fp94("");
    FilePath fpa0("");
    FilePath fpac("");
    FilePath fpb8("");
    FilePath fpc4("");
    FilePath fpd0("");
    FilePath fpdc("");
    FilePath fpe8("");
    FilePath fpf4("");
    mPlayFlags &= 0xffcfffff;
    Symbol animinst = BandCharDesc::GetAnimInstrument(mInstrumentType);
    BandCharDesc::CharInstrumentType ty =
        BandCharDesc::GetInstrumentFromSym(mInstrumentType);
    if (ty == BandCharDesc::kGuitar)
        mPlayFlags |= 0x100000;
    else if (ty == BandCharDesc::kBass)
        mPlayFlags |= 0x200000;
    if (ty != BandCharDesc::kNumInstruments) {
        if (!mGenre.Null() && !mTempo.Null()) {
            if (ty == BandCharacter::kMic) {
                mUseMicStandClips = mGenre != "banger";
            }
            fp94.SetRoot(MakeString(
                "char/main/anim/%s/body/%s/realtime_%s.milo", animinst, mGender, mGenre
            ));
            fpa0.SetRoot(MakeString(
                "char/main/anim/%s/body/%s/%s_%s.milo", animinst, mGender, mTempo, mGenre
            ));
            if (ty == BandCharDesc::kDrum) {
                fpac.SetRoot(MakeString(
                    "char/main/anim/%s/body_add/%s/%s_%s.milo",
                    animinst,
                    mGender,
                    mTempo,
                    mGenre
                ));
                fpb8.SetRoot(MakeString(
                    "char/main/anim/%s/body_add/%s/body_add_base.milo", animinst, mGender
                ));
            }
        }
        switch (ty) {
        case BandCharDesc::kGuitar:
        case BandCharDesc::kBass:
            fp88.SetRoot("char/main/rigging/guitar_rh.milo");
            if (mGender == "female")
                fpf4.SetRoot("char/main/anim/rigging/guitar/fret_left_female.milo");
            else
                fpf4.SetRoot("char/main/anim/rigging/guitar/fret_left.milo");
            break;
        case BandCharDesc::kDrum:
            fp88.SetRoot("char/main/rigging/drum.milo");
            fpc4.SetRoot(
                MakeString("char/main/anim/rigging/drum/stick_left_%s.milo", mGender)
            );
            fpd0.SetRoot(
                MakeString("char/main/anim/rigging/drum/stick_right_%s.milo", mGender)
            );
            if (mGender == "female") {
                fpdc.SetRoot("char/main/anim/rigging/drum/pedal_right_female.milo");
                fpe8.SetRoot("char/main/anim/rigging/drum/pedal_left_female.milo");
            } else {
                fpdc.SetRoot("char/main/anim/rigging/drum/pedal_right.milo");
                fpe8.SetRoot("char/main/anim/rigging/drum/pedal_left.milo");
            }
            break;
        case BandCharDesc::kMic:
            fp88.SetRoot("char/main/rigging/vocal.milo");
            break;
        case BandCharDesc::kKeyboard:
            fp88.SetRoot("char/main/rigging/keyboard.milo");
            break;
        default:
            MILO_FAIL("new instrument type added but not supported");
            break;
        }
        if (ty != BandCharDesc::kDrum || !mDrumVenue.Null()) {
            FilePath fp100("");
            MakeInstrumentPath(mInstrumentType, mDrumVenue, fp100);
            mFileMerger->Select(mInstrumentType, fp100, unk5a1);
        }
    }
    if (mInTourEnding && !mTestTourEndingVenue.Null()) {
        FilePath fp10c(MakeString(
            "char/main/anim/%s/finale/%s/%s/tour_endings.milo",
            animinst,
            mGender,
            mTestTourEndingVenue
        ));
        mFileMerger->Select("tour_ending_clips", fp10c, false);
    } else {
        // Retail has no LOADMGR_EDITMODE arm here (edit mode is dev-only and was
        // stripped): the objdiff shows the whole TheLoadMgr+0x5c test, the second
        // "finale" MakeString and the extra FilePath temp as pure inserts on our
        // side, and dropping them takes the frame 0x220 -> 0x210 (= retail),
        // which is what gates this function's 17 EH funclets.
        mFileMerger->Select("tour_ending_clips", FilePath(""), false);
    }
    mFileMerger->Select("rigging", fp88, false);
    mFileMerger->Select("body_realtime_clips", fp94, false);
    mFileMerger->Select("body_tempo_clips", fpa0, false);
    mFileMerger->Select("body_add_clips", fpac, false);
    mFileMerger->Select("body_add_base", fpb8, false);
    mFileMerger->Select("stick_left", fpc4, false);
    mFileMerger->Select("stick_right", fpd0, false);
    mFileMerger->Select("drum_pedal_right", fpdc, false);
    mFileMerger->Select("drum_pedal_left", fpe8, false);
    mFileMerger->Select("guitar_fret", fpf4, false);
    SetDircuts();
    unk5a1 = false;
    return DataNode(0);
}

DataNode BandCharacter::OnPostMerge(DataArray *da) {
    Symbol category = da->Sym(2);
    ObjectDir *dir = da->Obj<ObjectDir>(3);
    bool noTextures = da->Int(4) != 0;
    while (unk630.size() != 0) {
        OutfitConfig *cfg = unk630.front();
        unk630.pop_front();
        SyncOutfitConfig(cfg);
        cfg->Recompose();
        if (!mInCloset)
            cfg->CompressTextures();
    }
    RndTransformable *bone = Find<RndTransformable>("bone_guitar_lh_mod.mesh", false);
    if (bone)
        bone->ResetLocalXfm();
    unk680 = mInstDir->Find<RndMesh>("mic_stand.mesh", false);
    unk68c = Find<RndMesh>("drum_L-stick.mesh", false);
    unk698 = Find<RndMesh>("drum_R-stick.mesh", false);
    unk6a4 = Find<RndMesh>("guitar_pick.mesh", false);
    if (!mFileMerger->mLoadingLoad
        && (noTextures || (mFileMerger->mAsyncLoad && !unk6bd))) {
        SyncObjects();
    }
    return DataNode(0);
}

void BandCharacter::SaveBoneAndChildren(RndTransformable *bone) {
    if (strncmp(bone->Name(), "bone_", 5) == 0) {
        BoneState state;
        state.mBone = bone;
        state.mXfm = bone->WorldXfm();
        unk6e4.push_back(state);
        for (std::list<RndTransformable *>::const_iterator it =
                 bone->TransChildren().begin();
             it != bone->TransChildren().end();
             ++it) {
            SaveBoneAndChildren(*it);
        }
    }
}

DataNode BandCharacter::OnPortraitBegin(DataArray *da) {
    EnableBlinks(false, true);
    BoneState state;
    state.mBone = this;
    state.mXfm = mLocalXfm;
    unk6e4.push_front(state);
    RndTransformable *bone = Find<RndTransformable>("bone_pelvis.mesh", true);
    SaveBoneAndChildren(bone);
    strcpy(unk6f4, mGroupName);
    unk6ec = Hmx::Object::New<CharDriver>();
    unk6ec->Transfer(*mDriver);
    unk6f0 = mPlayFlags;
    return DataNode(0);
}

DataNode BandCharacter::OnPortraitEnd(DataArray *da) {
    EnableBlinks(true, false);
    SetLocalXfm(unk6e4.front().mXfm);
    unk6e4.pop_front();
    for (std::list<BoneState>::iterator it = unk6e4.begin(); it != unk6e4.end();
         ++it) {
        it->mBone->SetWorldXfm(it->mXfm);
    }
    unk6e4.clear();
    strcpy(mGroupName, unk6f4);
    mDriver->Transfer(*unk6ec);
    delete unk6ec;
    unk6ec = 0;
    mPlayFlags = unk6f0;
    return DataNode(0);
}

DataNode BandCharacter::OnHideCategories(DataArray *da) {
    if (!mFileMerger)
        return DataNode(0);
    static Symbol rm("Mesh");
    for (int i = 2; i < da->Size(); i++) {
        FileMerger::Merger *merger = mFileMerger->FindMerger(da->Sym(i), true);
        for (ObjPtrList<Hmx::Object>::iterator it =
                 merger->mLoadedObjects.begin();
             it != merger->mLoadedObjects.end();
             ++it) {
            Hmx::Object *obj = *it;
            if (obj->ClassName() == rm) {
                RndMesh *mesh = dynamic_cast<RndMesh *>(obj);
                if (mesh->Showing()) {
                    mesh->SetShowing(false);
                    unk74c.push_back(mesh);
                }
            }
        }
    }
    return DataNode(0);
}

DataNode BandCharacter::OnRestoreCategories(DataArray *da) {
    while (unk74c.size() != 0) {
        RndMesh *mesh = unk74c.front();
        mesh->SetShowing(true);
        unk74c.pop_front();
    }
    return DataNode(0);
}

BEGIN_PROPSYNCS(BandCharacter)
    SYNC_PROP(tempo, mTempo)
    SYNC_PROP(genre, mGenre)
    SYNC_PROP(drum_venue, mDrumVenue)
    SYNC_PROP(force_vertical, mForceVertical)
    SYNC_PROP_SET(instrument_type, mInstrumentType, SetInstrumentType(_val.Sym()))
    SYNC_PROP_SET(group_name, mGroupName, SetGroupName(_val.Str()))
    SYNC_PROP_SET(
        head_lookat_weight,
        mHeadLookAt ? mHeadLookAt->Weight() : 0,
        SetHeadLookatWeight(_val.Float())
    )
    SYNC_PROP_SET(in_closet, mInCloset, StartLoad(false, _val.Int(), false))
    SYNC_PROP(test_prefab, mTestPrefab)
    SYNC_PROP(use_mic_stand_clips, mUseMicStandClips)
    SYNC_PROP(in_tour_ending, mInTourEnding)
    SYNC_PROP(test_tour_ending_venue, mTestTourEndingVenue)
    SYNC_SUPERCLASS(BandCharDesc)
    SYNC_SUPERCLASS(Character)
END_PROPSYNCS

// sw2 scatter-include (default/BandCharacter <- band3/bandtrack/GemTrack.cpp)
#define gRev gRev_GemTrack
#define gAltRev gAltRev_GemTrack
#include "band3/bandtrack/GemTrack.cpp"
#undef gRev
#undef gAltRev

// ZS-MISSING-INSTANTIATION: retail out-of-lined these template COMDATs in this
// TU; our call sites (dynamic_cast / FormatString) never instantiate them.
// Force emission (BandWardrobe Find<T> idiom).
#include "rndobj/PostProc.h"
#include "utl/MakeString.h"
template RndPostProc *ObjectDir::Find<RndPostProc>(const char *, bool);
template const char *MakeString<const char *>(const char *, const char *);

// Lane-AE scatter force-emit: retail placed OvershellDir's OBJ_CLASSNAME
// COMDAT (?StaticClassName@OvershellDir@@SA?AVSymbol@@XZ) inside the .text span
// pinned to default/BandCharacter. The macro defines it inline, so it is only
// emitted where it is odr-used -- nothing in this TU used it, so our obj
// never defined the symbol and objdiff could not pair it. Force the use.
#include "bandobj/OvershellDir.h"
Symbol ForceEmit_OvershellDir_StaticClassName() { return OvershellDir::StaticClassName(); }
