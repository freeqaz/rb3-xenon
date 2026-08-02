#include "bandobj/OvershellDir.h"
#include "bandobj/BandList.h"
#include "utl/BinStream.h"
#include "utl/Symbols.h"

// Retail folds both rev words onto ONE base register with offsets 0/4, which
// only happens for internal-linkage, align(4) file-scope statics (altRev+0,
// rev+4) -- not for the DECLARE_REVS/INIT_REVS class statics. Same lever as
// BandSwatch.cpp / BandWardrobe.cpp / BandDirector.cpp.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gOvershellRevs;

// Retail RB3 keeps the object-version stack as FREE functions (the rb3-Wii
// obj/ObjVersion.h pair): the target calls PushRev(packedRevs, this) /
// PopRev(this) with no BinStream `this`. dc3's newer engine moved them onto
// BinStream, which is what our in-tree utl/BinStream.h declares.
void PushRev(int, Hmx::Object *);
int PopRev(Hmx::Object *);

OvershellDir::OvershellDir()
    : mSlotView("joined_default"), mInTrackMode(0), mControllerType("guitar"),
      mOnlineEnabled(0), mIsLocal(1), mPadNum(1), mPlatform("xbox"),
      mDefaultOption(gNullStr), mDefaultOptionIndex(0) {}

SAVE_OBJ(OvershellDir, 0x1E)

void OvershellDir::PreLoad(BinStream &bs) {
    int rev;
    bs >> rev;
    gOvershellRevs.rev = getHmxRev(rev);
    gOvershellRevs.altRev = getAltRev(rev);
    ASSERT_REVS(1, 0);
    PushRev(packRevs(gOvershellRevs.altRev, gOvershellRevs.rev), this);
    PanelDir::PreLoad(bs);
}

void OvershellDir::PostLoad(BinStream &bs) {
    PanelDir::PostLoad(bs);
    int revs = PopRev(this);
    gOvershellRevs.rev = getHmxRev(revs);
    gOvershellRevs.altRev = getAltRev(revs);
}

BEGIN_COPYS(OvershellDir)
    COPY_SUPERCLASS(PanelDir)
END_COPYS

void OvershellDir::CacheLists() {
    mBandLists.clear();
    for (ObjDirItr<BandList> it(this, false); it != 0; ++it) {
        mBandLists.push_back(it);
    }
}

void OvershellDir::ConcealAllLists(bool now) {
    for (int i = 0; i < mBandLists.size(); i++) {
        if (now)
            mBandLists[i]->ConcealNow();
        else
            mBandLists[i]->Conceal();
    }
}

void OvershellDir::ViewChanged() {
    static Message msgShowView(
        "show_view",
        DataNode(mSlotView),
        DataNode(mInTrackMode),
        DataNode(mDefaultOption),
        DataNode(mDefaultOptionIndex)
    );
    msgShowView[0] = DataNode(mSlotView);
    msgShowView[1] = DataNode(mInTrackMode);
    msgShowView[2] = DataNode(mDefaultOption);
    msgShowView[3] = DataNode(mDefaultOptionIndex);
    HandleType(msgShowView);
    mDefaultOption = gNullStr;
    mDefaultOptionIndex = -1;
}

BEGIN_HANDLERS(OvershellDir)
    HANDLE_SUPERCLASS(PanelDir)
    HANDLE_ACTION(set_default_option, SetDefaultOption(_msg->Sym(2)))
    HANDLE_ACTION(set_default_option_index, SetDefaultOptionIndex(_msg->Int(2)))
    HANDLE_ACTION(cache_lists, CacheLists())
    HANDLE_ACTION(conceal_all_lists, ConcealAllLists(_msg->Int(2)))
    HANDLE_CHECK(0x64)
END_HANDLERS

BEGIN_PROPSYNCS(OvershellDir)
    SYNC_PROP(controller_type, mControllerType)
    SYNC_PROP(online_enabled, mOnlineEnabled)
    SYNC_PROP(is_local, mIsLocal)
    SYNC_PROP(pad_num, mPadNum)
    SYNC_PROP(platform, mPlatform)
    // NOTE (lane CU-2): slot_view is the ONE arm of this function that does not
    // match (97.7%, 9 reordered instrs, target and base both 772 B).  Retail's
    // layout is: func block FALLS THROUGH, `return false` sits after it as a LOCAL
    // `li r3,0; b` (retail `beq 0x27c` == byte 636 == instr 159).  Measured, all
    // three source spellings -- do not re-hunt:
    //   SYNC_PROP_MODIFY (this)                     -> 97.7%, correct size
    //   SYNC_PROP_MODIFY_ALT / `if (synced){...}`   -> 91.4%, 3 instrs SHORT
    //   flattened `if (!PropSync(..)) return false` -> byte-identical to MODIFY
    // The ALT polarity gets retail's fall-through right but MSVC then TAIL-MERGES
    // the `return false`, which retail keeps local.  So the blocker is cross-jump
    // suppression, not source order -- a codegen-layout wall, not a macro choice.
    // (in_track_mode below already matches as plain MODIFY; do not change it --
    //  measured 96.8% as _ALT.)
    SYNC_PROP_MODIFY(slot_view, mSlotView, ViewChanged())
    SYNC_PROP_MODIFY(in_track_mode, mInTrackMode, ViewChanged())
    SYNC_SUPERCLASS(PanelDir)
END_PROPSYNCS
