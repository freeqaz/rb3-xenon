#pragma once
#include "obj/Data.h"
#include "utl/Symbol.h"
#include <hash_map>

// unk4 is a Harmonix `hash_map` keyed on Symbol in retail X360, not a std::map:
// the retail ctor calls hash_map<Symbol,_>::hash_map() out-of-line at this+4,
// and retail's own ConfigureQuestWeightData calls
// hash_map<Symbol,float,...>::operator[]. The Wii decomp approximated it as
// std::map. hash<Symbol> hashes the interned char* word identity, matching
// retail. Guarded so this and another hash<Symbol>-defining header can coexist
// in one TU without an ODR clash.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class TourWeightManager {
public:
    TourWeightManager();
    virtual ~TourWeightManager();
    virtual void Init(const DataArray *);

    void Cleanup();
    void ConfigureQuestWeightData(DataArray *);

    std::hash_map<Symbol, float> unk4;
};