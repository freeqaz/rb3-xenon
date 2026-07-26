// Ported from rb3-Wii src/system/bandobj/ScrollbarDisplay.cpp (MWCC -> MSVC X360).
#include "bandobj/ScrollbarDisplay.h"
#include "decomp.h"
#include "math/Utl.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "rndobj/Dir.h"
#include "ui/PanelDir.h"
#include "ui/UI.h"
#include "ui/UIListDir.h"
#include "ui/UIListState.h"
#include "ui/UIResource.h"
#include "utl/Symbols.h"

// Retail addresses the two revision words off a SINGLE base label with +0 /
// +4 offsets (`addi r29, r11, lbl_82CBDF08` then `sth ..,0x4(r29)` /
// `sth ..,0x0(r29)` / `lhz ..,0x4(r29)`). That is only possible for statics the
// compiler lays out itself, i.e. TU-local file statics -- class statics are
// external symbols each needing its own @ha/@l pair, which is what
// DECLARE_REVS/INIT_REVS produce.
static unsigned short gRev;
static unsigned short gAltRev;

void ScrollbarDisplay::Init() {
    Register();
    TheUI->InitResources("ScrollbarDisplay");
}

ScrollbarDisplay::ScrollbarDisplay()
    : m_pList(this, 0), mScrollbarHeight(200.0f), mAlwaysShow(0), mListXOffset(0),
      mListYOffset(0), mMinThumbHeight(1.0f), m_pTopBone(0), m_pBottomBone(0),
      m_pThumbTopBone(0), m_pThumbBottomBone(0), m_pThumbGroup(0), unk140(-1), unk144(-1),
      unk148(-1), unk14c(-1), m_fSavedPosition(0), m_fSavedScale(0) {}

ScrollbarDisplay::~ScrollbarDisplay() {}

BEGIN_COPYS(ScrollbarDisplay)
    COPY_SUPERCLASS(UIComponent)
END_COPYS

void ScrollbarDisplay::CopyMembers(const UIComponent *o, Hmx::Object::CopyType ty) {
    UIComponent::CopyMembers(o, ty);
    CREATE_COPY_AS(ScrollbarDisplay, pDisplay);
    MILO_ASSERT(pDisplay, 0x44);
    COPY_MEMBER_FROM(pDisplay, m_pList)
    COPY_MEMBER_FROM(pDisplay, mAlwaysShow)
    COPY_MEMBER_FROM(pDisplay, mListXOffset)
    COPY_MEMBER_FROM(pDisplay, mListYOffset)
    COPY_MEMBER_FROM(pDisplay, mScrollbarHeight)
    COPY_MEMBER_FROM(pDisplay, mMinThumbHeight)
}

// Retail-360 really saves (the Wii dev build's SAVE_OBJ MILO_FAIL stub is
// absent): rev 2, then the same member order PreLoad reads back.
void ScrollbarDisplay::Save(BinStream &bs) {
    bs << 2;
    // One chained full-expression from m_pList onward: retail threads the
    // BinStream& returned by the ObjPtr operator<< through the remaining
    // writes and therefore gives every temporary its own frame slot.
    bs << m_pList << mAlwaysShow << mListXOffset << mListYOffset << mScrollbarHeight
       << mMinThumbHeight;
    UIComponent::Save(bs);
}

BEGIN_LOADS(ScrollbarDisplay)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void ScrollbarDisplay::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(2, 0);
    bs >> m_pList;
    bs >> mAlwaysShow;
    if (gRev < 1) {
        bool b;
        bs >> b;
    }
    bs >> mListXOffset;
    bs >> mListYOffset;
    bs >> mScrollbarHeight;
    if (gRev >= 2)
        bs >> mMinThumbHeight;
    UIComponent::PreLoad(bs);
}

void ScrollbarDisplay::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    Update();
}

void ScrollbarDisplay::Enter() { UIComponent::Enter(); }

