#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "utl/Symbol.h"
#include <hash_map>
#include <vector>

// W16-AQ 2026-09-14: retail holds a hash_map here, not a std::map. Proven on
// retail bytes: ??1NameGenerator calls 0x8260ffd8 (hashtable::clear) then
// 0x826100f8, whose 104 B body is byte-identical (fuzzy 100, incl. relocation
// names) to our compiled hashtable<pair<const int,UIComponent*>,...>::~hashtable
// in default/CharacterCreatorPanel. Same correction the AccomplishmentProgress.h
// header records: "the Wii decomp approximated them as std::map".
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class NameGenerator : public Hmx::Object { // 0x34
public:
    NameGenerator(DataArray *);
    virtual ~NameGenerator();
    virtual DataNode Handle(DataArray *, bool);

    void Cleanup();
    void Init(DataArray *);
    void ConfigureNameData(DataArray *);
    DataArray *GetNameList(Symbol) const; // i think????
    Symbol GetRandomNameFromList(Symbol);

    std::hash_map<Symbol, DataArray *> m_mapNameLists;
};

// NOTE(AndrewB):
// Marking this as static like all(?) the other singletons
// causes linking errors w/ a TU (CharacterCreatorPanel.o)
// that hasn't been matched yet, no idea if this could cause
// issues in the future when that TU *is* matched
// but it links fine like this right now, so...
extern NameGenerator *TheNameGenerator;
