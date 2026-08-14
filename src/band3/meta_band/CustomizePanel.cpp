#include "meta_band/CustomizePanel.h"
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandCharacter.h"
#include "meta/Profile.h"
#include "meta_band/AssetMgr.h"
#include "meta_band/AssetProvider.h"
#include "meta_band/AssetTypes.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/CharCache.h"
#include "meta_band/ClosetMgr.h"
#include "meta_band/CurrentOutfitProvider.h"
#include "meta_band/InstrumentFinishProvider.h"
#include "meta_band/MakeupProvider.h"
#include "meta_band/NewAssetProvider.h"
#include "meta_band/PrefabMgr.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/UIEventMgr.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "ui/UIComponent.h"
#include "ui/UIPanel.h"
#include "utl/Messages3.h"
#include "utl/NetCacheMgr.h"
#include "utl/Messages.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

CustomizePanel::CustomizeState CustomizePanel::sBackStates[] = {
    (CustomizeState)0,    (CustomizeState)0,    (CustomizeState)1,    (CustomizeState)1,
    (CustomizeState)3,    (CustomizeState)4,    (CustomizeState)4,    (CustomizeState)4,
    (CustomizeState)1,    (CustomizeState)8,    (CustomizeState)8,    (CustomizeState)8,
    (CustomizeState)8,    (CustomizeState)8,    (CustomizeState)8,    (CustomizeState)8,
    (CustomizeState)8,    (CustomizeState)1,    (CustomizeState)0x11, (CustomizeState)0x11,
    (CustomizeState)0x11, (CustomizeState)0x11, (CustomizeState)0x11, (CustomizeState)1,
    (CustomizeState)0x17, (CustomizeState)0x17, (CustomizeState)0x17, (CustomizeState)0x17,
    (CustomizeState)0x17, (CustomizeState)3,    (CustomizeState)3,    (CustomizeState)1,
    (CustomizeState)0,    (CustomizeState)0x20, (CustomizeState)0x20, (CustomizeState)0x20,
    (CustomizeState)1,
};

CustomizePanel::CustomizePanel()
    : mCustomizeState(kCustomizeState_Invalid), mPendingState(kCustomizeState_Invalid),
      mPatchMenuReturnState(kCustomizeState_Invalid), mClosetMgr(0), mUser(0),
      mProfile(0), mCharData(0), mPreviewDesc(0), mNewAssetProvider(0),
      mCurrentOutfitProvider(0), mAssetProvider(0), mPremiumAssetProvider(0),
      mMakeupProvider(0), mInstrumentFinishProvider(0),
      mCurrentBoutique(kAssetBoutique_None), unk90(gNullStr), mCurrentMakeupIndex(-1),
      mRefreshingContent(0), mWaitingToLeave(0),
      mPatchCategory((BandCharDesc::Patch::Category)0), mPatchName(gNullStr) {}

CustomizePanel::~CustomizePanel() { mFocusComponents.clear(); }

void CustomizePanel::Load() {
    UIPanel::Load();
    mClosetMgr = ClosetMgr::GetClosetMgr();
    MILO_ASSERT(mClosetMgr, 0x82);
    mUser = mClosetMgr->GetUser();
    MILO_ASSERT(mUser, 0x85);
    mCharData = mUser->GetChar();
    MILO_ASSERT(mCharData, 0x88);
    if (PrefabMgr::PrefabIsCustomizable()) {
        mProfile = TheProfileMgr.GetProfileForUser(mUser);
    } else
        mProfile = mClosetMgr->GetProfile();
    mPreviewDesc = mClosetMgr->GetPreviewDesc();
    MILO_ASSERT(mPreviewDesc, 0x93);
    Symbol genderSym = mClosetMgr->GetGender();
    AssetGender assetGender = GetAssetGenderFromSymbol(genderSym);
    MILO_ASSERT(!mNewAssetProvider, 0x98);
    mNewAssetProvider = new NewAssetProvider(mProfile, assetGender);
    MILO_ASSERT(!mCurrentOutfitProvider, 0x9B);
    mCurrentOutfitProvider = new CurrentOutfitProvider();
    MILO_ASSERT(!mAssetProvider, 0x9E);
    mAssetProvider = new AssetProvider(mProfile, assetGender);
    MILO_ASSERT(!mMakeupProvider, 0xA6);
    mMakeupProvider = new MakeupProvider(genderSym);
    MILO_ASSERT(!mInstrumentFinishProvider, 0xA9);
    mInstrumentFinishProvider = new InstrumentFinishProvider();
    mUnlockedFacePaint = mProfile->HasCampaignKey(key_unlocked_face_paint);
    mUnlockedTattoos = mProfile->HasCampaignKey(key_unlocked_tattoos);
}

bool CustomizePanel::IsLoaded() const {
    return !UIPanel::IsLoaded() ? false : !TheContentMgr.RefreshInProgress();
}

void CustomizePanel::FinishLoad() {
    UIPanel::FinishLoad();
    if (mClosetMgr->GetGender() == female)
        DisableFaceHair();
    else
        EnableFaceHair();
}

void CustomizePanel::Enter() {
    UIPanel::Enter();
    mClosetMgr->PreviewCharacter(true, false);
    mRefreshingContent = TheContentMgr.RefreshInProgress();
    TheContentMgr.RegisterCallback(this, false);
    TheSessionMgr->AddSink(this, SigninChangedMsg::Type());
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW);
}

void CustomizePanel::Poll() {
    UIPanel::Poll();
    if (GetState() == kUp) {
        mClosetMgr->Poll();
    }
}

void CustomizePanel::Exit() {
    UIPanel::Exit();
    TheContentMgr.UnregisterCallback(this, true);
    TheSessionMgr->RemoveSink(this, SigninChangedMsg::Type());
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_AUTO);
}

void CustomizePanel::Unload() {
    UIPanel::Unload();
    SetCustomizeState(kCustomizeState_Invalid);
    mUser->UpdateData(2);
    MILO_ASSERT(mClosetMgr, 0x10A);
    MILO_ASSERT(mUser, 0x10B);
    MILO_ASSERT(mProfile, 0x10C);
    MILO_ASSERT(mCharData, 0x10D);
    MILO_ASSERT(mPreviewDesc, 0x10E);
    mClosetMgr->PreviewCharacter(false, false);
    mClosetMgr = nullptr;
    mUser = nullptr;
    mProfile = nullptr;
    mCharData = nullptr;
    mPreviewDesc = nullptr;
    RELEASE(mInstrumentFinishProvider);
    RELEASE(mMakeupProvider);
    RELEASE(mPremiumAssetProvider);
    RELEASE(mAssetProvider);
    RELEASE(mCurrentOutfitProvider);
    RELEASE(mNewAssetProvider);
}

bool CustomizePanel::Unloading() const { return !TheNetCacheMgr->IsUnloaded(); }
void CustomizePanel::ContentStarted() { mRefreshingContent = true; }
void CustomizePanel::ContentDone() { mRefreshingContent = false; }

void CustomizePanel::SetCustomizeState(CustomizeState state) {
    static Message msg("update_state", 0, 0);
    msg[0] = state;
    msg[1] = mCustomizeState;
    mCustomizeState = state;
    HandleType(msg);
}

void CustomizePanel::SetPendingState(CustomizeState state) { mPendingState = state; }
void CustomizePanel::SetPatchMenuReturnState(CustomizeState state) {
    mPatchMenuReturnState = state;
}

