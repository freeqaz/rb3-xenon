#include "bandobj/InstrumentDifficultyDisplay.h"
#include "decomp.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Dir.h"
#include "ui/UI.h"
#include "ui/UIResource.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"

INIT_REVS(InstrumentDifficultyDisplay);

void InstrumentDifficultyDisplay::Init() {
    Register();
    TheUI->InitResources("InstrumentDifficultyDisplay");
}

InstrumentDifficultyDisplay::InstrumentDifficultyDisplay()
    : mDifficultyAnim(0), mVocalPart1Mat(0), mVocalPart2Mat(0), mVocalPart3Mat(0),
      mInstrumentState(kName), mHasPart(1), mDifficulty(3), mNumVocalParts(0),
      mInstrumentType("band"), mInstrumentColorOverride(this, 0) {
    mInstrumentLabel = Hmx::Object::New<BandLabel>();
    mVocalPartMesh = Hmx::Object::New<RndMesh>();
}

InstrumentDifficultyDisplay::~InstrumentDifficultyDisplay() {
    delete mInstrumentLabel;
    delete mVocalPartMesh;
}

BEGIN_COPYS(InstrumentDifficultyDisplay)
    CREATE_COPY_AS(InstrumentDifficultyDisplay, p)
    MILO_ASSERT(p, 0x39);
    COPY_MEMBER_FROM(p, mDifficulty)
    COPY_MEMBER_FROM(p, mNumVocalParts)
    COPY_MEMBER_FROM(p, mInstrumentType)
    COPY_MEMBER_FROM(p, mHasPart)
    COPY_MEMBER_FROM(p, mInstrumentState)
    COPY_MEMBER_FROM(p, mInstrumentColorOverride)
    UIComponent::Copy(p, ty);
END_COPYS

// Retail-360 has a REAL Save here (the rb3-Wii oracle's `SAVE_OBJ` assert-stub is
// a 4-byte body; retail's fn_82323D18 is 184 B of BinStream writes). Member order
// recovered from the target: packed revs 4, then Symbol@-0x14, int@-0x1c,
// bool@-0x20, ObjPtr@-0x10, int@-0x18, int@-0x24, then UIComponent::Save.
BEGIN_SAVES(InstrumentDifficultyDisplay)
    SAVE_REVS(4, 0)
    bs << mInstrumentType << mDifficulty << mHasPart << mInstrumentColorOverride;
    bs << mNumVocalParts << (int)mInstrumentState;
    SAVE_SUPERCLASS(UIComponent)
END_SAVES

BEGIN_LOADS(InstrumentDifficultyDisplay)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void InstrumentDifficultyDisplay::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(4, 0);
    bs >> mInstrumentType >> mDifficulty >> mHasPart;
    if (gRev >= 1)
        bs >> mInstrumentColorOverride;
    if (gRev >= 2)
        bs >> mNumVocalParts;
    if (gRev == 3) {
        bool b;
        bs >> b;
        mInstrumentState = (InstrumentState)(b != 0);
    }
    if (gRev >= 4) {
        int i;
        bs >> i;
        mInstrumentState = (InstrumentState)i;
    }
    UIComponent::PreLoad(bs);
}

DECOMP_FORCEACTIVE(InstrumentDifficultyDisplay, "false")

void InstrumentDifficultyDisplay::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    Update();
}

void InstrumentDifficultyDisplay::UpdateDisplay() {
    mVocalPartMesh->SetShowing(false);
    if (mInstrumentState == kName) {
        mInstrumentLabel->SetColorOverride(mInstrumentColorOverride);
        if (mInstrumentLabel->TextToken() != mInstrumentType) {
            mInstrumentLabel->SetTextToken(mInstrumentType);
        }
    } else if (mInstrumentState == kIcon) {
        static Symbol get_inst_icon("get_inst_icon");
        Message msg(get_inst_icon);
        // Retail has NO kDataString type check here (the rb3-Wii oracle's
        // `if (handled.Type() == kDataString) ... else MILO_WARN(...)` emits a
        // cmpwi 0x12 / bne that the target bytes do not contain).
        mInstrumentLabel->SetIcon(*HandleType(msg).Str());
    }
    mInstrumentLabel->SetShowing(true);
}

