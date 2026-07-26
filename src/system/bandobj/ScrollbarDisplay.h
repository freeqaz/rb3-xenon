#pragma once
#include "obj/ObjMacros.h"
#include "ui/UIComponent.h"
#include "bandobj/BandList.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"

class ScrollbarDisplay : public UIComponent {
public:
    ScrollbarDisplay();
    OBJ_CLASSNAME(ScrollbarDisplay);
    OBJ_SET_TYPE(ScrollbarDisplay);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();
    virtual ~ScrollbarDisplay();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void Enter();
    virtual void CopyMembers(const UIComponent *, CopyType);
    virtual void Update();

    BandList *GetList() const { return m_pList.Ptr() ? m_pList.Ptr() : 0; }
    bool GetAlwaysShow() const { return mAlwaysShow; }
    float GetListXOffset() const { return mListXOffset; }
    float GetListYOffset() const { return mListYOffset; }
    float GetHeight() const { return mScrollbarHeight; }
    float GetMinThumbHeight() const { return mMinThumbHeight; }
    float GetSavedPosition() const { return m_fSavedPosition; }
    float GetSavedScale() const { return m_fSavedScale; }
    void SetList(BandList *);
    void SetAlwaysShow(bool);
    void SetListAttached(bool);
    bool GetListAttached() const;
    void SetListXOffset(float);
    void SetListYOffset(float);
    void SetHeight(float);
    void SetMinThumbHeight(float);
    void UpdateThumbScaleAndPosition();
    void UpdateSavedListInfo();
    float GetListHeight() const;
    void UpdateScrollbarHeightAndPosition();

    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(ScrollbarDisplay); }
    NEW_OBJ(ScrollbarDisplay);

    // NOTE: gRev/gAltRev are TU-local statics in ScrollbarDisplay.cpp, not
    // class statics -- see the comment there (retail addresses them off one
    // shared base, which only works for statics the compiler lays out itself).
    // See MicInputArrow.h: retail inlines the class operator new into
    // NewObject and still evaluates StaticClassName(); the tree-wide
    // OBJ_MEM_OVERLOAD is noinline and its name argument is swallowed by the
    // MemAlloc debug-arg macro, so spell it out locally.
    static void *operator new(unsigned int s) {
        StaticClassName();
        return MemAlloc(s, __FILE__, 0x30, "ScrollbarDisplay", 0);
    }
    static void *operator new(unsigned int s, void *place) { return place; }
    DELETE_OVERLOAD;

    // UIComponent is 0x140 on retail-360 (not the 0x10c the Wii header assumes).
    ObjPtr<BandList> m_pList; // 0x140
    float mScrollbarHeight; // 0x14c
    bool mAlwaysShow; // 0x150
    float mListXOffset; // 0x154
    float mListYOffset; // 0x158
    float mMinThumbHeight; // 0x15c
    RndMesh *m_pTopBone; // 0x160
    RndMesh *m_pBottomBone; // 0x164
    RndMesh *m_pThumbTopBone; // 0x168
    RndMesh *m_pThumbBottomBone; // 0x16c
    RndGroup *m_pThumbGroup; // 0x170
    int unk140; // 0x174
    int unk144; // 0x178
    int unk148; // 0x17c
    int unk14c; // 0x180
    float m_fSavedPosition; // 0x184
    float m_fSavedScale; // 0x188
};
