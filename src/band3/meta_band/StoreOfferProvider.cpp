#include "meta_band/StoreOfferProvider.h"
#include "meta/StoreOffer.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandStoreOffer.h"
#include "meta_band/BandStorePanel.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "rndobj/Mat.h"
#include "ui/UIList.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIListSlot.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

extern bool operator==(const StoreOffer *o, Symbol s);

StoreOfferProvider::StoreOfferProvider(std::vector<StoreOffer *> *offers)
    // Retail (fn_826644C0) stores 0 to 0x34..0x48 and mOffers to 0x30; there is
    // no second parameter and no mPacks member (removed -- see StoreOfferProvider.h).
    : mShortcuts(NULL),
      mOffers(offers),
      mElements(),
      mAlbumBgMat(NULL),
      mGroupBgMat(NULL),
      mSongBgMat(NULL) {}

StoreOfferProvider::~StoreOfferProvider() { ClearList(); }

void StoreOfferProvider::InitData(RndDir *dir) {
    mAlbumBgMat = dir->Find<RndMat>("album.mat", true);
    mGroupBgMat = dir->Find<RndMat>("group.mat", true);
    mSongBgMat = dir->Find<RndMat>("song.mat", true);
}

// ---------------------------------------------------------------------------
// DEFERRED WORK IN THIS TU (lane BV-1 handover; all read off the retail asm)
//
// Two functions exist in retail that this source does not have at all:
//
//  * fn_82663328 (0xB4) StoreOfferProvider::FindSongOffer(int songID)
//      for each offer in *mOffers:
//        if (offer->OfferType() == song && offer->GetSingleSongID() == songID)
//            return offer;
//      return NULL;
//      (`song` is a function-local static; callee fn_827A6D48 =
//       ?GetSingleSongID@StoreOffer@@QBAHXZ. Only in-TU caller: fn_826635D8.)
//
//  * fn_826635D8 (0x188) StoreOfferProvider::ShowBrowserPurchased(const StoreOffer*)
//      if (o->isPurchased) return true;                      // lbz 0x29
//      if (o->OfferType() == song)
//          return o->mPack.isPurchased || o->mAlbum.isPurchased;  // 0xa9 / 0x69
//      if (o->OfferType() == album || o->OfferType() == pack) {
//          for (it : o->mSongsInOffer) {                     // 0xd4 / 0xd8
//              StoreOffer *s = FindSongOffer(*it);
//              if (!s || !s->isPurchased) return false;
//          }
//          return o->NumSongs() != 0;
//      }
//      return false;
//      (OfferType() is re-called for each comparison, not cached in a local.
//       The 0x69/0xa9 offsets independently CONFIRM StoreOffer.h's nested
//       StorePurchaseable layout: mAlbum@0x40, mPack@0x80, +isPurchased@0x29.)
//
// Text() (fn_826639F0, 1756 bytes) is the big consumer: it calls fn_826635D8 at
// 0x82663BF0 / 0x82663C58 / 0x82663D3C (the cost, new and purchased arms).
//
// ⚠ CORRECTION (lane CF-7): the claim that "implementing ShowBrowserPurchased +
// FindSongOffer is the PREREQUISITE for Text" is REFUTED. Text now matches at
// 100.0% normalized (439/439 instructions) with ShowBrowserPurchased still the
// one-line stub below. The prerequisite was only ever the CALL SHAPE -- an
// out-of-line `bl` with the right arity at the right point -- because objdiff
// scores with `functionRelocDiffs=none`, which makes the callee ADDRESS
// score-invisible. Implementing the two missing bodies is still worth doing for
// CORRECTNESS (the stub is behaviourally wrong: it returns o->IsPurchased()
// only), but it is NOT gating any match. Neither function is in splits.txt, so
// neither is scored.
//
// BuildList (fn_826646F0) is DONE as a body-port: 77.2% -> 96.1% normalized
// (lane CF-7c). All six items the BV-1 handover listed are resolved, and each
// was re-verified against the retail asm rather than taken on trust -- see the
// per-site comments in BuildList itself. The nine packed guard bits of
// lbl_82E01E18 are now all present in retail's declaration order, with every
// ctor string read back out of orig/45410914/band.exe at its label address:
//   0x001 store_previous_chunk (820D6D5C)  0x002 browser_group    (820D6D4C)
//   0x004 browser_subgroup     (820D6D38)  0x008 localize_heading (820D6D24)
//   0x010 shortcut_group       (820D6D14)  0x020 shortcut_groups  (820D6D04)
//   0x040 by_artist            (820A4980)  0x080 cover            (820B0D58)
//   0x100 store_next_chunk     (820D6CF0)
// Three things the handover did NOT mention, all read off the asm:
//   * by_artist/cover are declared INSIDE the loop body (guards tested after
//     __RTDynamicCast at 0x82664A14/0x82664A38), not hoisted to the top.
//   * store_previous_chunk/store_next_chunk sit OUTSIDE their ifs -- the guards
//     fire before the BandStorePanel::Instance() calls.
//   * FindArray order is group/subgroup/shortcut/localize, which is NOT the
//     statics' declaration order. Declaration order != use order is itself the
//     proof that the statics are a declared block rather than point-of-use.
//
// What is LEFT on BuildList is regalloc only -- objdiff reports diff_op: none,
// i.e. zero opcode differences. The residue is one register-allocation
// tie-break: retail spills `this` to its parameter home slot 0x194 and keeps
// both dynamic_cast RTTI descriptors in r14/r15, whereas we keep `this` in r15
// and spill one RTTI descriptor to 0x60. That single choice accounts for the
// r15<->r22 swap (12 instrs) and six `lwz r11, 0x194(r31)` reloads. Plus two
// stack-slot swaps (0x58<->0x5c groupSym vs the Symbol(gNullStr) temp,
// 0x98<->0xac sortName vs groupArr). This is permuter-class; do not hand-grind
// it further without the permuter.
//
// DECISIVE NEGATIVE (do not retry): retail's empty-path test branches twice,
// dereference first then pointer (0x82664784 lbz/cmplwi/beq, then 0x82664790
// cmplwi/beq). Writing `if (*p == 0) p = NULL; if (p)` makes MSVC select
// BRANCHLESSLY (subfic/subfe/and, +3 instrs). Writing `if (*p != 0 && p)` is
// better (-3 mismatches, and it is what is here) but MSVC then FOLDS the null
// test away entirely -- it treats the dereference as proving non-null -- so the
// two target branches still cannot both be reproduced from either form.
//
// mPacks REMOVED (member + second ctor parameter, lane NCCC-0731-5f08/f102):
// retail's sizeof(StoreOfferProvider) is 0x4C, not 0x50 -- confirmed via
// class_layout_report.py and BandStorePanel's ctor (`li r3, 0x4c` before
// operator new). See StoreOfferProvider.h and BandStorePanel.cpp:60.
// ---------------------------------------------------------------------------
// Retail fn_826639F0 (1756 bytes). Reconstructed from the retail asm (lane CF-7);
// the rb3-Wii DEV body is the control-flow skeleton but three of its data sources
// do not exist in retail and were re-read out of the bytes:
//
//  * "rbn_icon" arm: Wii tests `offer->mPackedData->mIsRBN`. Retail StoreOffer has
//    no mPackedData; retail emits `bl <HasData>` with a function-local static whose
//    ctor string at lbl_820A44CC is literally "author" (read from band.exe), i.e.
//    `offer->HasData(author)`.
//  * "cost"/"new"/"purchased" arms: Wii open-codes
//    `offer->mOfferState && (offer->mOfferState->mFlags & 1)`. Retail replaces all
//    three with an out-of-line `bl fn_826635D8` = ShowBrowserPurchased.
//  * "purchased" arm: Wii reads mPackedData->mUpgradeId / mOfferState->mFlags.
//    Retail reads three plain bytes off BandStoreOffer -- 0x148, 0x149 and 0x160.
//    mUpgrade is at 0x120 and StorePurchaseable has isAvailable@0x28/isPurchased@0x29
//    (both compiler-verified), so 0x148/0x149 are exactly mUpgrade.isAvailable /
//    mUpgrade.isPurchased, and 0x160 is mUpgradeAvailable.
//
// ★ Every Symbol here is a FUNCTION-LOCAL static, not a utl/Symbols*.h global, and
// the DECLARATION ORDER below is not a guess -- it is read off the guard-bit indices
// packed into the single guard word lbl_82E01DE8:
//   0x0001 album   0x0002 pack    0x0004 song    0x0008 author  0x0010 store_new
//   0x0020 store_upgrade_in_library  0x0040 store_upgrade_purchased
//   0x0080 store_upgrade_available   0x0100 store_in_library
//   0x0200 store_purchased           0x0400 store_unavailable
//   0x0800 store_upgrade_available*  0x1000 store_in_library*
//   0x2000 store_purchased*          0x4000 store_famous_by
// (*) bits 0x800/0x1000/0x2000 point at DIFFERENT Symbol addresses than 0x80/0x100/
// 0x200 while reusing the same ctor strings -- that is the signature of the same
// name declared a second time in a different block scope, which is why the
// available-branch tokens below are re-declared rather than hoisted and shared.
//
// Retail has NO mElements.size() guard (the Wii DEV one was dropped), consistent
// with Mat/DataSymbol/IsActive in this same TU.
void StoreOfferProvider::Text(int i, int pos, UIListLabel *listLabel, UILabel *label)
    const {
    Element *e = mElements[pos];
    StoreOffer *offer = e->mOffer;
    AppLabel *appLabel = dynamic_cast<AppLabel *>(label);
    MILO_ASSERT(appLabel, 0x36);
    if (offer) {
        if (listLabel->Matches("album")) {
            static Symbol album("album");
            static Symbol pack("pack");
            // OfferType() is re-called for the second comparison, not cached.
            if (offer->OfferType() == album || offer->OfferType() == pack) {
                appLabel->SetOfferName(offer);
                return;
            }
        } else if (listLabel->Matches("song")) {
            static Symbol song("song");
            if (offer->OfferType() == song) {
                appLabel->SetOfferName(offer);
                return;
            }
        } else if (listLabel->Matches("rbn_icon")) {
            static Symbol author("author");
            if (offer->HasData(author)) {
                label->SetIcon(0x55);
                return;
            }
        } else if (listLabel->Matches("cost")) {
            if (!ShowBrowserPurchased(offer) && !offer->InLibrary()
                && !offer->IsCompletelyUnavailable()) {
                appLabel->SetOfferCost(offer);
                return;
            }
        } else if (listLabel->Matches("new")) {
            if (!ShowBrowserPurchased(offer) && !offer->InLibrary()
                && offer->IsNewRelease() && !offer->IsCompletelyUnavailable()) {
                static Symbol store_new("store_new");
                appLabel->SetTextToken(store_new);
                return;
            }
        } else if (listLabel->Matches("purchased")) {
            BandStoreOffer *bso = dynamic_cast<BandStoreOffer *>(offer);
            MILO_ASSERT(bso, 0x7c);
            // All six are computed BEFORE the branch in retail, in this order.
            bool notUnavail = !bso->IsCompletelyUnavailable();
            bool isPurchased = ShowBrowserPurchased(bso);
            bool inLibrary = bso->InLibrary();
            bool hasUpgrade = bso->mUpgrade.isAvailable;
            bool upgradePurchased = bso->mUpgrade.isPurchased;
            bool upgradeAvailable = bso->mUpgradeAvailable;
            if (!notUnavail) {
                if (hasUpgrade) {
                    if (upgradeAvailable) {
                        static Symbol store_upgrade_in_library(
                            "store_upgrade_in_library"
                        );
                        appLabel->SetTextToken(store_upgrade_in_library);
                        return;
                    }
                    if (upgradePurchased) {
                        static Symbol store_upgrade_purchased(
                            "store_upgrade_purchased"
                        );
                        appLabel->SetTextToken(store_upgrade_purchased);
                        return;
                    }
                    static Symbol store_upgrade_available("store_upgrade_available");
                    appLabel->SetTextToken(store_upgrade_available);
                    return;
                }
                if (inLibrary) {
                    static Symbol store_in_library("store_in_library");
                    appLabel->SetTextToken(store_in_library);
                    return;
                }
                if (isPurchased) {
                    static Symbol store_purchased("store_purchased");
                    appLabel->SetTextToken(store_purchased);
                    return;
                }
                static Symbol store_unavailable("store_unavailable");
                appLabel->SetTextToken(store_unavailable);
                return;
            }
            if (isPurchased || inLibrary) {
                if (hasUpgrade && !upgradePurchased && !upgradeAvailable) {
                    static Symbol store_upgrade_available("store_upgrade_available");
                    appLabel->SetTextToken(store_upgrade_available);
                    return;
                }
                // Retail branches on inLibrary here, NOT on isPurchased as the Wii
                // DEV source does, and has no store_downloaded arm at all.
                if (inLibrary) {
                    static Symbol store_in_library("store_in_library");
                    appLabel->SetTextToken(store_in_library);
                    return;
                }
                static Symbol store_purchased("store_purchased");
                appLabel->SetTextToken(store_purchased);
                return;
            }
        }
    } else {
        if (e->mActive == 0) {
            if (listLabel->Matches("group") && e->mIsCover == 0) {
                appLabel->SetStoreGroupName(this, pos);
                return;
            } else if (listLabel->Matches("famousby") && e->mIsCover != 0) {
                static Symbol store_famous_by("store_famous_by");
                appLabel->SetTextToken(store_famous_by);
                return;
            } else if (listLabel->Matches("famousby_group") && e->mIsCover != 0) {
                appLabel->SetStoreGroupName(this, pos);
                return;
            }
        } else {
            if (listLabel->Matches("group_center") && e->mActive != 0) {
                appLabel->SetStoreGroupName(this, pos);
                return;
            }
        }
    }
    appLabel->SetTextToken(gNullStr);
}

