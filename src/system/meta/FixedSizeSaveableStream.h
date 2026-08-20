#pragma once
#include "utl/BufStream.h"
#include <hash_map>

// Retail RB3-360 FixedSizeSaveableStream's two "map" members are actually STLport
// hash_map (Harmonix's original symbol-table type). GetSymbol's find inlines the
// int-key out-of-line hashtable::find COMDAT (lbl_82552CD0) with a NULL-miss
// sentinel and value at slist node+0x8 — not _Rb_tree::_M_find. sizeof(hash_map)
// = 0x1c (the _M_max_load_factor float at +0x18), so the second map lands at 0x4c
// rather than the std::map 0x48. The ctor inits +0x4c (m_mapIDToSymbol) then +0x30
// (m_mapSymbolToID), the SetSymbolID path uses hash_map<Symbol,int>::operator[]
// (fn_82590258 -> Symbol-key find fn_82543F88) and hash_map<int,Symbol> insert
// (fn_82561180). hash<Symbol> hashes the interned char* word identity, matching
// retail exactly.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
#if HX_NATIVE
// Native: hash_map aliases std::unordered_map, which defaults to std::hash<K>.
// STLport's stlpmtx_std / _STLP_TEMPLATE_NULL spellings don't exist here.
namespace std {
template <> struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#else
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif
#endif

class FixedSizeSaveableStream : public BufStream {
public:
    FixedSizeSaveableStream(void *, int, bool);
    virtual ~FixedSizeSaveableStream();
    // NOT virtual: retail's FixedSizeSaveableStream vtable has 11 slots -- the
    // SAME count as its BufStream base -- so retail adds no new virtuals here.
    // Ours measured 14.  The +3 decomposes exactly: BufStream::Size (fixed in
    // BufStream.h) plus these two, which are declared here and never called,
    // never overridden, and have no subclass to dispatch to.
    bool FinishWrite() { return 0; }
    bool FinishStream() { return 0; }

    bool HasSymbol(Symbol) const;
    bool HasID(int) const;
    int GetID(Symbol) const;
    int AddSymbol(Symbol);
    Symbol GetSymbol(int) const;
    void InitializeTable();
    int ReadInt();
    float ReadFloat();
    void SetSymbolID(Symbol, int);
    void SaveTable();
    void LoadTable(int);

    std::hash_map<Symbol, int> &GetSymbolToIDMap();
    static int GetSymbolTableSize(int);

    std::hash_map<Symbol, int> m_mapSymbolToID; // 0x30
    std::hash_map<int, Symbol> m_mapIDToSymbol; // 0x4c
    int m_iCurrentID; // 0x68
    int m_iTableOffset; // 0x6c
};