void ScrollbarDisplay::UpdateSavedListInfo() {
    if (m_pList) {
        int num = m_pList->NumProviderData() - m_pList->NumDisplay();
        UIListState &state = m_pList->GetListState();
        if (state.ScrollPastMaxDisplay())
            num += state.MaxDisplay();
        if (num < 1)
            num = 1;
        int iFirstShowing = m_pList->FirstShowing();
        if (iFirstShowing != unk144 || num != unk140) {
            MILO_ASSERT(iFirstShowing >= 0, 0xA2);
            if (iFirstShowing > num)
                iFirstShowing = num;
            unk140 = num;
            unk144 = iFirstShowing;
            m_fSavedPosition = (float)unk144 / (float)unk140;
            MILO_ASSERT(m_fSavedPosition >= 0.0f, 0xAC);
            MILO_ASSERT(m_fSavedPosition <= 1.0f, 0xAD);
        }
        num = m_pList->NumProviderData();
        int disp = m_pList->NumDisplay();
        if (disp != unk14c || num != unk140) {
            unk148 = num;
            unk14c = disp;
            if (unk14c < unk148)
                m_fSavedScale = (float)unk14c / (float)unk148;
            else
                m_fSavedScale = 1.0f;
            MILO_ASSERT(m_fSavedScale >= 0.0f, 0xC3);
            MILO_ASSERT(m_fSavedScale <= 1.0f, 0xC4);
            if (GetListAttached())
                SetHeight(GetListHeight());
        }
    }
}

float ScrollbarDisplay::GetListHeight() const {
    MILO_ASSERT(m_pList, 0xDF);
    float f = 0;
    const UIListDir *dir = m_pList->GetUIListDir();
    if (dir)
        f = dir->ElementSpacing();
    return f * m_pList->NumDisplay();
}

DECOMP_FORCEACTIVE(
    ScrollbarDisplay,
    "m_pThumbTopBone",
    "m_pThumbBottomBone",
    "fHeight >= 0",
    "m_pTopBone",
    "m_pBottomBone",
    "m_pThumbGroup"
)

void ScrollbarDisplay::UpdateScrollbarHeightAndPosition() {
    MILO_ASSERT(m_pTopBone, 0xFF);
    MILO_ASSERT(m_pBottomBone, 0x100);
    if (GetListAttached()) {
        SetLocalPos(Vector3(mListXOffset, 0, mListYOffset));
    }
    float minH = mMinThumbHeight;
    if (mScrollbarHeight < minH)
        mScrollbarHeight = minH;
    Vector3 v(m_pTopBone->LocalXfm().v);
    v.z -= mScrollbarHeight;
    m_pBottomBone->SetLocalPos(v);
}

void ScrollbarDisplay::UpdateThumbScaleAndPosition() {
    MILO_ASSERT(m_pThumbTopBone, 0x11B);
    MILO_ASSERT(m_pThumbBottomBone, 0x11C);
    MILO_ASSERT(m_pTopBone, 0x11D);
    MILO_ASSERT(m_pBottomBone, 0x11E);
    float h = mScrollbarHeight;
    float scale = h * GetSavedScale();
    float pos = GetSavedPosition();
    MaxEq(scale, mMinThumbHeight);
    pos *= h - scale;
    Vector3 top(m_pTopBone->LocalXfm().v);
    top.z -= pos;
    m_pThumbTopBone->SetLocalPos(top);
    Vector3 thumb(m_pThumbTopBone->LocalXfm().v);
    thumb.z -= scale;
    m_pThumbBottomBone->SetLocalPos(thumb);
    MILO_ASSERT(m_pThumbGroup, 0x13E);
    if (m_fSavedScale < 1.0f)
        m_pThumbGroup->SetShowing(true);
    else
        m_pThumbGroup->SetShowing(false);
}

void ScrollbarDisplay::DrawShowing() {
    RndDir *pDir = mResource->Dir();
    MILO_ASSERT(pDir, 0x14E);
    UpdateSavedListInfo();
    UpdateScrollbarHeightAndPosition();
    UpdateThumbScaleAndPosition();
    if (mAlwaysShow || m_fSavedScale < 1.0f) {
        pDir->SetWorldXfm(WorldXfm());
        pDir->Draw();
    }
}