bool CustomizePanel::InPreviewState() const {
    switch (mCustomizeState) {
    case 2:
    case kCustomizeState_BrowseTorso:
    case kCustomizeState_BrowseLegs:
    case kCustomizeState_BrowseFeet:
    case kCustomizeState_BrowseHats:
    case kCustomizeState_BrowseEarrings:
    case kCustomizeState_BrowsePiercings:
    case kCustomizeState_BrowseGlassesAndMasks:
    case kCustomizeState_BrowseBandanas:
    case kCustomizeState_BrowseWrists:
    case kCustomizeState_BrowseRings:
    case kCustomizeState_BrowseGloves:
    case kCustomizeState_BrowseHair:
    case kCustomizeState_BrowseEyebrows:
    case kCustomizeState_BrowseFaceHair:
    case kCustomizeState_BrowseEyeMakeup:
    case kCustomizeState_BrowseLipMakeup:
    case kCustomizeState_BrowseGuitars:
    case kCustomizeState_BrowseBasses:
    case kCustomizeState_BrowseDrums:
    case kCustomizeState_BrowseMicrophones:
    case kCustomizeState_BrowseKeyboards:
    case 0x1d:
    case 0x1e:
    case 0x24:
        return true;
    default:
        return false;
    }
}

// RB3-360 retail (lane RESIDUAL-2, 2026-08-14): the `in_clothing_state` arm of
// Handle() calls a NON-MEMBER, not the `InClothingState()` const member that
// rb3-Wii spells there.  Evidence is retail bytes via the dead-`this`-home
// oracle: MSVC /O1 homes the vbase-adjusted `this` of an INLINED MEMBER call
// into a dead stack slot, and retail has that home at 3 of the 4 inlined member
// calls in Handle -- every one except this arm.  With the member form our build
// emitted two extra instructions there (`subi r10,r26,0xb8` / `stw r10,0x94(r31)`,
// the dead home); routing the arm through this file-static helper drops both and
// keeps retail's trailing bool mask and store order, closing the site exactly.
// (`const` and an in-class definition were both tried by lane RESIDUAL-1 and
// changed nothing -- only removing the `this` parameter works.)
static bool IsClothingState(CustomizePanel::CustomizeState s) {
    return s >= CustomizePanel::kCustomizeState_BrowseTorso
        && s <= CustomizePanel::kCustomizeState_BrowseFeet;
}

bool CustomizePanel::InClothingState() const { return IsClothingState(mCustomizeState); }

void CustomizePanel::UpdateNewAssetProvider() {
    mNewAssetProvider->Update();
    RefreshNewAssetsList();
}

void CustomizePanel::UpdateCurrentOutfitProvider() {
    mCurrentOutfitProvider->Update();
    RefreshCurrentOutfitList();
}

void CustomizePanel::UpdateAssetProvider() {
    AssetType ty = GetAssetTypeFromCurrentState();
    if (ty != kAssetType_None) {
        mAssetProvider->Update(ty, mCurrentBoutique);
        RefreshAssetsList();
    }
}

