#pragma once
#include "bandobj/BandCharDesc.h"
#include "bandobj/OutfitConfig.h"
#include "meta_band/ClosetMgr.h"
#include "ui/UIPanel.h"
#include "world/ColorPalette.h"
#include <hash_map>

class ChooseColorPanel : public UIPanel {
public:
    ChooseColorPanel();
    OBJ_CLASSNAME(ChooseColorPanel);
    OBJ_SET_TYPE(ChooseColorPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Draw();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    void AddColorOption(int, ColorPalette *);
    int GetCurrentColor();
    void PreviewColor(int);
    ColorPalette *GetColorPalette() { return mColorOptions[mCurrentOption]; }
    NEW_OBJ(ChooseColorPanel);
    static void Init() { REGISTER_OBJ_FACTORY(ChooseColorPanel); }

    ClosetMgr *mClosetMgr; // 0x38
    OutfitConfig *mCurrentOutfitConfig; // 0x3c
    BandCharDesc::OutfitPiece *mCurrentOutfitPiece; // 0x40
    // Retail RB3-360 uses an STLport hash_map here, not the Wii build's
    // std::map (same divergence SongStatusMgr.h documents). Adjudicated on
    // RETAIL BODY SIZES, which are independent of the symbol name's value-type
    // component (that part is ICF-folded and unreliable):
    //   ctor       retail calls a 76 B body; hash_map ctor = 76 B (4 retail /
    //              29 our instances), map ctor = 84 B (64 our instances)
    //   operator[] retail calls a 120 B body; hash_map op[] = 120 B (20
    //              instances), map op[] = 160 B (22 instances)
    // Layout-neutral: the compiler reports BOTH containers at 0x1c, so the
    // members after this one do not move (mNumOptions @0x64, mCurrentOption
    // @0x68 — compiler-verified, and note the old "// 0x44" comment was wrong;
    // the real offset is 0x48). Lane DS-1.
    std::hash_map<int, ColorPalette *> mColorOptions; // 0x48, 0x1c bytes
    int mNumOptions; // 0x5c
    int mCurrentOption; // 0x60
};