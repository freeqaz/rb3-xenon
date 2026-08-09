#pragma once
#include "obj/Object.h"
#include "meta/MetaMusicScene.h"
#include <hash_map>

// Keying a hash_map on Symbol REQUIRES this specialization to be visible before
// the first instantiation.  Without it two things break, and only one of them
// shows up in the TU you are editing: this header's own standalone TU fails
// with _hashtable.h C2064 (the primary template is not callable), and -- worse
// -- any TU that includes this header before SongMgr.h fails with C2908
// ("hash<_Key> has already been instantiated"), because instantiating the
// primary template here makes SongMgr.h's later explicit specialization
// illegal.  MoviePanel.cpp hid both by pulling the specialization in
// transitively.  Same RB3_HASH_SYMBOL_DEFINED guard the other nine headers use.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
#if HX_NATIVE
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

class MetaMusicManager : public Hmx::Object {
public:
    MetaMusicManager(DataArray *);
    virtual ~MetaMusicManager();
    virtual DataNode Handle(DataArray *, bool);

    Symbol GetSceneForScreen(Symbol) const;
    MetaMusicScene *GetScene(Symbol) const;

    bool SceneExists(Symbol s) const { return GetScene(s); }
    bool SceneForScreenExists(Symbol s) const { return GetSceneForScreen(s) != gNullStr; }

private:
    void Init(DataArray *);
    void Cleanup();
    bool IsScreenInSceneMap(Symbol) const;
    void ConfigureMetaMusicSceneData(DataArray *);

    // Retail keys both of these with STLport hash_maps, not the Wii build's
    // std::maps.  ??0MetaMusicManager@@QAA@PAVDataArray@@@Z does
    //   addi r3, r30, 0x28 ; bl ??0?$hash_map@...@stlpmtx_std@@QAA@XZ
    //   addi r3, r30, 0x44 ; bl ??0?$hash_map@...@stlpmtx_std@@QAA@XZ
    // (retail 0x8255D480, body `li r4, 0x64` -> _M_initialize_buckets(100), a
    // hashtable ctor; a std::map ctor is inlined and makes no call at all).
    //
    // The spacing settles the size independently of the callee name:
    // 0x44 - 0x28 = 0x1c, which is sizeof(hash_map); a map would put the second
    // container at 0x40.  The compiler confirms m_mapScenes is already at 0x28
    // (/d1reportSingleClassLayoutMetaMusicManager) -- the old `// 0x1c` comment
    // here was simply wrong, so nothing before these members needs to move.
    // Both are trailing members, so widening them 0x18 -> 0x1c shifts nothing
    // else and needs no compensating pad.
    std::hash_map<Symbol, MetaMusicScene *> m_mapScenes; // 0x28
    std::hash_map<Symbol, Symbol> m_mapScreenToScene; // 0x44
};

extern MetaMusicManager *TheMetaMusicManager;
