#include "meta_band/ChooseColorPanel.h"
#include "bandobj/OutfitConfig.h"
#include "meta_band/ClosetMgr.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "ui/UIPanel.h"
#include "utl/Symbols.h"

// Retail constructs mColorOptions with an out-of-line `bl` to the map default
// ctor (target 180 B); /Ob2 inlines it for us into ~10 stores plus a 16-byte
// stack temp (base 252 B, +0x10 of frame). The class layout is compiler-verified
// identical to retail (mColorOptions @0x48, 28 B, ints @0x64/0x68), so this is
// purely an inline-policy divergence -- the same lever used in VocalPlayer.cpp,
// EQEffect.cpp and mtx.cpp (auto_inline(off) is the WRONG lever here -- it only
// stops THIS function being inlined elsewhere, not callees being inlined into it;
// measured byte-identical. See obj/Object.h:632.)
//
// TRADE-OFF, measured (lane DI-2/D) -- this pragma is NOT free. MSVC generates
// the implicit dtor family for this class inside whatever inline_depth region
// the ctor sits in (moving the ctor to end-of-TU does not escape it), so
// ??_GChooseColorPanel stops inlining ??1/??_D and regresses 100 -> 38.5
// (ours 20 insns vs retail's 17). Unit 27/29 -> 28/29, but:
//     matched +1 / masked_equal +1 / honest +0 / matched_code +180 B
// i.e. the extra matched FUNCTION is disclosure (anon byte-pairing); the 180
// CODE bytes are real (exactly this ctor). Neither shape is a behavioural bug.
// Drop this pragma if you would rather keep ??_G byte-exact.
#pragma inline_depth(0)
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
