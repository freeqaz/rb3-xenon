#pragma once
#include "game/BandUser.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/Award.h"
#include "meta_band/TexLoadPanel.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "ui/UIListProvider.h"
#include "utl/Symbol.h"

class AwardAssetProvider : public UIListProvider, public Hmx::Object {
public:
    AwardAssetProvider(const std::vector<DynamicTex *> &icons)
        : unk20(gNullStr), mIcons(icons), mMaleMat(0), mFemaleMat(0), mUnisexMat(0) {}
    virtual ~AwardAssetProvider() {}
    virtual void InitData(RndDir *);
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual Symbol DataSymbol(int i_iIndex) const;
    virtual int NumData() const { return unk24.size(); }

    void Update(Symbol s);

    RndMat *GetMatForName(String &str) const {
        std::vector<DynamicTex *>::const_iterator it =
            std::find(mIcons.begin(), mIcons.end(), str);
        RndMat *ret;
        if (it != mIcons.end())
            ret = (*it)->mMat;
        else {
            MILO_WARN("No Icon found for %s!", str.c_str());
            ret = nullptr;
        }
        return ret;
    }

    Symbol unk20; // 0x2c
    std::vector<Symbol> unk24; // 0x30
    const std::vector<DynamicTex *> &mIcons; // 0x3c
    RndMat *mMaleMat; // 0x40
    RndMat *mFemaleMat; // 0x44
    RndMat *mUnisexMat; // 0x48
};

class NewAwardPanel : public TexLoadPanel {
public:
    NewAwardPanel();
    OBJ_CLASSNAME(NewAwardPanel);
    OBJ_SET_TYPE(NewAwardPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Enter();
    virtual void Load();
    virtual void Unload();
    virtual void FinishLoad();

    void PopAndShowFirstAward();
    void LoadIcons();
    int GetNumAssets() const;
    NEW_OBJ(NewAwardPanel);
    static void Init() { REGISTER_OBJ_FACTORY(NewAwardPanel); }

    LocalBandUser *mUser; // 0x54
    Symbol mAwardName; // 0x58
    Symbol mAwardReason; // 0x5c
    AwardAssetProvider *m_pAwardAssetProvider; // 0x60
};