void ScrollbarDisplay::Update() {
    UIComponent::Update();
    const DataArray *pTypeDef = TypeDef();
    MILO_ASSERT(pTypeDef, 0x163);
    RndDir *pDir = mResource->Dir();
    MILO_ASSERT(pDir, 0x166);
    // Retail uses function-local static Symbols here (one shared guard word,
    // five bits -> five `??__F`-style guard-clear thunks in the .text span),
    // not the utl/Symbols.h globals.
    static Symbol top_bone("top_bone");
    m_pTopBone = pDir->Find<RndMesh>(pTypeDef->FindStr(top_bone), true);
    static Symbol bottom_bone("bottom_bone");
    m_pBottomBone = pDir->Find<RndMesh>(pTypeDef->FindStr(bottom_bone), true);
    static Symbol thumb_top_bone("thumb_top_bone");
    m_pThumbTopBone = pDir->Find<RndMesh>(pTypeDef->FindStr(thumb_top_bone), true);
    static Symbol thumb_bottom_bone("thumb_bottom_bone");
    m_pThumbBottomBone = pDir->Find<RndMesh>(pTypeDef->FindStr(thumb_bottom_bone), true);
    MILO_ASSERT(m_pThumbTopBone, 0x171);
    MILO_ASSERT(m_pThumbBottomBone, 0x172);
    MILO_ASSERT(m_pTopBone, 0x173);
    MILO_ASSERT(m_pBottomBone, 0x174);
    static Symbol thumb_group("thumb_group");
    m_pThumbGroup = pDir->Find<RndGroup>(pTypeDef->FindStr(thumb_group), true);
    MILO_ASSERT(m_pThumbGroup, 0x178);
}

void ScrollbarDisplay::SetList(BandList *blist) {
    m_pList = blist;
    if (!m_pList)
        SetListAttached(false);
}

void ScrollbarDisplay::SetListAttached(bool b) {
    BandList *blist = GetList();
    if (b && blist) {
        SetTransParent(blist, false);
    } else {
        PanelDir *pDir = dynamic_cast<PanelDir *>(Dir());
        MILO_ASSERT(pDir, 0x197);
        SetTransParent(pDir, false);
    }
}

bool ScrollbarDisplay::GetListAttached() const {
    // Retail reuses the (already-zero) blist register as the false result
    // instead of pre-zeroing a separate one -- i.e. a short-circuit `&&`
    // expression, not the Wii dev build's nested if/assign form.
    BandList *blist = GetList();
    RndTransformable *parent = TransParent();
    return blist && blist == parent;
}

void ScrollbarDisplay::SetAlwaysShow(bool b) { mAlwaysShow = b; }
void ScrollbarDisplay::SetListXOffset(float f) { mListXOffset = f; }
void ScrollbarDisplay::SetListYOffset(float f) { mListYOffset = f; }
void ScrollbarDisplay::SetMinThumbHeight(float f) { mMinThumbHeight = f; }
void ScrollbarDisplay::SetHeight(float f) {
    mScrollbarHeight = f;
    if (mScrollbarHeight < mMinThumbHeight)
        mScrollbarHeight = mMinThumbHeight;
}

BEGIN_HANDLERS(ScrollbarDisplay)
    HANDLE_SUPERCLASS(UIComponent)
    HANDLE_CHECK(0x1E9)
END_HANDLERS

// Retail's SyncProperty uses function-local static Symbols (one shared guard
// word, seven bits) rather than the utl/Symbols.h globals.
BEGIN_PROPSYNCS(ScrollbarDisplay)
    static Symbol scrollbar_list("scrollbar_list");
    SYNC_PROP_SET(scrollbar_list, GetList(), SetList(_val.Obj<BandList>()))
    static Symbol always_show("always_show");
    SYNC_PROP_SET(always_show, GetAlwaysShow(), SetAlwaysShow(_val.Int()))
    static Symbol list_attached("list_attached");
    SYNC_PROP_SET(list_attached, GetListAttached(), SetListAttached(_val.Int()))
    static Symbol list_x_offset("list_x_offset");
    SYNC_PROP_SET(list_x_offset, GetListXOffset(), SetListXOffset(_val.Float()))
    static Symbol list_y_offset("list_y_offset");
    SYNC_PROP_SET(list_y_offset, GetListYOffset(), SetListYOffset(_val.Float()))
    static Symbol scrollbar_height("scrollbar_height");
    SYNC_PROP_SET(scrollbar_height, GetHeight(), SetHeight(_val.Float()))
    static Symbol min_thumb_height("min_thumb_height");
    SYNC_PROP_SET(min_thumb_height, GetMinThumbHeight(), SetMinThumbHeight(_val.Float()))
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS
