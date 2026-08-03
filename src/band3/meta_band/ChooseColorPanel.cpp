#include "meta_band/ChooseColorPanel.h"
#include "bandobj/OutfitConfig.h"
#include "meta_band/ClosetMgr.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "ui/UIPanel.h"
#include "utl/Symbols.h"

// ⚠ CORRECTION (lane DS-1): this block used to call the callee "the map default
// ctor". It is a HASH_MAP ctor -- mColorOptions is now declared std::hash_map
// (see ChooseColorPanel.h for the retail-body-size adjudication: 76 B ctor and
// 120 B operator[], vs 84 B / 160 B for map). The container kind was invisible
// to the metric because callee names are relocation args, so the wrong
// identification survived at a clean 100%. Correcting it took
// ChooseColorPanel::NewObject 99.96 -> 100.0 (+100 B).
//
// Retail constructs mColorOptions with an out-of-line `bl` to that container's
// default ctor; /Ob2 inlines it for us into ~10 stores plus a 16-byte
// stack temp (base 252 B, +0x10 of frame). The class layout is compiler-verified
// identical to retail (mColorOptions @0x48, 28 B, ints @0x64/0x68) and is
// UNCHANGED by the map->hash_map swap (both containers are 0x1c), so this is
// purely an inline-policy divergence -- the same lever used in VocalPlayer.cpp,
// EQEffect.cpp and mtx.cpp (auto_inline(off) is the WRONG lever here -- it only
// stops THIS function being inlined elsewhere, not callees being inlined into it;
// measured byte-identical. See obj/Object.h:632.)
//
// ⚠ CORRECTION (lane DY-2a): the depth ARGUMENT matters, and the "TRADE-OFF"
// this comment used to describe was an artifact of only ever trying depth 0.
// Lane DI-2/D measured that inline_depth(0) buys the ctor but costs
// ??_GChooseColorPanel (100 -> 38.5, ours 20 insns vs retail's 17) and
// concluded the two were mutually exclusive. They are NOT.
//
// MSVC does generate this class's implicit dtor family inside whatever
// inline_depth region the ctor sits in (that part of DI-2/D's finding stands --
// the vftable, and hence ??_G, is required BY the ctor, so it inherits the
// ctor's region and moving the ctor to end-of-TU does not escape it). But
// retail's ??_G is not "uninlined": at 0x82612E80 it makes TWO out-of-line calls,
// `bl ??1ChooseColorPanel` then `bl ??1Object@Hmx@@`, which is ??_D (the vbase
// dtor) expanded into ??_G at exactly ONE level, with ??1 left out of line.
// depth 0 forbids that one level; depth 1 permits it.
//
// depth 1 does NOT cost the ctor, which was the reason to fear it: the hash_map
// default ctor is a direct callee and would nominally be depth-1 eligible, yet
// MEASURED it still does not expand (MSVC declines it because its own body is
// dominated by depth-2 callees it cannot expand). All three affected bodies are
// verified 100% at depth 1: ??_G 17/17 insns, the ctor 45/45, NewObject 25/25.
// ⇒ Do not "restore" depth 0, and do not remove the pragma; both regress.
#pragma inline_depth(1)
ChooseColorPanel::ChooseColorPanel()
    : mCurrentOutfitConfig(0), mCurrentOutfitPiece(0), mNumOptions(-1),
      mCurrentOption(-1) {
    mClosetMgr = ClosetMgr::GetClosetMgr();
    MILO_ASSERT(mClosetMgr, 0x19);
}
#pragma inline_depth()

void ChooseColorPanel::Load() {
    UIPanel::Load();
    mCurrentOutfitConfig = mClosetMgr->GetCurrentOutfitConfig();
    MILO_ASSERT(mCurrentOutfitConfig, 0x21);
    mCurrentOutfitPiece = mClosetMgr->GetCurrentOutfitPiece();
    MILO_ASSERT(mCurrentOutfitPiece, 0x24);
    mNumOptions = mCurrentOutfitConfig->NumColorOptions();
    OutfitConfig *cfg = mCurrentOutfitConfig;
    ObjVector<OutfitConfig::MatSwap> &mats = cfg->mMats;
    for (int i = 0; i < mats.size(); i++) {
        OutfitConfig::MatSwap *pMatSwap = &mats[i];
        MILO_ASSERT(pMatSwap, 0x2C);
        if (pMatSwap->mColor1Option != pMatSwap->mColor2Option) {
            AddColorOption(pMatSwap->mColor1Option, pMatSwap->mColor1Palette);
            AddColorOption(pMatSwap->mColor2Option, pMatSwap->mColor2Palette);
        } else if (pMatSwap->mColor1Palette) {
            AddColorOption(pMatSwap->mColor1Option, pMatSwap->mColor1Palette);
        } else if (pMatSwap->mColor2Palette) {
            AddColorOption(pMatSwap->mColor1Option, pMatSwap->mColor2Palette);
        } else {
            MILO_WARN(
                "(%s.milo) OutfitConfig mats[%i] has no color palettes set!",
                mCurrentOutfitPiece->mName,
                i
            );
        }
    }
    MILO_ASSERT(mColorOptions.size() == mNumOptions, 0x49);
    if (mNumOptions >= 1)
        mCurrentOption = 0;
}

void ChooseColorPanel::Enter() { UIPanel::Enter(); }

void ChooseColorPanel::Poll() {
    UIPanel::Poll();
    mClosetMgr->ForceClosetPoll();
}

void ChooseColorPanel::Draw() { UIPanel::Draw(); }

void ChooseColorPanel::Exit() { UIPanel::Exit(); }

void ChooseColorPanel::Unload() {
    UIPanel::Unload();
    mCurrentOutfitConfig = 0;
    mColorOptions.clear();
    mNumOptions = -1;
    mCurrentOption = -1;
}

void ChooseColorPanel::AddColorOption(int i, ColorPalette *pal) {
    if (pal)
        mColorOptions[i] = pal;
}

int ChooseColorPanel::GetCurrentColor() {
    int color = mCurrentOutfitPiece->mColors[mCurrentOption];
    if (color == -1) {
        color = mCurrentOutfitConfig->mColors[mCurrentOption];
    }
    return color;
}

void ChooseColorPanel::PreviewColor(int color) {
    mCurrentOutfitPiece->mColors[mCurrentOption] = color;
    mClosetMgr->PreviewCharacter(true, false);
}

BEGIN_HANDLERS(ChooseColorPanel)
    HANDLE_EXPR(get_color_palette, GetColorPalette())
    HANDLE_EXPR(get_current_color, GetCurrentColor())
    HANDLE_ACTION(preview_color, PreviewColor(_msg->Int(2)))
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x9C)
END_HANDLERS

// Retail's SyncProperty compares against FUNCTION-LOCAL static Symbols (guard word +
// ??__F atexit funclet per prop), not the centralized globals in utl/Symbols*.h --
// same divergence RB3_HANDLE_LOCAL_STATIC fixes for the HANDLE_* family. SYNC_PROP is
// not covered by that gate, so override it TU-locally (no other TU's codegen moves).
#undef SYNC_PROP
#define SYNC_PROP(symbol, member)                                                        \
    {                                                                                    \
        static Symbol _ps(#symbol);                                                      \
        if (sym == _ps)                                                                  \
            return PropSync(member, _val, _prop, _i + 1, _op);                           \
    }

BEGIN_PROPSYNCS(ChooseColorPanel)
    SYNC_PROP(num_options, mNumOptions)
    SYNC_PROP(current_option, mCurrentOption)
END_PROPSYNCS
