#include "bandobj/PlayerDiffIcon.h"
#include "rndobj/Font.h"
#include "ui/UI.h"
#include "ui/UILabel.h"
#include "ui/UIResource.h"
#include "utl/Symbols.h"

// --- retail-360 layout shims -------------------------------------------------
// Retail RB3's UILabel / RndText / RndFont differ from our DC3-era engine
// headers in exactly three places this TU touches (all verified against the
// target disassembly of PlayerDiffIcon::DrawShowing @0x82325CA8):
//   * UILabel's alpha scalar lives at 0x1BC (inside the still-unreconstructed
//     UILabel tail, so there is no named member to assign),
//   * RndText holds its single RndFont* at 0xEC (dc3's RndText replaced the
//     single-font model with ObjVector<Style>),
//   * RndFont holds its RndMat* at 0x30 (dc3's RndFont uses ObjPtrVec mMats).
// Model them positionally and TU-locally rather than perturbing the shared
// engine headers, whose current layouts many other units' codegen depends on.
// Retail UILabel::TextObj() is an OUT-OF-LINE accessor (target fn_827F2438 =
// `lwz r3,0x144(r3); blr`); our in-tree UILabel::TextObj() is inline, which
// costs three instructions here. Call the retail accessor by declaration.
RndText *UILabelTextObj(UILabel *);

static inline void SetLabelAlpha(UILabel *l, float a) {
    *(float *)((char *)l + 0x1BC) = a;
}
static inline RndFont *TextFont(RndText *t) { return *(RndFont **)((char *)t + 0xEC); }
static inline RndMat *FontMat(RndFont *f) { return *(RndMat **)((char *)f + 0x30); }

// Retail addresses both rev statics off ONE base register with a +0/+4
// displacement, which requires an INTERNAL-linkage adjacent pair. DECLARE_REVS
// makes them CLASS statics (external symbols) and costs a second `lis`.
// gAltRev FIRST (retail stores hi16 at +0, lo16 at +4 and reads gRev from +4);
// explicit `= 0` is required or they land in .bss where MSVC picks the order.
static unsigned short gAltRev = 0;
static unsigned short gRev = 0;

void PlayerDiffIcon::Init() {
    Register();
    TheUI->InitResources("PlayerDiffIcon");
}

PlayerDiffIcon::PlayerDiffIcon()
    : mPlayerMat(0), mNoPlayerMat(0), mNumPlayers(1), mDiff(0), mAlpha(1.0f) {}

PlayerDiffIcon::~PlayerDiffIcon() {}

BEGIN_COPYS(PlayerDiffIcon)
    CREATE_COPY_AS(PlayerDiffIcon, p)
    MILO_ASSERT(p, 0x2C);
    COPY_MEMBER_FROM(p, mNumPlayers)
    COPY_MEMBER_FROM(p, mDiff)
    COPY_SUPERCLASS_FROM(UIComponent, p)
END_COPYS

BEGIN_SAVES(PlayerDiffIcon)
    SAVE_REVS(0, 0)
    bs << mNumPlayers;
    bs << mDiff;
    SAVE_SUPERCLASS(UIComponent)
END_SAVES

BEGIN_LOADS(PlayerDiffIcon)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void PlayerDiffIcon::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0, 0);
    bs >> mNumPlayers;
    bs >> mDiff;
    UIComponent::PreLoad(bs);
}

void PlayerDiffIcon::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    Update();
}