// Retail (fn_826642B8): no mElements.size() guard, and `song` is a FUNCTION-LOCAL
// static Symbol declared inside the Matches("bg") block (guard word lbl_82E01DF0
// bit 0, Symbol at lbl_82E01DEC, ctor string lbl_820010F0="song") -- not the
// centralized global Symbol our DC3-era Symbols*.h headers provide.
RndMat *StoreOfferProvider::Mat(int i, int j, UIListMesh *mesh) const {
    StoreOffer *offer = mElements[j]->mOffer;
    if (mesh->Matches("bg")) {
        static Symbol song("song");
        if (!offer) {
            return mGroupBgMat;
        } else if (offer->OfferType() == song) {
            return mSongBgMat;
        } else {
            return mAlbumBgMat;
        }
    }
    return mesh->DefaultMat();
}

// Retail (fn_82664398) indexes mElements directly -- there is NO mElements.size()
// guard. The guard comes from the rb3-Wii DEV build; retail dropped it.
Symbol StoreOfferProvider::DataSymbol(int i) const {
    Element *e = mElements[i];
    if (e->mOffer) {
        return e->mOffer->ShortName();
    } else if (e->mActive) {
        return e->mGroupHeading;
    }
    return gNullStr;
}

// Retail (fn_82664418, 52 bytes) likewise has no size() guard.
bool StoreOfferProvider::IsActive(int i) const {
    // Not matched (71.9%): retail materialises the result in a scratch register and
    // byte-masks it at a single return (`clrlwi r3,r11,24`); MSVC here fuses the
    // returns instead (`beqlr`). Tried: uninitialised bool + explicit else (71.5%),
    // and `result = (mActive != false)` which went BRANCHLESS (subic/subfe, 51.5%).
    // BOOL_MASK / permuter-class -- left at the best-scoring shape.
    Element *e = mElements[i];
    bool result = false;
    if (e->mOffer != NULL || e->mActive) {
        result = true;
    }
    return result;
}

// Retail (fn_82664450, 96 bytes) searches ONLY mOffers -- the mPacks fallback
// present in the rb3-Wii DEV source does not exist in the retail X360 build.
StoreOffer *StoreOfferProvider::FindOffer(Symbol s) const {
    std::vector<StoreOffer *>::iterator it =
        std::find(mOffers->begin(), mOffers->end(), s);
    if (it == mOffers->end())
        return NULL;
    return *it;
}

// Retail (fn_82663408 / fn_826634F0, 192 bytes each): a SINGLE loop over mOffers --
// the mPacks fallback loop in the rb3-Wii DEV source does not exist in retail (the
// target calls HasSong once, not twice). The type Symbol is a function-local static
// (FindPack: guard lbl_82E01D90 bit 0, Symbol lbl_82E01D8C, string lbl_820B08D0).
const StoreOffer *StoreOfferProvider::FindPack(const StoreOffer *song) const {
    MILO_ASSERT(song->OfferType() == "song", 0x153);
    static Symbol pack("pack");
    for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
         it != mOffers->end();
         ++it) {
        StoreOffer *cand = *it;
        if (cand->OfferType() == pack && cand->HasSong(song)) {
            return cand;
        }
    }
    return NULL;
}

const StoreOffer *StoreOfferProvider::FindAlbum(const StoreOffer *song) const {
    MILO_ASSERT(song->OfferType() == "song", 0x169);
    static Symbol album("album");
    for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
         it != mOffers->end();
         ++it) {
        StoreOffer *cand = *it;
        if (cand->OfferType() == album && cand->HasSong(song)) {
            return cand;
        }
    }
    return NULL;
}

