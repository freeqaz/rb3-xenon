#pragma once
#include "math/Color.h"
#include "obj/Object.h"
#include "utl/MemMgr.h"

struct ColorSet {
    Hmx::Color mPrimary;   // 0x0
    Hmx::Color mSecondary; // 0x10
    // Retail RB3-360 sizes ColorSet at 0x44 (the vector<ColorSet> in
    // ColorPalette::Load uses a 0x44 element stride / memcpy, see the three
    // `li r5,0x44` sites in the target). DC3/rb3-Wii shrank ColorSet to just
    // the two colors (0x20); RB3 retained 0x24 of trailing (non-serialized)
    // storage. Only the two colors are read by operator>>; the tail is unused
    // scratch, so it is represented as trailing padding to match the size.
    char mPad[0x24]; // 0x20  (retail-only; not present in DC3/rb3-Wii ColorSet)
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

    OBJ_MEM_OVERLOAD(0x14)
    NEW_OBJ(ColorPalette)

    int NumColors() const { return mColors.size(); }
    const Hmx::Color &GetColor(int idx) const;

protected:
    ColorPalette();

    /** "Color for materials" */
    std::vector<Hmx::Color> mColors; // 0x2c
};