// RB3-360 residual note (CustomizePanel::Handle stalls at 99.0): retail's
// update_makeup_provider arm calls this outlined (bl fn_825F85A0) where ours
// inlines it. Forcing __declspec(noinline) here DOES outline it but cascades
// into a whole-function layout/regalloc reshuffle (99.0 -> 72.0) — reverted.
// ⚠⚠ TOOLING TRAP THIS FUNCTION DEMONSTRATED (lane DQ-1) — READ BEFORE YOU
// REVERT ANYTHING HERE ON A SCORE DROP.  objdiff has a fast path in
// objdiff-core/src/diff/code.rs:647 `diff_instructions()`:
//
//     // Fast path: if same length, pair instructions 1:1 without running the
//     // diff algorithm.  This is valid because same-length sequences have no
//     // insertions/deletions ...
//
// That justification is FALSE: a sequence with N insertions AND N deletions is
// the same length.  Mid-lane this function was exactly that — 5 inserts and 5
// deletes, base 1259 == target 1259 — and objdiff scored it **72.2%** when the
// true content was ~99%: identical instructions, merely shifted +2 then -3 and
// back into sync.  Three consecutive measurements make the mechanism plain:
//     base 1253 vs target 1259 (unequal) -> proper diff, 98.7%
//     base 1259 vs target 1259 (EQUAL)   -> 1:1 fast path, 578 'replace', 72.2%
//     base 1261 vs target 1259 (unequal) -> proper diff, 99.8%
// So GETTING THE SIZE RIGHT CAN MAKE THE SCORE COLLAPSE, and the collapse looks
// exactly like a catastrophic regression.  It nearly caused the (correct)
// save_prefab fix below to be reverted.  The artifact is one-directional and
// cannot fabricate a false 100% — a 1:1 pairing of differing instructions still
// counts them as mismatched — so `matched_functions` is safe; only sub-100
// scores are distorted.  Diagnose by dumping the row list and looking for a
// constant shift, never by trusting the percentage.
//
// Handle residual status after lane DQ-1 (99.8% normalized, 3 of 1261
// instructions):  the inverted has_license/has_patch cross-jump is FIXED (see
// HasLicense below).  What is left is (a) the same this-spill-vs-remat, 2
// base-only instructions `subi r10,r26,0xb8 / stw r10,0x94(r31)` emitted inside
// the in_clothing_state arm, and (b) one target-only `clrlwi r11,r11,24` at
// 0x82619818, the last instruction of the shared has_license/has_patch bool
// tail.  For (b): making HasPatch return int (to force a DataNode(int) overload
// and with it a bool->int mask) was TRIED and changed NOTHING -- do not re-fund
// that idea.  For (a): the slot 0x94(r31) is the setup_asset_patch_data Symbol
// temp, and in our build nothing ever reloads `this` from it.
//
// ── lane RESIDUAL-1 (2026-08-14): SECOND DOCUMENTED NEGATIVE.  DO NOT REOPEN
//    THIS ROW FOR THE BYTES.  Read the next paragraph before anything else.
//
// ⛔⛔ THE "5,036 B BEHIND 3 MISMATCHES" FRAMING IS WRONG, AND THE ERROR IS AN
// INSTRUMENT ERROR.  `run_objdiff` reports on the `none` ruler, which is
// STRUCTURALLY BLIND to relocation-NAME charges.  `report.json` — the graded
// ruler — has shipped `functionRelocDiffs=name_check` since d04c83df, and on it
// this row has FIVE charged sites, not three:
//     3x insert/delete  (the two below), plus
//     2x diff_arg from ICF fold-aliases:
//       [ 330] tgt ??A?$hash_map@HPAVSongUpgradeData@@...  vs
//              base ??A?$hash_map@HPAVUIComponent@@...
//       [1112] tgt ?RemoveCPPT@CCFGLM@NUISPEECH@@AAAXPAVCPPT@2@G@Z  vs
//              base ?TakePortrait@ClosetMgr@@QAAXXZ
// Measured on the graded ruler: fuzzy 99.75378 / mpn 99.76172.  `mpn` excludes
// arg penalties, `fuzzy` includes them.  ⇒ CLOSING ALL THREE INSTRUCTIONS BUYS
// mpn == 100 (+1 matched_function) AND ZERO BYTES, because `matched_code` is
// all-or-nothing on `fuzzy == 100`.  The row would simply join the documented
// "mpn==100, fuzzy<100" population.  The 5,036 B additionally requires BOTH ICF
// aliases to be proven on retail bytes and added to icf_aliases.map — and an
// UNPROVEN alias lifts the score by construction (integrity hazard).
//
// (a) is now DIAGNOSED, not guessed.  cl.exe /FAs names the slot:
//         stw r10,$T266039(r31)   ; 148 (94h)      <- src line 1178
// It is a COMPILER TEMPORARY holding the vbase-adjusted `this` (r26-0xb8),
// created by the INLINED MEMBER CALL, and it is dead.  Control, with an
// untreated population: in our build 4 of 4 inlined member calls in Handle
// home `this` (in_clothing_state, set_current_character_patch,
// refresh_patch_edit, has_patch); RETAIL HAS THE HOME IN 3 OF THOSE 4 —
// has_patch's is `subi r11,r26,0xb8 / stw r11,0x78(r31)`, equally dead.  Only
// in_clothing_state lacks it.  ⇒ retail did NOT inline a member call of `this`
// there.  But retail DOES have the trailing `clrlwi` that ONLY the call form
// produces (replacing the call with the raw `mCustomizeState >= .. && <= ..`
// comparison drops the mask AND swaps retail's stw order).  ⇒ retail inlined a
// bool-returning NON-MEMBER.  No plausible source shape for that was found —
// both call sites and rb3-Wii spell it as a const member — so nothing was
// changed.  RULED OUT by compiling and reading the listing, not by argument:
//     * `const` on InClothingState()          -> home still emitted (identical)
//     * in-class (header) definition instead of out-of-line -> home still emitted
//
// (b) RULED OUT, new: moving the `!= 0` INSIDE a bool-returning HasLicense
// (which is what rb3-Wii's arm shape implies, since its arm has no `!= 0`)
// changes NOTHING — MSVC emits no truncation at a bool return boundary when the
// value came from a comparison.  A scan of the entire TU's /FAs listing finds
// ZERO occurrences of `subfe` followed by `clrlwi ,24` anywhere, i.e. no
// construct in this file reproduces retail's shape.
//
// ── lane RESIDUAL-2 (2026-08-14): (a) is CLOSED; only (b) remains.  Re-priced
//    first: FOLDPROVE-1/2 landed both ICF aliases, so on the graded ruler this
//    row is now 5,036 B / fuzzy == mpn == 99.76172 with THREE charged sites and
//    ZERO diff_arg -- i.e. RESIDUAL-1's "buys zero bytes" is stale (it was true
//    only while the aliases still charged the row).  ⇒ a retirement is valid
//    only on the tree it was measured on.
//
// (a) CLOSED by the oracle below: routing the arm through the file-static
//     IsClothingState() drops both dead-home instructions with nothing else
//     moving (1258 equal / 2 insert -> 1258 equal / 0 insert).  The MECHANISM is
//     retail-byte evidence; the SPELLING (a file-static delegate) is a choice --
//     any callee without a `this` parameter would do.
//
// (b) STILL OPEN, and now characterised rather than merely undiagnosed.  The
//     old note "no construct in this file reproduces it" was a TU-LOCAL scan and
//     is too weak a claim: a binary-wide scan of every target .s (keyed on the
//     `.fn` symbol, NOT the synthetic address column) finds `subfe` followed by
//     `clrlwi rX,rX,24` at exactly **12 sites**, and we already match FOUR of
//     them 100% -- BandUI::OnMsg(ContentReadFailureMsg), DataExists,
//     MetaPerformer::Handle, ModifierMgr::Handle.  So the construct IS
//     reproducible under our compiler; it is this ARM that resists.
//     ★ THE RULE, read off those controls: the mask is emitted ONLY AT A PHI.
//     In TourProgress, DataFunc and MetaPerformer the `clrlwi` is literally a
//     BRANCH TARGET (a `.L_` label sits immediately before it), merging an
//     early-out `false` path with the computed path; in ModifierMgr it is an
//     if/else-if/else that MSVC collapsed; at our own in_clothing_state arm it
//     is the `&&` short-circuit.  RETAIL'S CustomizePanel SITE HAS NO LABEL
//     THERE (the only label, .L_82619778, is on the `subic` -- that is the
//     has_patch cross-jump, which is BEFORE the subfe), so the mask sits on a
//     straight-line subfe, which is the one shape no source form reproduces.
//     ⛔ AND NO CAST CAN REINTRODUCE IT: MSVC range-analyses the `subfe` result
//     as 0/1 and elides every narrowing -- even an explicit `(unsigned char)`
//     cast measured INERT.  That is why arm-expression work is hopeless here.
//     Measured inert this lane (all leave 1258 equal / 1 delete): arm `!= 0`
//     (baseline), `? true : false`, `!!`, `DataNode(...)`, `(unsigned char)`,
//     an extra `bool`-parameter boundary, `bool HasLicense` + arm `!= 0`,
//     `bool HasLicense{ return X != 0; }` + bare arm (RESIDUAL-1's, reproduced
//     independently), `bool HasLicense{ int r = X; return r; }`.
//     Measured WORSE (MSVC emits branches instead of collapsing): an if/else
//     body on HasPatch (+5 insert), an if/else int->bool helper (+4 insert),
//     `return (int)TheSongMgr.HasLicense(s)` (+3 insert / +5 delete).
//     DIAGNOSTIC (do not redo): breaking the has_license/has_patch cross-jump
//     shows the STANDALONE has_patch arm also emits no mask -- so neither arm
//     owns it in our source, and it is not a merge artefact of ours.
//     ⇒ Until someone finds a source shape that puts a phi on the far side of
//     that subfe, the row cannot cross, and closing (a) alone buys EXACTLY ZERO
//     bytes (whole-binary A/B this lane: Dmatched +0, Dcode_bytes +0,
//     Dcode% +0.000000pp, Dfuzzy +0.000080pp, 0 units off 100%).
//
// ★ REUSABLE INSTRUMENT FOUND HERE (the dead store is a source-shape oracle):
// MSVC /O1 creates a dead stack home for the vbase-adjusted `this` of an
// INLINED MEMBER call.  Presence/absence of that dead home therefore witnesses
// whether retail's source called a MEMBER or a NON-MEMBER at that site — a
// source-level fact no source diff and no oracle can see.  Pair it with the
// trailing `clrlwi`, which witnesses that an inlined callee RETURNED bool.
void CustomizePanel::UpdateMakeupProvider(Symbol type) {
    // retail fn_82614E60 is 0x80 bytes and is CALLED (not inlined) from the
    // update_makeup_provider arm. Its body is: two guarded function-local
    // static Symbol inits ("eyes" @0x82011054, "lips" @0x820C31C0) whose
    // *initializers* survive even though the MILO_ASSERT that consumed them is
    // compiled out — that bulk is what keeps MSVC from inlining it. Declaring
    // them locally reproduces retail's size naturally, instead of forcing
    // __declspec(noinline) (which a previous lane found reshuffles the whole
    // function).
    static Symbol eyes("eyes");
    static Symbol lips("lips");
    MILO_ASSERT(type == eyes || type == lips, 0x1A9);
    mMakeupProvider->Update(type);
}

void CustomizePanel::SetCurrentBoutique(Symbol s) {
    mCurrentBoutique = GetAssetBoutiqueFromSymbol(s);
    UpdateAssetProvider();
}