Symbol StoreOfferProvider::PosToShortcut(int pos) {
    Element **start = &mElements[0];
    Element **it = &mElements[pos];
    while (it >= start) {
        Element *e = *it;
        if (e->mShortcut.Str() != gNullStr) {
            return e->mShortcut;
        }
        --it;
    }
    MILO_FAIL("StoreOfferProvider is missing a shortcut before index %i!", pos);
    return gNullStr;
}

int StoreOfferProvider::ShortcutToPos(Symbol s) {
    for (unsigned int i = 0; i < mElements.size(); i++) {
        if (mElements[i]->mShortcut == s) {
            return i;
        }
    }
    MILO_FAIL("StoreOfferProvider can't find shortcut \"%s\"!", s);
    return 0;
}

int StoreOfferProvider::PosToNextGroupPos(int pos) {
    unsigned int n = mElements.size();
    for (unsigned int i = pos + 1; i < n; i++) {
        if (mElements[i]->mGroupHeading.Str() != gNullStr) {
            return i;
        }
    }
    return 0;
}

int StoreOfferProvider::PosToPrevGroupPos(int pos) {
    for (int i = pos - 2; i >= 0; i--) {
        if (mElements[i]->mGroupHeading.Str() != gNullStr) {
            return i;
        }
    }
    int n = mElements.size();
    for (int i = n - 1; i > pos; i--) {
        if (mElements[i]->mGroupHeading.Str() != gNullStr) {
            return i;
        }
    }
    return 0;
}

StoreOfferProvider::Element *StoreOfferProvider::GetElementAtIndex(int i) const {
    return mElements[i];
}

// Retail fn_826635D8 (0x188 bytes). Signature-only port: the real body needs
// FindSongOffer (not implemented -- see the deferred-work block above) plus
// public access to StoreOffer's protected mPack/mAlbum purchaseables, which
// would mean a StoreOffer.h header change out of scope for this pass. This
// stub exists only so Handle()'s show_browser_purchased arm gets a real call
// target (retail calls this out-of-line) instead of an inline byte load --
// ShowBrowserPurchased itself is NOT in splits.txt and is not scored.
bool StoreOfferProvider::ShowBrowserPurchased(const StoreOffer *o) const {
    return o->IsPurchased();
}

// Field offsets in BandStorePanel are protected; access via byte offsets.
namespace {
// Retail reads the char* at panel+0xC0 / panel+0xCC (BuildList 0x82664778 /
// 0x82665020). String::mStr is at String+0x8, so the Strings themselves are at
// 0xB8 / 0xC4 -- confirmed independently by class_layout_report.py
// (mPrevChunkPath 0xb8, mNextChunkPath 0xc4) and by BandStorePanel.h:94-95.
inline const String &PrevChunkPath(const BandStorePanel *p) {
    return *reinterpret_cast<const String *>(
        reinterpret_cast<const char *>(p) + 0xB8
    );
}
inline const String &NextChunkPath(const BandStorePanel *p) {
    return *reinterpret_cast<const String *>(
        reinterpret_cast<const char *>(p) + 0xC4
    );
}
}

void StoreOfferProvider::BuildList(DataArray *grouping) {
    ClearList();
    mShortcuts = new DataArray(0);
    // Guard bit 0x1 of lbl_82E01E18 is tested at 0x82664750, BEFORE the
    // BandStorePanel::Instance() call at 0x82664774 -- so the static is declared
    // outside the `if`, not inside it.
    static Symbol store_previous_chunk("store_previous_chunk");
    // Retail tests the DEREFERENCE first and the pointer second (0x82664784
    // lbz / cmplwi / beq, then 0x82664790 cmplwi / beq). Writing this as
    // `if (*p == 0) p = NULL; if (p)` instead makes MSVC select branchlessly
    // (subfic / subfe / and) and costs five instructions. The two forms are
    // equivalent here because the dereference is unconditional either way.
    const char *prevPath = PrevChunkPath(BandStorePanel::Instance()).c_str();
    if (*prevPath != 0 && prevPath != NULL) {
        Element *prev = new Element(NULL, store_previous_chunk, true, false, true);
        mElements.push_back(prev);
        mElements.back()->mShortcut = store_previous_chunk;
        mShortcuts->Insert(mShortcuts->Size(), DataNode(store_previous_chunk));
    }
    if (grouping) {
        // Retail declares each of these as a FUNCTION-LOCAL static Symbol (guard-bit
        // test + inline ??0Symbol ctor), and initialises them in this order:
        // browser_group, browser_subgroup, localize_heading, shortcut_group,
        // shortcut_groups -- note localize_heading comes BEFORE shortcut_group,
        // the reverse of the rb3-Wii DEV source.
        static Symbol browser_group("browser_group");
        static Symbol browser_subgroup("browser_subgroup");
        static Symbol localize_heading("localize_heading");
        static Symbol shortcut_group("shortcut_group");
        // Guard bit 0x20 (lbl_82E01E00, string lbl_820D6D04) is initialised but
        // its value is NEVER loaded -- retail compiles MILO_ASSERT to
        // ((void)(cond)), so /Oi drops the pure comparison while keeping the
        // Symbol ctor side effect. Declaration order here is what fixes every
        // later guard bit index.
        static Symbol shortcut_groups("shortcut_groups");
        Symbol sortName = grouping->Sym(0);
        // Retail's FindArray order is group, subgroup, SHORTCUT, localize
        // (0x82664928 / 0x8266493C / 0x8266494C / 0x82664958) -- i.e. the
        // reverse of the declaration order of the last two statics. localize is
        // consumed inline; retail keeps no locArr variable.
        DataArray *groupArr = grouping->FindArray(browser_group, true);
        DataArray *subgroupArr = grouping->FindArray(browser_subgroup, false);
        DataArray *shortcutArr = grouping->FindArray(shortcut_group, false);
        bool localize = grouping->FindArray(localize_heading, true)->Int(1) != 0;
        MILO_ASSERT(sortName != shortcut_groups, 0x1FA);
        Element *lastGroup = NULL;
        Element *lastSubgroup = NULL;
        Symbol curGroupSym(gNullStr);
        for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
             it != mOffers->end();
             ++it) {
            StoreOffer *offer = dynamic_cast<BandStoreOffer *>(*it);
            MILO_ASSERT(offer, 0x208);
            // Guard bits 0x40 / 0x80 are tested INSIDE the loop body, right
            // after __RTDynamicCast (0x82664A14 / 0x82664A38), so both statics
            // are declared at point of use here rather than hoisted.
            static Symbol by_artist("by_artist");
            static Symbol cover("cover");
            // StoreOffer::IsCover() is inlined and DataArray-driven; there is no
            // out-of-line ?IsCover@StoreOffer@@ in the binary. Retail emits this
            // exact chain at 0x82664A54-0x82664AE4, with all three temporaries
            // destroyed at the end of the full expression.
            bool isCover = sortName == by_artist && offer->HasData(cover)
                && offer->GetData(DataArrayPtr(DataNode(cover)), false).Int(NULL)
                    != 0;
            // DataArrayPtr, not a hand-rolled `new DataArray(1)`: retail calls
            // ctor fn_8228D370 and releases the raw mArray once at the bottom of
            // the loop body (0x82664F00), which is exactly ~DataArrayPtr().
            // NB: the extra parens defeat the most-vexing-parse. The DataNode
            // must stay a TEMPORARY -- retail destroys it immediately after the
            // DataArrayPtr ctor (0x82664B60-0x82664B70).
            DataArrayPtr args((DataNode(offer)));
            Symbol groupSym =
                groupArr->ExecuteScript(1, NULL, args, 0)
                    .ForceSym(NULL);
            Symbol subgroupSym = subgroupArr
                ? subgroupArr->ExecuteScript(1, NULL, args, 0).ForceSym(NULL)
                : Symbol(gNullStr);
            if (groupSym.Str() != gNullStr &&
                (!lastGroup || lastGroup->mGroupHeading != groupSym ||
                 lastGroup->mIsCover != isCover)) {
                Element *g = new Element(NULL, groupSym, localize, isCover, false);
                mElements.push_back(g);
                // No `lastSubgroup = NULL;` here -- retail writes r21 only at
                // init and at `lwz r21,-0x4(r11)` (0x82664E98).
                lastGroup = mElements.back();
                Symbol shortcutSym = shortcutArr
                    ? shortcutArr->ExecuteScript(1, NULL, args, 0).ForceSym(NULL)
                    : groupSym;
                if (curGroupSym.Str() == gNullStr || curGroupSym != shortcutSym) {
                    if (localize) {
                        mShortcuts->Insert(
                            mShortcuts->Size(), DataNode(shortcutSym)
                        );
                    } else {
                        mShortcuts->Insert(
                            mShortcuts->Size(), DataNode(shortcutSym.Str())
                        );
                    }
                    lastGroup->mShortcut = shortcutSym;
                    curGroupSym = shortcutSym;
                }
                if (subgroupSym.Str() != gNullStr) {
                    Element *sg =
                        new Element(NULL, subgroupSym, localize, false, false);
                    mElements.push_back(sg);
                    lastSubgroup = mElements.back();
                }
            } else if (subgroupSym.Str() != gNullStr &&
                       (!lastSubgroup ||
                        lastSubgroup->mGroupHeading != subgroupSym)) {
                Element *sg =
                    new Element(NULL, subgroupSym, localize, false, false);
                mElements.push_back(sg);
                lastSubgroup = mElements.back();
            }
            Element *e =
                new Element(offer, Symbol(gNullStr), localize, false, false);
            mElements.push_back(e);
        }
    } else {
        for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
             it != mOffers->end();
             ++it) {
            StoreOffer *offer = dynamic_cast<BandStoreOffer *>(*it);
            MILO_ASSERT(offer, 0x243);
            Element *e =
                new Element(offer, Symbol(gNullStr), false, false, false);
            mElements.push_back(e);
        }
    }
    // Guard bit 0x100 is tested at 0x82664FFC, BEFORE the Instance() call at
    // 0x8266501C -- same shape as store_previous_chunk above.
    static Symbol store_next_chunk("store_next_chunk");
    // Same deref-then-pointer test order as prevPath (0x82665024 / 0x82665030).
    const char *nextPath = NextChunkPath(BandStorePanel::Instance()).c_str();
    if (*nextPath != 0 && nextPath != NULL) {
        Element *next = new Element(NULL, store_next_chunk, true, false, true);
        mElements.push_back(next);
        mElements.back()->mShortcut = store_next_chunk;
        mShortcuts->Insert(mShortcuts->Size(), DataNode(store_next_chunk));
    }
}

