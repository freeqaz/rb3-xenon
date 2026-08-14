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

// Retail folds both rev words onto ONE base register with offsets 0/4, which
// only happens for internal-linkage, align(4) file-scope statics (altRev+0,
// rev+4) -- not for the DECLARE_REVS/INIT_REVS class statics. Same lever as
// BandWardrobe.cpp / BandDirector.cpp / BandSwatch.cpp. The header's
// gRev/gAltRev static member declarations are left in place (undefined,
// unused, harmless) since no other TU odr-uses them.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs;
#define gAltRev gRevs.altRev
#define gRev gRevs.rev

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

// LANE ACTIONABLE-1 (2026-08-14): 772 B, fuzzy 99.425, 2 charges — the largest
// register-clean SOURCE_INSDEL row outside the do-not-reopen list, and it does
// NOT close. Residual: base emits one extra `stw r28, 0x58, r31` and pushes the
// second insert()'s hidden return slot from 0x58 to 0x5c.
//
// Mechanism: `push_back` is `insert(end(), x)`, and for an STLport list `end()`
// IS the list address (r28). In the FIRST loop below both sides materialize that
// iterator temp exactly once (0x54 arg, 0x58 return). In the SECOND loop retail
// still does it once, but we build it TWICE (0x54 and 0x58) and so need a third
// slot at 0x5c. Confirmed by /Z7 stack-layout: one BASE_ONLY 4-byte slot.
//
// MEASURED AND REJECTED — the oracle's unnamed-temporary form
// `push_back(String(it->Name()))`: it makes things strictly worse (frame shrinks
// 0x10, 24 charges instead of 2) because the named `String s` local IS retail's
// shape — retail's `addi r6, r31, 0x80` is exactly this named temp's slot. So
// the named local is CORRECT here even though rb3-Wii spells it unnamed, and the
// duplicated end() temp is independent of the temporary's form.
//
// This is the same class as the surplus `stw rN, 0x5x, r31` in TrackDir::~TrackDir
// and FaderGroup::~FaderGroup (and the mirror-image dead spill in
// StoreMainPanel::FinishLoad): spill-slot liveness, i.e. permuter territory.
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
        String s(pgroups[i]->Dir()->Name());
        ncThis->mEffectors.push_back(s);
    }

    for (ObjDirItr<BandIKEffector> it(Dir(), true); it; ++it) {
        if (strncmp("player", it->Name(), 6) != 0) {
            String s(it->Name());
            ncThis->mEffectors.push_back(s);
        }
    }
}

BEGIN_SAVES(BandRetargetVignette)
    SAVE_REVS(4, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndPollable)
    bs << mPlayer;
    bs << mBone;
    bs << mProp;
END_SAVES

BEGIN_LOADS(BandRetargetVignette)
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndPollable)
    if (gRev != 0 && gRev < 3) {
        std::list<String> strs;
        bs >> strs;
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