Symbol CustomizePanel::GetCurrentBoutique() {
    return GetSymbolFromAssetBoutique(mCurrentBoutique);
}

void CustomizePanel::ClearCurrentBoutique() { mCurrentBoutique = kAssetBoutique_None; }

Symbol CustomizePanel::GetWearing() {
    BandCharDesc *desc = mPreviewDesc;
    AssetType ty = GetAssetTypeFromCurrentState();
    Symbol ret(gNullStr);
    switch (ty) {
    case kAssetType_None:
    case kAssetType_Feet:
    case kAssetType_Legs:
    case kAssetType_Torso:
        break;
    case kAssetType_Bandana:
        ret = desc->mOutfit.mFaceHair.mName;
        break;
    case kAssetType_Bass:
        ret = StripFinish(desc->mInstruments.mBass.mName);
        break;
    case kAssetType_Drum:
        ret = StripFinish(desc->mInstruments.mDrum.mName);
        break;
    case kAssetType_Earrings:
        ret = desc->mOutfit.mEarrings.mName;
        break;
    case kAssetType_Eyebrows:
        ret = desc->mOutfit.mEyebrows.mName;
        break;
    case kAssetType_FaceHair:
        ret = desc->mOutfit.mFaceHair.mName;
        break;
    case kAssetType_GlassesAndMasks:
        ret = desc->mOutfit.mGlasses.mName;
        break;
    case kAssetType_Gloves:
        ret = desc->mOutfit.mHands.mName;
        break;
    case kAssetType_Guitar:
        ret = StripFinish(desc->mInstruments.mGuitar.mName);
        break;
    case kAssetType_Hair:
        ret = desc->mOutfit.mHair.mName;
        break;
    case kAssetType_Hat:
        ret = desc->mOutfit.mHair.mName;
        break;
    case kAssetType_Keyboard:
        ret = desc->mInstruments.mKeyboard.mName;
        break;
    case kAssetType_Mic:
        ret = desc->mInstruments.mMic.mName;
        break;
    case kAssetType_Piercings:
        ret = desc->mOutfit.mPiercings.mName;
        break;
    case kAssetType_Rings:
        ret = desc->mOutfit.mRings.mName;
        break;
    case kAssetType_Wrists:
        ret = desc->mOutfit.mWrist.mName;
        break;
    default:
        MILO_ASSERT(false, 0x20D);
        break;
    }
    if (ret == gNullStr) {
        ret = GetDefaultAssetFromAssetType(
            ty, GetAssetGenderFromSymbol(mClosetMgr->GetGender())
        );
    }
    return ret;
}

Symbol CustomizePanel::StripFinish(Symbol s) {
    AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
    MILO_ASSERT(pAssetMgr, 0x21F);
    return pAssetMgr->StripFinish(s);
}

void CustomizePanel::RefreshNewAssetsList() { Handle(refresh_new_assets_list_msg, true); }
void CustomizePanel::RefreshAssetsList() { Handle(refresh_assets_list_msg, true); }
void CustomizePanel::RefreshCurrentOutfitList() {
    Handle(refresh_current_outfit_list_msg, true);
}

void CustomizePanel::PreviewAsset(Symbol s) {
    if (InPreviewState()) {
        BandCharDesc *desc = mPreviewDesc;
        AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
        MILO_ASSERT(pAssetMgr, 0x255);
        AssetType ty;
        if (s == none_bandana || s == none_earrings || s == none_eyebrows
            || s == none_facehair || s == none_glasses || s == none_hair || s == none_hat
            || s == none_piercings || s == none_rings || s == none_wrists) {
            ty = pAssetMgr->GetTypeFromName(s);
            s = gNullStr;
        } else {
            Asset *pAsset = pAssetMgr->GetAsset(s);
            MILO_ASSERT(pAsset, 0x269);
            BandProfile *p = mProfile;
            p->mProfileAssets.SetOld(s);
            ty = pAsset->GetType();
            if (pAsset->HasFinishes()) {
                s = MakeString("%s_%s", s.Str(), pAsset->GetFinish(0).Str());
            }
        }
        Symbol ret = none;
        switch (ty) {
        case kAssetType_None:
            break;
        case kAssetType_Bandana:
            desc->mOutfit.mFaceHair.mName = s;
            ret = facehair;
            break;
        case kAssetType_Bass:
            desc->mInstruments.mBass.mName = s;
            ret = bass;
            break;
        case kAssetType_Drum:
            desc->mInstruments.mDrum.mName = s;
            ret = drum;
            break;
        case kAssetType_Earrings:
            desc->mOutfit.mEarrings.mName = s;
            ret = earrings;
            break;
        case kAssetType_Eyebrows:
            desc->mOutfit.mEyebrows.mName = s;
            ret = eyebrows;
            break;
        case kAssetType_FaceHair:
            desc->mOutfit.mFaceHair.mName = s;
            ret = facehair;
            break;
        case kAssetType_Feet:
            desc->mOutfit.mFeet.mName = s;
            ret = feet;
            break;
        case kAssetType_GlassesAndMasks:
            desc->mOutfit.mGlasses.mName = s;
            ret = glasses;
            break;
        case kAssetType_Gloves:
            desc->mOutfit.mHands.mName = s;
            ret = hands;
            break;
        case kAssetType_Guitar:
            desc->mInstruments.mGuitar.mName = s;
            ret = guitar;
            break;
        case kAssetType_Hair:
            desc->mOutfit.mHair.mName = s;
            ret = hair;
            break;
        case kAssetType_Hat:
            desc->mOutfit.mHair.mName = s;
            ret = hair;
            break;
        case kAssetType_Keyboard:
            desc->mInstruments.mKeyboard.mName = s;
            ret = keyboard;
            break;
        case kAssetType_Legs:
            desc->mOutfit.mLegs.mName = s;
            ret = legs;
            break;
        case kAssetType_Mic:
            desc->mInstruments.mMic.mName = s;
            ret = mic;
            break;
        case kAssetType_Piercings:
            desc->mOutfit.mPiercings.mName = s;
            ret = piercings;
            break;
        case kAssetType_Rings:
            desc->mOutfit.mRings.mName = s;
            ret = rings;
            break;
        case kAssetType_Torso:
            desc->mOutfit.mTorso.mName = s;
            ret = torso;
            break;
        case kAssetType_Wrists:
            desc->mOutfit.mWrist.mName = s;
            ret = wrist;
            break;
        default:
            MILO_ASSERT(false, 0x304);
            break;
        }
        mClosetMgr->SetCurrentOutfitPiece(ret);
        if (IsInstrumentAssetType(ret)) {
            mClosetMgr->SetInstrumentType(ret);
        }
        mClosetMgr->PreviewCharacter(true, false);
    }
}

