// Ported from rb3-Wii src/system/bandobj/MicInputArrow.cpp (MWCC -> MSVC X360).
#include "bandobj/MicInputArrow.h"
#include "decomp.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "rndobj/Dir.h"
#include "synth/MicManagerInterface.h"
#include "ui/UI.h"
#include "ui/UIResource.h"
#include "utl/Symbols.h"

INIT_REVS(MicInputArrow)

MicClientID sNullMicClientID(-1, -1);

void MicInputArrow::Init() {
    Register();
    TheUI->InitResources("MicInputArrow");
}

DECOMP_FORCEACTIVE(
    MicInputArrow, __FILE__, "( 0) <= (num) && (num) < ( kNumArrows)", "t", "dir", "m"
)

MicInputArrow::MicInputArrow()
    : mArrowNum(0), mMicManagerInterface(0), mMicEnergyNormalizer(1.0f) {
    for (int i = 0; i < 3; i++) {
        mConnectedFlags.push_back(-1);
        mHiddenFlags.push_back(true);
    }
}

BEGIN_COPYS(MicInputArrow)
    CREATE_COPY_AS(MicInputArrow, m)
    MILO_ASSERT(m, 0x4A);
    COPY_MEMBER_FROM(m, mArrowNum)
    COPY_SUPERCLASS_FROM(UIComponent, m)
END_COPYS

SAVE_OBJ(MicInputArrow, 0x53)

BEGIN_LOADS(MicInputArrow)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void MicInputArrow::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0, 0);
    bs >> mArrowNum;
    UIComponent::PreLoad(bs);
}

void MicInputArrow::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    Update();
}

void MicInputArrow::DrawShowing() {
    RndDir *d = mResource->Dir();
    MILO_ASSERT(d, 0x70);
    for (int i = 0; i < mLevelAnims.size(); i++) {
        if (!mHiddenFlags[i]) {
            float f;
            if (mMicManagerInterface) {
                f = mMicManagerInterface->GetEnergyForMic(MicClientID(i, -1));
            } else
                f = 0;
            mLevelAnims[i]->SetFrame(f * mMicEnergyNormalizer, 1.0f);
        }
    }
    d->SetWorldXfm(WorldXfm());
    d->Draw();
}

void MicInputArrow::SetMicMgr(MicManagerInterface *m) { mMicManagerInterface = m; }

void MicInputArrow::SetMicConnected(bool connected, int arrowNum) {
    MILO_ASSERT_RANGE(arrowNum, 0, mConnectedFlags.size(), 0x94);
    if ((char)connected != mConnectedFlags[arrowNum]) {
        mConnectedFlags[arrowNum] = connected;
        if (connected)
            mConnectedTrigs[arrowNum]->Trigger();
        else
            mDisconnectedTrigs[arrowNum]->Trigger();
    }
}

enum {
    kNumArrows = 3,
};

void MicInputArrow::SetMicHidden(int arrowNum) {
    MILO_ASSERT_RANGE(arrowNum, 0, kNumArrows, 0xA6);
    mHiddenFlags[arrowNum] = true;
    mHiddenTrigs[arrowNum]->Trigger();
}

void MicInputArrow::SetMicPreview(int arrowNum) {
    MILO_ASSERT_RANGE(arrowNum, 0, kNumArrows, 0xAF);
    mHiddenFlags[arrowNum] = false;
    mPreviewTrigs[arrowNum]->Trigger();
}

void MicInputArrow::SetMicExtended(int arrowNum) {
    MILO_ASSERT_RANGE(arrowNum, 0, kNumArrows, 0xB7);
    mHiddenFlags[arrowNum] = false;
    mExtendedTrigs[arrowNum]->Trigger();
}

void MicInputArrow::Update() {
    UIComponent::Update();
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0xC4);
    RndDir *dir = mResource->Dir();
    MILO_ASSERT(dir, 199);
    // Retail uses function-local static Symbols here (one shared guard word,
    // bits 0x1..0x40 for the seven names), not the utl/Symbols.h globals.
    mConnectedTrigs.clear();
    static Symbol connected_triggers("connected_triggers");
    DataArray *arr = t->FindArray(connected_triggers);
    for (int i = 1; i < arr->Size(); i++) {
        mConnectedTrigs.push_back(dir->Find<EventTrigger>(arr->Str(i), true));
    }
    mDisconnectedTrigs.clear();
    static Symbol disconnected_triggers("disconnected_triggers");
    DataArray *arr2 = t->FindArray(disconnected_triggers);
    for (int i = 1; i < arr2->Size(); i++) {
        mDisconnectedTrigs.push_back(dir->Find<EventTrigger>(arr2->Str(i), true));
    }
    mHiddenTrigs.clear();
    static Symbol hidden_triggers("hidden_triggers");
    DataArray *arr3 = t->FindArray(hidden_triggers);
    for (int i = 1; i < arr3->Size(); i++) {
        mHiddenTrigs.push_back(dir->Find<EventTrigger>(arr3->Str(i), true));
    }
    mPreviewTrigs.clear();
    static Symbol preview_triggers("preview_triggers");
    DataArray *arr4 = t->FindArray(preview_triggers);
    for (int i = 1; i < arr4->Size(); i++) {
        mPreviewTrigs.push_back(dir->Find<EventTrigger>(arr4->Str(i), true));
    }
    mExtendedTrigs.clear();
    static Symbol extended_triggers("extended_triggers");
    DataArray *arr5 = t->FindArray(extended_triggers);
    for (int i = 1; i < arr5->Size(); i++) {
        mExtendedTrigs.push_back(dir->Find<EventTrigger>(arr5->Str(i), true));
    }
    mLevelAnims.clear();
    static Symbol level_anims("level_anims");
    DataArray *arr6 = t->FindArray(level_anims);
    for (int i = 1; i < arr6->Size(); i++) {
        mLevelAnims.push_back(dir->Find<RndAnimatable>(arr6->Str(i), true));
    }
    static Symbol mic_energy_normalizer("mic_energy_normalizer");
    mMicEnergyNormalizer = t->FindFloat(mic_energy_normalizer);
}

BEGIN_HANDLERS(MicInputArrow)
    HANDLE_ACTION(set_mic_mgr, SetMicMgr(_msg->Obj<MicManagerInterface>(2)))
    HANDLE_ACTION(set_mic_connected, SetMicConnected(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(set_mic_extended, SetMicExtended(_msg->Int(2)))
    HANDLE_ACTION(set_mic_preview, SetMicPreview(_msg->Int(2)))
    HANDLE_ACTION(set_mic_hidden, SetMicHidden(_msg->Int(2)))
    HANDLE_SUPERCLASS(UIComponent)
    HANDLE_CHECK(0x10B)
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

BEGIN_PROPSYNCS(MicInputArrow)
    SYNC_PROP(arrow_num, mArrowNum)
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS
