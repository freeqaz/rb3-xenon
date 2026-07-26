#pragma once
#include "obj/ObjMacros.h"
#include "ui/PanelDir.h"
#include "utl/Symbol.h"
#include <vector>

class BandList;

class OvershellDir : public PanelDir {
public:
    OvershellDir();
    OBJ_CLASSNAME(OvershellDir);
    OBJ_SET_TYPE(OvershellDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual ~OvershellDir() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);

    void CacheLists();
    void ConcealAllLists(bool);
    void ViewChanged();
    void SetDefaultOption(Symbol o) { mDefaultOption = o; }
    void SetDefaultOptionIndex(int idx) { mDefaultOptionIndex = idx; }

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(OvershellDir)
    REGISTER_OBJ_FACTORY_FUNC(OvershellDir)
    static void Init() { Register(); }

    Symbol mSlotView;
    bool mInTrackMode;
    Symbol mControllerType;
    bool mOnlineEnabled;
    bool mIsLocal;
    int mPadNum;
    Symbol mPlatform;
    std::vector<BandList *> mBandLists;
    Symbol mDefaultOption;
    int mDefaultOptionIndex;
};