void CustomizePanel::PreviewFinish(Symbol s) {
    BandCharDesc *desc = mPreviewDesc;
    BandCharDesc::OutfitPiece *pOutfitPiece = mClosetMgr->GetCurrentOutfitPiece();
    MILO_ASSERT(pOutfitPiece, 0x318);
    Symbol outfitName = pOutfitPiece->mName;
    AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
    MILO_ASSERT(pAssetMgr, 0x31C);
    Symbol stripped = pAssetMgr->StripFinish(outfitName);
    Asset *pAsset = pAssetMgr->GetAsset(stripped);
    MILO_ASSERT(pAsset, 800);
    Symbol s1c = MakeString("%s_%s", stripped.Str(), s.Str());
    Symbol outfit = none;
    switch (pAsset->GetType()) {
    case kAssetType_Guitar:
        desc->mInstruments.mGuitar.mName = s1c;
        outfit = guitar;
        break;
    case kAssetType_Bass:
        desc->mInstruments.mBass.mName = s1c;
        outfit = bass;
        break;
    case kAssetType_Drum:
        desc->mInstruments.mDrum.mName = s1c;
        outfit = drum;
        break;
    default:
        break;
    }
    mClosetMgr->SetCurrentOutfitPiece(outfit);
    mClosetMgr->PreviewCharacter(true, false);
}

void CustomizePanel::SelectAsset(Symbol s) {
    if (!mClosetMgr->IsCharacterLoading()) {
        if (s == none_bandana || s == none_earrings || s == none_eyebrows
            || s == none_facehair || s == none_glasses || s == none_hair || s == none_hat
            || s == none_piercings || s == none_rings || s == none_wrists) {
            mClosetMgr->FinalizeChanges(true, InClothingState());
            LeaveState(false);
        } else {
            AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
            MILO_ASSERT(pAssetMgr, 0x36A);
            Asset *pAsset = pAssetMgr->GetAsset(s);
            MILO_ASSERT(pAsset, 0x36D);
            BandProfile *p = mProfile;
            if (p->mProfileAssets.HasAsset(s)) {
                if (pAsset->HasFinishes()) {
                    mInstrumentFinishProvider->Update(s);
                    ChooseFinish();
                } else if (pAsset->GetBoutique() != kAssetBoutique_Premium) {
                    ChooseColors();
                }
            } else
                ShowLockedDialog();
        }
    }
}

void CustomizePanel::ShowLockedDialog() { Handle(show_locked_dialog_msg, true); }
void CustomizePanel::ChooseFinish() { Handle(choose_finish_msg, true); }
void CustomizePanel::ChooseColors() { Handle(choose_colors_msg, true); }
void CustomizePanel::GotoCustomizeClothingScreen() {
    Handle(goto_customize_clothing_screen_msg, true);
}

Symbol CustomizePanel::GetCurrentMakeup(Symbol type) {
    MILO_ASSERT(type == eyes || type == lips, 0x3A9);
    BandCharDesc *desc = mPreviewDesc;
    for (int i = 0; i < desc->mPatches.size(); i++) {
        BandCharDesc::Patch &curPatch = desc->mPatches[i];
        if (curPatch.mCategory == BandCharDesc::Patch::kPatchMakeup) {
            String meshName = curPatch.mMeshName;
            std::vector<String> subStrings;
            Symbol s6;
            if (meshName.split("_", subStrings) == 4) {
                s6 = subStrings[2].c_str();
            } else
                MILO_WARN("Invalid makeup mesh: (%s)", meshName);
            if (s6 == type) {
                SetCurrentMakeupIndex(i);
                String result = meshName.substr(0, meshName.length() - 5);
                Symbol sym = result.c_str();
                return sym;
            }
        }
    }
    ClearCurrentMakeupIndex();
    return gNullStr;
}

void CustomizePanel::SetCurrentMakeupIndex(int idx) { mCurrentMakeupIndex = idx; }
void CustomizePanel::ClearCurrentMakeupIndex() { mCurrentMakeupIndex = -1; }

void CustomizePanel::PreviewMakeup(Symbol s) {
    if (mCustomizeState != kCustomizeState_BrowseEyeMakeup
        && mCustomizeState != kCustomizeState_BrowseLipMakeup)
        return;
    else {
        static Symbol none_makeup("none_makeup");
        std::vector<BandCharDesc::Patch> &rPatches = mPreviewDesc->mPatches;
        if (s == none_makeup) {
            if (mCurrentMakeupIndex != -1) {
                MILO_ASSERT_RANGE(mCurrentMakeupIndex, 0, rPatches.size(), 0x3F3);
                std::vector<BandCharDesc::Patch>::iterator it = rPatches.begin() + mCurrentMakeupIndex;
                rPatches.erase(it);
                ClearCurrentMakeupIndex();
            }
        } else {
            String meshName = MakeString("%s.mesh", s.Str());
            if (mCurrentMakeupIndex == -1) {
                BandCharDesc::Patch patch;
                patch.mTexture = -1;
                patch.mCategory = BandCharDesc::Patch::kPatchMakeup;
                patch.mMeshName = meshName;
                rPatches.push_back(patch);
                mCurrentMakeupIndex = rPatches.size() - 1;
            } else
                rPatches[mCurrentMakeupIndex].mMeshName = meshName;
        }
        mClosetMgr->RecomposePatches(0x20);
    }
}

bool CustomizePanel::HasNewAssets() {
    return mProfile->mProfileAssets.GetNumNewAssets(
               GetAssetGenderFromSymbol(mClosetMgr->GetGender())
           )
        > 0;
}

bool CustomizePanel::AssetProviderHasAsset(Symbol s) {
    AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
    MILO_ASSERT(pAssetMgr, 0x42B);
    AssetType nameType = pAssetMgr->GetTypeFromName(s);
    if (nameType != GetAssetTypeFromCurrentState()) {
        return false;
    } else
        return mAssetProvider->HasAsset(s);
}

void CustomizePanel::SetupCurrentOutfit(Symbol s) {
    AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
    MILO_ASSERT(pAssetMgr, 0x43D);
    if (pAssetMgr->HasAsset(s)) {
        Asset *pAsset = pAssetMgr->GetAsset(s);
        MILO_ASSERT(pAsset, 0x443);
        Symbol assetSym = GetSymbolFromAssetType(pAsset->GetType());
        if (assetSym == bandana) {
            assetSym = facehair;
        } else if (assetSym == hat) {
            assetSym = hair;
        }
        mClosetMgr->SetCurrentOutfitPiece(assetSym);
    }
}

bool CustomizePanel::HasPatch() {
    return mPreviewDesc->FindPatchIndex(
               (BandCharDesc::Patch::Category)mPatchCategory, mPatchName.c_str()
           )
        != -1;
}

void CustomizePanel::EnableFaceHair() { Handle(enable_facehair_msg, true); }
void CustomizePanel::DisableFaceHair() { Handle(disable_facehair_msg, true); }
// RB3-360 retail (lane DQ-1): the return type here is `int`, not `bool`, and the
// has_license arm spells the test out as `... != 0`.  That is not cosmetic — it
// is what produces retail's four-instruction bool chain at 0x82619808:
//     clrlwi r11, r3, 24     <- bool (BandSongMgr::HasLicense) -> int
//     subic  r10, r11, 0x1   <- int -> bool  (`!= 0`)
//     subfe  r11, r10, r11
//     clrlwi r11, r11, 24
// and, crucially, it is what makes MSVC cross-jump has_patch BACKWARD into this
// arm's tail (has_patch enters at the `subic`, its own `FindPatchIndex()+1`
// already in r11).  With `bool` + no `!= 0` this arm emitted NO normalisation at
// all, so there was nothing to merge into and MSVC merged the other direction —
// has_license jumping forward into has_patch.  Restoring the int/`!= 0` form
// removed 9 of the 12 remaining mismatches in CustomizePanel::Handle.
int CustomizePanel::HasLicense(Symbol s) { return TheSongMgr.HasLicense(s); }