// Retail (fn_82664568) loads mElements begin/end straight from 0x34/0x38, i.e. the
// source uses begin()/end(); `&mElements[0]` + `start + size()` forces an extra
// subf/srawi/slwi round-trip to recompute end.
void StoreOfferProvider::ClearList() {
    // 97.3%: remaining 12 mismatches are ALL register swaps (r29/r30/r31
    // permutation) at identical size and instruction order -- permuter-class.
    // Combining the two declarations into the for-init scores identically.
    Element **start = mElements.begin();
    Element **end = mElements.end();
    for (Element **it = start; it != end; ++it) {
        delete *it;
    }
    mElements.clear();
    if (mShortcuts) {
        mShortcuts->Release();
        mShortcuts = NULL;
    }
}

BEGIN_HANDLERS(StoreOfferProvider)
    HANDLE_ACTION(build_list, BuildList(_msg->Array(2)))
    HANDLE_ACTION(build_list_no_grouping, BuildList(NULL))
    HANDLE_ACTION(clear_list, ClearList())
    HANDLE_EXPR(find_offer, FindOffer(_msg->Sym(2)))
    HANDLE_EXPR(
        find_album,
        (Hmx::Object *)FindAlbum(dynamic_cast<StoreOffer *>(_msg->GetObj(2)))
    )
    HANDLE_EXPR(
        find_pack,
        (Hmx::Object *)FindPack(dynamic_cast<StoreOffer *>(_msg->GetObj(2)))
    )
    // Retail (0x82665824-0x826658A4) is a bog-standard macro arm using the
    // same function-local static Symbol as its thirteen siblings (guard bit
    // 0x40 of lbl_82E01E54) and an out-of-line call to ShowBrowserPurchased.
    // The prior hand-written `if (sym == show_browser_purchased)` against the
    // GLOBAL Symbol skipped a guard bit, which shifted every later arm's
    // packed-guard-word bit position by one -- see ShowBrowserPurchased's
    // doc comment for why its body is still a stub.
    HANDLE_EXPR(
        show_browser_purchased,
        DataNode(ShowBrowserPurchased(dynamic_cast<StoreOffer *>(_msg->GetObj(2))))
    )
    HANDLE_EXPR(get_shortcut_array, DataNode(mShortcuts, kDataArray))
    HANDLE_EXPR(has_shortcuts, mShortcuts->Size() != 0)
    HANDLE_EXPR(pos_to_shortcut, PosToShortcut(_msg->Int(2)))
    HANDLE_EXPR(shortcut_to_pos, ShortcutToPos(_msg->Sym(2)))
    HANDLE_EXPR(pos_to_next_group_pos, PosToNextGroupPos(_msg->Int(2)))
    HANDLE_EXPR(pos_to_prev_group_pos, PosToPrevGroupPos(_msg->Int(2)))
    HANDLE_EXPR(is_chunk, mElements[_msg->Int(2)]->mActive)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x2B2)
END_HANDLERS

int StoreOfferProvider::NumData() const { return mElements.size(); }
