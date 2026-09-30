#pragma once
#include "math/Color.h"
#include "obj/Object.h"
#include "utl/MemMgr.h"

struct ColorSet {
    Hmx::Color mPrimary;   // 0x0
    Hmx::Color mSecondary; // 0x10
    // 0x20, as in DC3/rb3-Wii. W16-HM re-measured on retail bytes: the
    // vector<ColorSet> reader (0x824DFE48), resize (0x826F95C0) and
    // ColorPalette::Load (0x824DFEB0) all step by 0x20 (addi 0x20 / srawi 5).
    // An earlier 0x24-byte tail pad, justified by `li r5,0x44` sites, was wrong.
};

/**
 * @brief Contains a set of colors.
 * Original _objects description:
 * "List of primary/secondary colors for OutfitConfig"
 */
class ColorPalette : public Hmx::Object {
    friend class BandSwatch;
public:
    OBJ_CLASSNAME(ColorPalette);
    OBJ_SET_TYPE_ENGINE(ColorPalette);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    OBJ_MEM_OVERLOAD_INLINE_DEL(0x14)
    NEW_OBJ(ColorPalette)

    int NumColors() const { return mColors.size(); }
    // Defined inline (not out-of-line in Crowd.cpp) because retail INLINES this
    // into its callers: OutfitConfig::MatSwap::Compose emits the whole body --
    // `lwz 0x28/0x2c` (mColors begin/end), `srawi 4` for size(), then the
    // twllei/divwu/mullw/subf modulo -- with no `bl` to GetColor at all. With
    // /O1 (/Ob2, no LTCG) that is only reachable if the definition is visible in
    // the header, so the out-of-line placement was a porting artifact.
    const Hmx::Color &GetColor(int idx) const {
        MILO_ASSERT(mColors.size(), 0x18);
        int colorIdx = idx % mColors.size();
        return mColors[colorIdx];
    }

protected:
    ColorPalette();

    /** "Color for materials" */
    // 0x28, NOT 0x2c: cl.exe /d1reportSingleClassLayoutColorPalette puts mColors
    // at 0x28 (sizeof(ColorPalette) == 0x34), and retail agrees -- Compose reads
    // the vector's begin/end as `lwz 0x28(r11)` / `lwz 0x2c(r11)`. The old 0x2c
    // comment was the vector's _M_finish, off by one field.
    std::vector<Hmx::Color> mColors; // 0x28
};