Symbol CustomizePanel::GetAssetShot(Symbol s) {
    AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
    MILO_ASSERT(pAssetMgr, 0x4B8);
    AssetType ty = pAssetMgr->GetTypeFromName(s);
    const char *shotstr = gNullStr;
    switch (ty) {
    case kAssetType_None:
        MILO_ASSERT(false, 0x4C0);
        break;
    case kAssetType_Bandana:
        shotstr = "head";
        break;
    case kAssetType_Bass:
        shotstr = "guitar";
        break;
    case kAssetType_Drum:
        shotstr = "drums";
        break;
    case kAssetType_Earrings:
        shotstr = "head";
        break;
    case kAssetType_Eyebrows:
        shotstr = "head";
        break;
    case kAssetType_FaceHair:
        shotstr = "head";
        break;
    case kAssetType_Feet:
        shotstr = "feet";
        break;
    case kAssetType_GlassesAndMasks:
        shotstr = "head";
        break;
    case kAssetType_Gloves:
        shotstr = "gloves";
        break;
    case kAssetType_Guitar:
        shotstr = "guitar";
        break;
    case kAssetType_Hair:
        shotstr = "head";
        break;
    case kAssetType_Hat:
        shotstr = "head";
        break;
    case kAssetType_Keyboard:
        shotstr = "keyboard";
        break;
    case kAssetType_Legs:
        shotstr = "legs";
        break;
    case kAssetType_Mic:
        shotstr = "microphone";
        break;
    case kAssetType_Piercings:
        shotstr = "head";
        break;
    case kAssetType_Rings:
        shotstr = "rings";
        break;
    case kAssetType_Torso:
        shotstr = "torso";
        break;
    case kAssetType_Wrists:
        shotstr = "wrists";
        break;
    default:
        MILO_ASSERT(false, 0x4FC);
        break;
    }
    return MakeString("%s_1.shot", shotstr);
}

AssetType CustomizePanel::GetAssetTypeFromCurrentState() {
    switch (mCustomizeState) {
    case kCustomizeState_Invalid:
        return kAssetType_None;
    case 1:
        return kAssetType_None;
    case 2:
        return kAssetType_None;
    case 3:
        return kAssetType_None;
    case 4:
        return kAssetType_None;
    case kCustomizeState_BrowseTorso:
        return kAssetType_Torso;
    case kCustomizeState_BrowseLegs:
        return kAssetType_Legs;
    case kCustomizeState_BrowseFeet:
        return kAssetType_Feet;
    case 8:
        return kAssetType_None;
    case kCustomizeState_BrowseHats:
        return kAssetType_Hat;
    case kCustomizeState_BrowseEarrings:
        return kAssetType_Earrings;
    case kCustomizeState_BrowsePiercings:
        return kAssetType_Piercings;
    case kCustomizeState_BrowseGlassesAndMasks:
        return kAssetType_GlassesAndMasks;
    case kCustomizeState_BrowseBandanas:
        return kAssetType_Bandana;
    case kCustomizeState_BrowseWrists:
        return kAssetType_Wrists;
    case kCustomizeState_BrowseRings:
        return kAssetType_Rings;
    case kCustomizeState_BrowseGloves:
        return kAssetType_Gloves;
    case kCustomizeState_HairAndMakeup:
        return kAssetType_None;
    case kCustomizeState_BrowseHair:
        return kAssetType_Hair;
    case kCustomizeState_BrowseEyebrows:
        return kAssetType_Eyebrows;
    case kCustomizeState_BrowseFaceHair:
        return kAssetType_FaceHair;
    case kCustomizeState_BrowseEyeMakeup:
        return kAssetType_None;
    case kCustomizeState_BrowseLipMakeup:
        return kAssetType_None;
    case kCustomizeState_Instruments:
        return kAssetType_None;
    case kCustomizeState_BrowseGuitars:
        return kAssetType_Guitar;
    case kCustomizeState_BrowseBasses:
        return kAssetType_Bass;
    case kCustomizeState_BrowseDrums:
        return kAssetType_Drum;
    case kCustomizeState_BrowseMicrophones:
        return kAssetType_Mic;
    case kCustomizeState_BrowseKeyboards:
        return kAssetType_Keyboard;
    case 29:
        return kAssetType_Torso;
    case 30:
        return kAssetType_Torso;
    case 31:
        return kAssetType_None;
    default:
        return kAssetType_None;
    }
}

void CustomizePanel::SetFocusComponent(CustomizeState state, Symbol sym) {
    UIComponent *pComponent = mDir->Find<UIComponent>(sym.Str(), true);
    MILO_ASSERT(pComponent, 0x574);
    mFocusComponents[state] = pComponent;
}

void CustomizePanel::StoreFocusComponent() {
    UIComponent *pFocusComponent = FocusComponent();
    MILO_ASSERT(pFocusComponent, 0x57E);
    mFocusComponents[mCustomizeState] = pFocusComponent;
}

UIComponent *CustomizePanel::GetFocusComponent() {
    return mFocusComponents[mCustomizeState];
}

DataNode CustomizePanel::OnMsg(const SigninChangedMsg &msg) {
    if (mProfile) {
        if (!mProfile->HasValidSaveData()) {
            static Symbol sign_out("sign_out");
            static Message init("init", 0);
            init[0] = 4;
            TheUIEventMgr->TriggerEvent(sign_out, init);
        }
    }
    return 1;
}

DataNode CustomizePanel::OnMsg(const ButtonDownMsg &msg) {
    if (mWaitingToLeave)
        return 1;
    if (mPendingState != 0)
        return 1;
    JoypadAction action = msg.GetAction();
    if (action == kAction_Cancel)
        return LeaveState(true);
    else if (mCustomizeState == 0x21) {
        switch (action) {
        case kAction_Up:
            MovePatch(0, -0.05f);
            break;
        case kAction_Right:
            MovePatch(0.05f, 0);
            break;
        case kAction_Down:
            MovePatch(0, 0.05f);
            break;
        case kAction_Left:
            MovePatch(-0.05f, 0);
            break;
        default:
            break;
        }
    } else if (mCustomizeState == 0x22) {
        switch (action) {
        case kAction_Right:
            RotatePatch(-10);
            break;
        case kAction_Left:
            RotatePatch(10);
            break;
        default:
            break;
        }
    } else if (mCustomizeState == 0x23) {
        switch (action) {
        case kAction_Up:
            ScalePatch(0, 0.05f);
            break;
        case kAction_Right:
            ScalePatch(0.05f, 0);
            break;
        case kAction_Down:
            ScalePatch(0, -0.05f);
            break;
        case kAction_Left:
            ScalePatch(-0.05f, 0);
            break;
        default:
            break;
        }
    }
    return DataNode(kDataUnhandled, 0);
}

DataNode CustomizePanel::OnMsg(const UIComponentScrollMsg &) {
    if (mPendingState != 0)
        return 1;
    else
        return DataNode(kDataUnhandled, 0);
}