void PlayerDiffIcon::DrawShowing() {
    RndDir *d = mResource->Dir();
    MILO_ASSERT(d, 0x53);
    for (int i = 0; i < mPlayerMeshes.size(); i++) {
        mPlayerMeshes[i]->SetMat(i < mNumPlayers ? mPlayerMat : mNoPlayerMat);
    }
    for (int i = 0; i < mDiffLabels.size(); i++) {
        mDiffLabels[i]->SetShowing(i == mDiff);
    }
    for (ObjDirItr<UILabel> it(d, true); it != 0; ++it) {
        SetLabelAlpha(it, mAlpha);
        RndFont *font = TextFont(UILabelTextObj(it));
        if (font) {
            RndMat *mat = FontMat(font);
            if (mat)
                mat->SetColor(mColor);
        }
    }
    for (ObjDirItr<RndMat> it(d, true); it != 0; ++it) {
        it->SetAlpha(mAlpha);
    }
    d->SetWorldXfm(WorldXfm());
    d->Draw();
}

RndDrawable *PlayerDiffIcon::CollideShowing(const Segment &s, float &f, Plane &p) {
    return mResource->Dir()->Collide(s, f, p);
}

int PlayerDiffIcon::CollidePlane(const Plane &p) {
    return mResource->Dir()->CollidePlane(p);
}

void PlayerDiffIcon::SetAlphaColor(float a, UIColor *c) {
    mAlpha = a;
    if (c)
        mColor = c->GetColor();
}

void PlayerDiffIcon::GrowBoundingBox(Box &) const {}

void PlayerDiffIcon::SetNumPlayersDiff(int i, int j) {
    mNumPlayers = i;
    mDiff = j;
}

void PlayerDiffIcon::Update() {
    UIComponent::Update();
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0x9A);
    RndDir *dir = mResource->Dir();
    MILO_ASSERT(dir, 0x9D);
    mPlayerMeshes.clear();
    // Retail builds each of these four Symbols as a FUNCTION-LOCAL STATIC
    // (one shared guard word, bits 0x1/0x2/0x4/0x8 in source order) rather
    // than referencing the centralized globals in utl/Symbols*.h -- see the
    // RB3_HANDLE_LOCAL_STATIC note in obj/ObjMacros.h for the same lever.
    static Symbol player_meshes("player_meshes");
    DataArray *arr = t->FindArray(player_meshes);
    for (int i = 1; i < arr->Size(); i++) {
        mPlayerMeshes.push_back(dir->Find<RndMesh>(arr->Str(i), true));
    }
    static Symbol player_mat("player_mat");
    mPlayerMat = dir->Find<RndMat>(t->FindStr(player_mat), true);
    static Symbol no_player_mat("no_player_mat");
    mNoPlayerMat = dir->Find<RndMat>(t->FindStr(no_player_mat), true);
    mDiffLabels.clear();
    static Symbol diff_labels("diff_labels");
    DataArray *arr2 = t->FindArray(diff_labels);
    for (int i = 1; i < arr2->Size(); i++) {
        mDiffLabels.push_back(dir->Find<BandLabel>(arr2->Str(i), true));
    }
}

BEGIN_HANDLERS(PlayerDiffIcon)
    HANDLE_ACTION(set_num_players_diff, SetNumPlayersDiff(_msg->Int(2), _msg->Int(3)))
    HANDLE_SUPERCLASS(UIComponent)
    HANDLE_CHECK(0xBD)
END_HANDLERS

// Retail's SyncProperty compares against FUNCTION-LOCAL static Symbols (guard word +
// ??__F atexit funclet per prop), not the centralized globals in utl/Symbols*.h --
// same divergence RB3_HANDLE_LOCAL_STATIC fixes for the HANDLE_* family. SYNC_PROP*
// is not covered by that gate, so override it TU-locally (no other TU's codegen
// moves).  Lane CT-4: measured +6 matched on DialogDisplay with the same lever.
#undef SYNC_PROP
#define SYNC_PROP(symbol, member)                                                        \
    {                                                                                    \
        static Symbol _ps(#symbol);                                                      \
        if (sym == _ps)                                                                  \
            return PropSync(member, _val, _prop, _i + 1, _op);                           \
    }

BEGIN_PROPSYNCS(PlayerDiffIcon)
    SYNC_PROP(num_players, mNumPlayers)
    SYNC_PROP(diff, mDiff)
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS
