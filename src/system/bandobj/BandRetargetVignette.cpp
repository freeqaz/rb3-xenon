#include "bandobj/BandRetargetVignette.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandIKEffector.h"
#include "bandobj/BandWardrobe.h"
#include "char/CharPollGroup.h"
#include "char/CharPollable.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include "decomp.h"
#include "utl/Symbols.h"
#include <string.h>

INIT_REVS(BandRetargetVignette);
const char *BandRetargetVignette::sIkfs[] = {
    "bone_pelvis.ikf", "bone_L-ankle.ikf",   "bone_R-ankle.ikf", "bone_L-foreArm.ikf",
    "bone_L-hand.ikf", "bone_R-foreArm.ikf", "bone_R-hand.ikf",  "bone_prop0.ikf",
    "bone_prop1.ikf",  "bone_prop2.ikf",     "bone_prop3.ikf",   "bone_head.ikf",
    0
};

BandRetargetVignette::BandRetargetVignette()
    : mPlayer("player0"), mBone("bone_R-hand"), mProp(this, 0) {}

BandRetargetVignette::~BandRetargetVignette() {}

void BandRetargetVignette::Enter() { RndPollable::Enter(); }
void BandRetargetVignette::Exit() {}

void BandRetargetVignette::Poll() {
    if (TheBandWardrobe) {
        for (std::list<String>::iterator it = mEffectors.begin(); it != mEffectors.end();
             ++it) {
            const char *cur = it->c_str();
            if (strncmp(cur, "player", 6) == 0 && strlen(cur) == 7) {
                BandCharacter *bchar =
                    TheBandWardrobe->FindTarget(cur, TheBandWardrobe->mVignetteNames);
                if (bchar)
                    bchar->Poll();
                else
                    MILO_NOTIFY_ONCE("%s has NULL for %s", PathName(this), cur);
            } else {
                Dir()->Find<BandIKEffector>(cur, true)->Poll();
            }
        }
    }
}

void BandRetargetVignette::ListPollChildren(std::list<RndPollable *> &polls) const {
    if (TheBandWardrobe) {
        for (int i = 0; i < 4; i++)
            polls.push_back(TheBandWardrobe->GetCharacter(i));
        for (ObjDirItr<BandIKEffector> it(Dir(), true); it; ++it) {
            polls.push_back(it);
        }
    }
}

void BandRetargetVignette::EnterDir() const {
    for (int i = 0; i < 4; i++) {
        BandCharacter *bchar = TheBandWardrobe->GetCharacter(i);
        Symbol name = TheBandWardrobe->mVignetteNames.names[i];
        for (int j = 0; sIkfs[j] != 0; j++) {
            BandIKEffector *ik = bchar->Find<BandIKEffector>(sIkfs[j], false);
            if (ik) {
                ik->mMore = Dir()->Find<BandIKEffector>(
                    MakeString("%s_%s", name, sIkfs[j]), false
                );
            }
        }
    }
    CharPollableSorter psorter;
    std::vector<RndPollable *> pgroups;
    pgroups.reserve(20);
    for (int i = 0; i < 4; i++) {
        BandCharacter *bchar = TheBandWardrobe->GetCharacter(i);
        CharPollGroup *grp = bchar->Find<CharPollGroup>("vignette.pgrp", true);
        grp->SortPolls();
        pgroups.push_back(grp);
    }
    psorter.Sort(pgroups);
    BandRetargetVignette *ncThis = const_cast<BandRetargetVignette *>(this);
    ncThis->mEffectors.clear();
    for (int i = 0; i < pgroups.size(); i++) {
        ncThis->mEffectors.push_back(
            String(pgroups[i]->Dir()->Name())
        );
    }

    for (ObjDirItr<BandIKEffector> it(Dir(), true); it; ++it) {
        const char *itName = it->Name();
        if (strncmp("player", itName, 6) != 0) {
            ncThis->mEffectors.push_back(String(itName));
        }
    }
}

SAVE_OBJ(BandRetargetVignette, 0x9F)

BEGIN_LOADS(BandRetargetVignette)
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndPollable)
    if (gRev != 0 && gRev < 3) {
        std::list<String> strs;
        BinStreamRev d(bs, 0);
        d >> strs;
    }
    if (gRev > 1) {
        bs >> mPlayer;
        bs >> mBone;
        if (gRev > 3)
            bs >> mProp;
    }
END_LOADS

BEGIN_COPYS(BandRetargetVignette)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndPollable)
    CREATE_COPY(BandRetargetVignette)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPlayer)
        COPY_MEMBER(mBone)
        COPY_MEMBER(mProp)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(BandRetargetVignette)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xD1)
END_HANDLERS

BEGIN_PROPSYNCS(BandRetargetVignette)
    SYNC_PROP(effectors, mEffectors)
    SYNC_PROP(player, mPlayer)
    SYNC_PROP(bone, mBone)
    SYNC_PROP(prop, mProp)
END_PROPSYNCS

DECOMP_FORCEACTIVE(BandRetargetVignette, "vector")