DataNode CustomizePanel::LeaveState(bool b1) {
    if (b1) {
        static Message cMsg("handle_sound_back", 0);
        cMsg[0] = mUser;
        Handle(cMsg, true);
    }
    if (mCustomizeState == kCustomizeState_BrowseTorso
        && mCurrentBoutique == kAssetBoutique_TShirts) {
        mClosetMgr->ResetCharacterPreview();
        SetPendingState((CustomizeState)3);
        return 1;
    } else
        switch (mCustomizeState) {
        case 1:
            return LeaveCustomizePanel();
        case 2:
            mClosetMgr->ClearInstrument();
            mClosetMgr->ResetCharacterPreview();
            SetPendingState((CustomizeState)1);
            return 1;
        case kCustomizeState_HairAndMakeup:
            GotoCustomizeClothingScreen();
            return 1;
        case kCustomizeState_BrowseEyeMakeup:
        case kCustomizeState_BrowseLipMakeup:
            ClearCurrentMakeupIndex();
            mClosetMgr->ResetPatches();
            mClosetMgr->RecomposePatches(0x20);
            SetCustomizeState(kCustomizeState_HairAndMakeup);
            return 1;
        case kCustomizeState_Instruments:
            GotoCustomizeClothingScreen();
            return 1;
        case kCustomizeState_BrowseGuitars:
        case kCustomizeState_BrowseBasses:
        case kCustomizeState_BrowseDrums:
        case kCustomizeState_BrowseMicrophones:
        case kCustomizeState_BrowseKeyboards:
            mClosetMgr->ClearInstrument();
            SetPendingState(kCustomizeState_Instruments);
            return 1;
        case 0x1f:
            mClosetMgr->ResetCharacterPreview();
            GotoCustomizeClothingScreen();
            return 1;
        case 0x20:
            SetCustomizeState(mPatchMenuReturnState);
            return 1;
        case 0x21:
        case 0x22:
        case 0x23:
            mClosetMgr->ResetPatches();
            RefreshPatchEdit();
            SetCustomizeState((CustomizeState)0x20);
            return 1;
        default:
            mClosetMgr->ResetCharacterPreview();
            SetPendingState(sBackStates[mCustomizeState]);
            return 1;
        }
}

DataNode CustomizePanel::LeaveCustomizePanel() {
    if (mClosetMgr->IsCharacterLoading()) {
        mWaitingToLeave = true;
        return 1;
    } else {
        mClosetMgr->TakePortrait();
        return DataNode(kDataUnhandled, 0);
    }
}

void CustomizePanel::SetIsWaitingToLeave(bool b) { mWaitingToLeave = b; }

#pragma push
#pragma pool_data off
void CustomizePanel::MovePatch(float dx, float dy) {
    int idx = mPreviewDesc->FindPatchIndex(
        (BandCharDesc::Patch::Category)mPatchCategory, mPatchName.c_str()
    );
    if (idx != -1) {
        BandCharDesc::Patch *patch = mPreviewDesc->GetPatch(idx);
        float oldX = patch->mUV.x;
        float oldY = patch->mUV.y;
        Vector2 newUV;
        newUV.x = oldX + dx;
        newUV.y = oldY + dy;
        if (newUV.x < 0.0f)
            newUV.x = 0.0f;
        else if (newUV.x > 1.0f)
            newUV.x = 1.0f;
        if (newUV.y < 0.0f)
            newUV.y = 0.0f;
        else if (newUV.y > 1.0f)
            newUV.y = 1.0f;
        bool changed = false;
        if (newUV.x != oldX || newUV.y != oldY)
            changed = true;
        if (changed) {
            patch->mUV.x = newUV.x;
            patch->mUV.y = newUV.y;
            RefreshPatchEdit();
        }
    }
}

#pragma pool_data on
void CustomizePanel::RotatePatch(int degrees) {
    int idx = mPreviewDesc->FindPatchIndex(
        (BandCharDesc::Patch::Category)mPatchCategory, mPatchName.c_str()
    );
    if (idx != -1) {
        BandCharDesc::Patch *patch = mPreviewDesc->GetPatch(idx);
        patch->mRotation = fmod(0.017453292f * (float)degrees + patch->mRotation, 6.2831854820251465);
        RefreshPatchEdit();
    }
}
#pragma pool_data off

void CustomizePanel::ScalePatch(float dx, float dy) {
    int idx = mPreviewDesc->FindPatchIndex(
        (BandCharDesc::Patch::Category)mPatchCategory, mPatchName.c_str()
    );
    if (idx != -1) {
        BandCharDesc::Patch *patch = mPreviewDesc->GetPatch(idx);
        float oldX = patch->mScale.x;
        float oldY = patch->mScale.y;
        Vector2 newScale;
        newScale.x = oldX + dx;
        newScale.y = oldY + dy;
        if (newScale.x < 0.0f)
            newScale.x = 0.0f;
        else if (newScale.x > 5.0f)
            newScale.x = 5.0f;
        if (newScale.y < 0.0f)
            newScale.y = 0.0f;
        else if (newScale.y > 5.0f)
            newScale.y = 5.0f;
        bool changed = false;
        if (newScale.x != oldX || newScale.y != oldY)
            changed = true;
        if (changed) {
            patch->mScale.x = newScale.x;
            patch->mScale.y = newScale.y;
            RefreshPatchEdit();
        }
    }
}
#pragma pop

void CustomizePanel::ClearAssetPatchData() {
    unk90 = gNullStr;
    mPatchCategory = BandCharDesc::Patch::kPatchNone;
}

bool CustomizePanel::IsCurrentAssetPatchable() {
    if (unk90 != gNullStr && mPatchCategory != 0)
        return true;
    else
        return false;
}

void CustomizePanel::SetupAssetPatchData(Symbol sym) {
    if (sym == none) {
        ClearAssetPatchData();
        return;
    }
    AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
    MILO_ASSERT(pAssetMgr, 0x65F);
    AssetType ty = pAssetMgr->GetTypeFromName(sym);
    if (mClosetMgr->GetAssetFromAssetType(ty) != sym) {
        ClearAssetPatchData();
        return;
    }
    OutfitConfig *cfg = mClosetMgr->GetCurrentOutfitConfig();
    if (!cfg) {
        ClearAssetPatchData();
        return;
    }
    BandCharDesc::Patch::Category cat = GetPatchCategoryFromAssetType(ty);
    for (int i = 0; i < cfg->mPatches.size(); i++) {
        if (cfg->mPatches[i].mCategory == cat) {
            unk90 = sym;
            mPatchCategory = cat;
            return;
        }
    }
    ClearAssetPatchData();
}

bool CustomizePanel::IsAssetPatchable() {
    OutfitConfig *cfg = mClosetMgr->GetCurrentOutfitConfig();
    if (!cfg)
        return false;
    BandCharDesc::Patch::Category cat =
        GetPatchCategoryFromAssetType(GetAssetTypeFromSymbol(mClosetMgr->unk44));
    for (int i = 0; i < cfg->mPatches.size(); i++) {
        if (cfg->mPatches[i].mCategory == cat)
            return true;
    }
    return false;
}

const char *CustomizePanel::GetPlacementMeshFromCurrentCamShot() {
    static Symbol placement_mesh("placement_mesh");
    CamShot *pCurrentShot = mClosetMgr->GetCurrentShot();
    MILO_ASSERT(pCurrentShot, 0x6B6);
    const char *meshStr = pCurrentShot->Property(placement_mesh, true)->Str();
    return MakeString("%s_placement_%s.mesh", mClosetMgr->GetGender().Str(), meshStr);
}

void CustomizePanel::PreparePatchEdit(BandCharDesc::Patch::Category cat) {
    mPatchCategory = cat;
    mPatchName = GetPlacementMeshFromCurrentCamShot();
}