void InstrumentDifficultyDisplay::DrawShowing() {
    RndDir *dir = mResource->Dir();
    MILO_ASSERT(dir, 0xBB);
    mDifficultyAnim->SetFrame(mHasPart ? mDifficulty : 7.0f, 1.0f);
    if (mInstrumentState != kHidden) {
        mInstrumentLabel->SetTransParent(this, false);
        mInstrumentLabel->Draw();
    }
    SetWorldXfm(WorldXfm());
    dir->SetWorldXfm(WorldXfm());
    dir->Draw();
    mVocalPartMesh->SetTransParent(this, false);
    mVocalPartMesh->Draw();
}

DECOMP_FORCEACTIVE(InstrumentDifficultyDisplay, "set_song")

void InstrumentDifficultyDisplay::SetValues(Symbol s, int i1, int i2, bool b) {
    mInstrumentType = s;
    mDifficulty = i1;
    mNumVocalParts = i2;
    mHasPart = b;
    UpdateDisplay();
}

void InstrumentDifficultyDisplay::SetInstrumentState(InstrumentState state) {
    mInstrumentState = state;
    UpdateDisplay();
}

void InstrumentDifficultyDisplay::Update() {
    UIComponent::Update();
    const DataArray *typeDef = TypeDef();
    MILO_ASSERT(typeDef, 0xEF);
    RndDir *dir = mResource->Dir();
    MILO_ASSERT(dir, 0xF2);
    static Symbol difficulty_anim("difficulty_anim");
    mDifficultyAnim = dir->Find<RndPropAnim>(typeDef->FindStr(difficulty_anim), true);
    static Symbol vocal_part1_mat("vocal_part1_mat");
    mVocalPart1Mat = dir->Find<RndMat>(typeDef->FindStr(vocal_part1_mat), true);
    static Symbol vocal_part2_mat("vocal_part2_mat");
    mVocalPart2Mat = dir->Find<RndMat>(typeDef->FindStr(vocal_part2_mat), true);
    static Symbol vocal_part3_mat("vocal_part3_mat");
    mVocalPart3Mat = dir->Find<RndMat>(typeDef->FindStr(vocal_part3_mat), true);
    static Symbol vocal_part_mesh("vocal_part_mesh");
    RndMesh *vocalpartmesh = dir->Find<RndMesh>(typeDef->FindStr(vocal_part_mesh), true);
    mVocalPartMesh->Copy(vocalpartmesh, kCopyShallow);
    vocalpartmesh->SetShowing(false);
    static Symbol instrument_label("instrument_label");
    BandLabel *instrLabel =
        dir->Find<BandLabel>(typeDef->FindStr(instrument_label), true);
    static Symbol instrument_icon("instrument_icon");
    BandLabel *instrIcon = dir->Find<BandLabel>(typeDef->FindStr(instrument_icon), true);
    if (mInstrumentState == kName) {
        mInstrumentLabel->ResourceCopy(instrLabel);
    } else if (mInstrumentState == kIcon) {
        mInstrumentLabel->ResourceCopy(instrIcon);
    }
    instrLabel->SetShowing(false);
    instrIcon->SetShowing(false);
    UpdateDisplay();
}

BEGIN_HANDLERS(InstrumentDifficultyDisplay)
    HANDLE_ACTION_STATIC(
        set_values, SetValues(_msg->Sym(2), _msg->Int(3), _msg->Int(4), _msg->Int(5))
    )
    HANDLE_SUPERCLASS(UIComponent)
    HANDLE_CHECK(0x11B)
END_HANDLERS

BEGIN_PROPSYNCS(InstrumentDifficultyDisplay)
    { _NEW_STATIC_SYMBOL(difficulty) SYNC_PROP_MODIFY_ALT(_s, mDifficulty, UpdateDisplay()) }
    { _NEW_STATIC_SYMBOL(instrument_type) SYNC_PROP_MODIFY_ALT(_s, mInstrumentType, UpdateDisplay()) }
    { _NEW_STATIC_SYMBOL(num_vocal_parts) SYNC_PROP_MODIFY_ALT(_s, mNumVocalParts, UpdateDisplay()) }
    { _NEW_STATIC_SYMBOL(has_part) SYNC_PROP_MODIFY_ALT(_s, mHasPart, UpdateDisplay()) }
    SYNC_PROP_SET_STATIC(
        instrument_state,
        mInstrumentState,
        SetInstrumentState((InstrumentState)_val.Int())
    )
    SYNC_PROP_STATIC(instrument_color_override, mInstrumentColorOverride)
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS
