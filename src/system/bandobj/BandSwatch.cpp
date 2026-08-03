#include "bandobj/BandSwatch.h"
#include "math/Rand.h"
#include "obj/ObjMacros.h"
#include "rndobj/Mat.h"
#include "ui/UI.h"
#include "ui/UIResource.h"
#include "utl/Symbols.h"

ColorPalette *BandSwatch::sDummyPalette;

// Retail folds both rev words onto ONE base register with offsets 0/4, which
// only happens for internal-linkage, align(4) file-scope statics (altRev+0,
// rev+4) -- not for the DECLARE_REVS/INIT_REVS class statics. Same lever as
// BandWardrobe.cpp / BandDirector.cpp. Cannot use the `#define gRev` spelling
// here: the Rnd_Xbox.cpp scatter-include below re-#defines those names.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gSwatchRevs;

void BandSwatch::Init() {
    TheUI->InitResources("BandSwatch");
    Register();
    sDummyPalette = Hmx::Object::New<ColorPalette>();
    if (LOADMGR_EDITMODE) {
        for (int i = 0; i < 10; i++) {
            sDummyPalette->mColors.push_back(
                Hmx::Color(RandomFloat(), RandomFloat(), RandomFloat())
            );
        }
    }
}

void BandSwatch::Terminate() { RELEASE(sDummyPalette); }

BandSwatch::BandSwatch() : mColorPalette(this, 0) { MILO_ASSERT(sDummyPalette, 0x30); }

BandSwatch::~BandSwatch() { DeleteAll(unk1e8); }

BEGIN_COPYS(BandSwatch)
    CREATE_COPY_AS(BandSwatch, s)
    MILO_ASSERT(s, 0x3D);
    COPY_MEMBER_FROM(s, mColorPalette)
    COPY_SUPERCLASS_FROM(UIList, s)
END_COPYS

SAVE_OBJ(BandSwatch, 0x48)

BEGIN_LOADS(BandSwatch)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void BandSwatch::PreLoad(BinStream &bs) {
    int rev;
    bs >> rev;
    gSwatchRevs.rev = getHmxRev(rev);
    gSwatchRevs.altRev = getAltRev(rev);
    ASSERT_REVS(1, 0);
    if (gSwatchRevs.rev != 0)
        bs >> mColorPalette;
    UIList::PreLoad(bs);
}

RndMat *BandSwatch::Mat(int, int, UIListMesh *) const {
    return mResource->Dir()->Find<RndMat>("color.mat", true);
}

int BandSwatch::NumData() const { return unk1e8.size(); }

UIColor *BandSwatch::SlotColorOverride(int, int idx, UIListWidget *, UIColor *col) const {
    if (!unk1e8.empty())
        return unk1e8[idx];
    else
        return col;
}

void BandSwatch::SetColors(ColorPalette *palette) { mColorPalette = palette; }

void BandSwatch::UpdateColors() {
    DeleteAll(unk1e8);
    std::vector<Hmx::Color> &colors =
        mColorPalette.Ptr() ? mColorPalette->mColors : sDummyPalette->mColors;
    for (std::vector<Hmx::Color>::iterator it = colors.begin(); it != colors.end();
         ++it) {
        Hmx::Color hmxcol(*it);
        UIColor *newcol = Hmx::Object::New<UIColor>();
        newcol->SetColor(hmxcol);
        unk1e8.push_back(newcol);
    }
    Refresh(false);
}

void BandSwatch::Enter() {
    UIList::SetProvider(this);
    UpdateColors();
}

BEGIN_HANDLERS(BandSwatch)
    HANDLE_ACTION(set_colors, SetColors(_msg->Obj<ColorPalette>(2)))
    HANDLE_SUPERCLASS(UIList)
    HANDLE_CHECK(0x94)
END_HANDLERS

BEGIN_PROPSYNCS(BandSwatch)
    static Symbol color_palette("color_palette");
    SYNC_PROP_MODIFY_ALT(color_palette, mColorPalette, UpdateColors())
    SYNC_SUPERCLASS(UIList)
END_PROPSYNCS
// sw2 scatter-include (default/BandSwatch <- rnddx9/Rnd_Xbox.cpp)
#define gRev gRev_Rnd_Xbox
#define gAltRev gAltRev_Rnd_Xbox
#include "rnddx9/Rnd_Xbox.cpp"
#undef gRev
#undef gAltRev

// Lane-AE scatter force-emit: retail placed PatchRenderer's OBJ_CLASSNAME
// COMDAT (?StaticClassName@PatchRenderer@@SA?AVSymbol@@XZ) inside the .text span
// pinned to default/BandSwatch. The macro defines it inline, so it is only
// emitted where it is odr-used -- nothing in this TU used it, so our obj
// never defined the symbol and objdiff could not pair it. Force the use.
#include "bandobj/PatchRenderer.h"
Symbol ForceEmit_PatchRenderer_StaticClassName() { return PatchRenderer::StaticClassName(); }