void CustomizePanel::PrepareAssetPatchEdit() {
    switch (mPatchCategory) {
    case BandCharDesc::Patch::kPatchBass:
        mPatchName = "instrument_placement01.mesh";
        break;
    case BandCharDesc::Patch::kPatchGuitar:
        mPatchName = "instrument_placement01.mesh";
        break;
    case BandCharDesc::Patch::kPatchTorso:
        mPatchName = GetPlacementMeshFromCurrentCamShot();
        break;
    default:
        break;
    }
}

void CustomizePanel::SetCurrentCharacterPatch() {
    mClosetMgr->SetCurrentCharacterPatch(mPatchCategory, mPatchName.c_str());
}

void CustomizePanel::FinishPatchEdit() {
    mClosetMgr->UpdateCharacterPatch(mPatchCategory, mPatchName.c_str());
    mClosetMgr->SetPatches();
    RefreshPatchEdit();
}

void CustomizePanel::RefreshPatchEdit() { mClosetMgr->RecomposePatches(mPatchCategory); }

// RB3-360 retail (lane DQ-1): the "not loaded" text is NOT a MILO_WARN, it is the
// RETURN VALUE.  Retail fn_826169C8 is `DataNode CustomizePanel::SavePrefab(const
// char*)`: r3 = hidden DataNode return, r4 = this, r5 = the char*.  The false leg
// does `bl DataNode::DataNode(const char*)` on the literal at 0x820C3638 (which
// carries NO trailing '\n' — MILO_WARN's would) and returns; the true leg tail-
// calls BandCharacter::SavePrefabFromCloset, passing the char* straight through.
DataNode CustomizePanel::SavePrefab(const char *name) {
    if (!IsLoaded()) {
        return DataNode("Tried to save prefab, but customize_panel is not loaded.");
    }
    BandCharacter *pBandCharacter = TheCharCache->GetCharacter(mUser->GetSlot());
    MILO_ASSERT(pBandCharacter, 0x6fb);
    return pBandCharacter->SavePrefabFromCloset(name);
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(CustomizePanel)
    HANDLE_EXPR(get_character, GetCharData())
    HANDLE_ACTION(set_current_boutique, SetCurrentBoutique(_msg->Sym(2)))
    HANDLE_EXPR(get_current_boutique, GetCurrentBoutique())
    HANDLE_ACTION(clear_current_boutique, ClearCurrentBoutique())
    HANDLE_EXPR(get_wearing, GetWearing())
    HANDLE_ACTION(preview_asset, PreviewAsset(_msg->Sym(2)))
    HANDLE_ACTION(preview_finish, PreviewFinish(_msg->Sym(2)))
    HANDLE_ACTION(select_asset, SelectAsset(_msg->Sym(2)))
    HANDLE_EXPR(asset_provider_has_asset, AssetProviderHasAsset(_msg->Sym(2)))
    HANDLE_EXPR(get_asset_shot, GetAssetShot(_msg->Sym(2)))
    HANDLE_ACTION(
        set_focus_component, SetFocusComponent((CustomizeState)_msg->Int(2), _msg->Sym(3))
    )
    HANDLE_ACTION(store_focus_component, StoreFocusComponent())
    HANDLE_EXPR(get_focus_component, GetFocusComponent())
    HANDLE_ACTION(set_state, SetCustomizeState((CustomizeState)_msg->Int(2)))
    HANDLE_EXPR(get_state, GetCustomizeState())
    HANDLE_EXPR(leave_state, LeaveState(_msg->Int(2)))
    HANDLE_EXPR(in_clothing_state, IsClothingState(mCustomizeState))
    HANDLE_ACTION(
        set_patch_menu_return_state, SetPatchMenuReturnState((CustomizeState)_msg->Int(2))
    )
    HANDLE_EXPR(get_patch_menu_return_state, GetPatchMenuReturnState())
    HANDLE_EXPR(is_refreshing_content, mRefreshingContent)
    HANDLE_EXPR(has_license, HasLicense(_msg->Sym(2)) != 0)
    HANDLE_EXPR(new_asset_provider, mNewAssetProvider)
    HANDLE_EXPR(current_outfit_provider, mCurrentOutfitProvider)
    HANDLE_EXPR(asset_provider, mAssetProvider)
    HANDLE_EXPR(premium_asset_provider, mPremiumAssetProvider)
    HANDLE_EXPR(makeup_provider, mMakeupProvider)
    HANDLE_EXPR(instrument_finish_provider, mInstrumentFinishProvider)
    HANDLE_ACTION(update_new_asset_provider, UpdateNewAssetProvider())
    HANDLE_ACTION(update_current_outfit_provider, UpdateCurrentOutfitProvider())
    HANDLE_ACTION(update_asset_provider, UpdateAssetProvider())
    HANDLE_ACTION(update_makeup_provider, UpdateMakeupProvider(_msg->Sym(2)))
    HANDLE_EXPR(get_current_makeup, GetCurrentMakeup(_msg->Sym(2)))
    HANDLE_ACTION(preview_makeup, PreviewMakeup(_msg->Sym(2)))
    HANDLE_EXPR(has_new_assets, HasNewAssets())
    HANDLE_ACTION(setup_current_outfit, SetupCurrentOutfit(_msg->Sym(2)))
    HANDLE_ACTION(setup_asset_patch_data, SetupAssetPatchData(_msg->Sym(2)))
    HANDLE_EXPR(is_asset_patchable, IsAssetPatchable())
    HANDLE_EXPR(is_current_asset_patchable, IsCurrentAssetPatchable())
    HANDLE_ACTION(
        prepare_patch_edit, PreparePatchEdit((BandCharDesc::Patch::Category)_msg->Int(2))
    )
    HANDLE_ACTION(prepare_asset_patch_edit, PrepareAssetPatchEdit())
    HANDLE_ACTION(set_current_character_patch, SetCurrentCharacterPatch())
    HANDLE_ACTION(finish_patch_edit, FinishPatchEdit())
    HANDLE_ACTION(refresh_patch_edit, RefreshPatchEdit())
    HANDLE_EXPR(has_patch, HasPatch())
    HANDLE_ACTION(set_is_waiting_to_leave, SetIsWaitingToLeave(_msg->Int(2)))
    HANDLE_EXPR(is_waiting_to_leave, mWaitingToLeave)
    HANDLE_ACTION(take_portrait, mClosetMgr->TakePortrait())
    HANDLE_EXPR(save_prefab, SavePrefab(_msg->Str(2)))
    // RB3-360: the Wii-dev asset-token cheat arms (cheat_toggle_asset_tokens /
    // show_asset_tokens), their mShowAssetTokens member, and
    // CheatToggleAssetTokens() do not exist in retail — the retail Handle body
    // jumps straight from save_prefab to the HANDLE_MESSAGE block, and retail
    // RTTI places the vbase at 0xB8 (no trailing bool). Deleted outright
    // (member could not be gated per-TU: other TUs include this header).
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(UIComponentScrollMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x786)
END_HANDLERS
#pragma pop

BEGIN_PROPSYNCS(CustomizePanel)
    SYNC_PROP_SET(
        pending_state, (int &)mPendingState, SetPendingState((CustomizeState)_val.Int())
    )
    SYNC_PROP(unlocked_face_paint, mUnlockedFacePaint)
    SYNC_PROP(unlocked_tattoos, mUnlockedTattoos)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS
