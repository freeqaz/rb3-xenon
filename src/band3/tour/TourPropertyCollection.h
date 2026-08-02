#pragma once
#include "meta/FixedSizeSaveable.h"
#include "utl/Symbol.h"
#include <hash_map>

// mTourProperties is a Harmonix `hash_map` keyed on Symbol in retail X360, not a
// std::map. Adjudicated on RETAIL BYTES, confounder-immune: our source has
// exactly two `mTourProperties[...]` sites (SetPropertyValue, FakeFill) and
// retail's TourPropertyCollection.s calls
// hash_map<Symbol,float,...>::operator[] exactly twice, plus one hash_map
// default ctor for the member. The value type (M = float) matches too.
// Corroboration: retail's FixedSizeSaveable::SaveStd/LoadStd, which SaveFixed
// and LoadFixed call with this member, are instantiated on hash_map, not map.
// The Wii decomp approximated it as std::map.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class TourPropertyCollection : public FixedSizeSaveable {
public:
    TourPropertyCollection();
    virtual ~TourPropertyCollection();
    virtual void SaveFixed(FixedSizeSaveableStream &) const;
    virtual void LoadFixed(FixedSizeSaveableStream &, int);

    float GetPropertyValue(Symbol) const;
    void SetPropertyValue(Symbol, float);
    void Clear();
    void FakeFill();

    static int SaveSize(int);

    std::hash_map<Symbol, float> mTourProperties; // 0x8
};