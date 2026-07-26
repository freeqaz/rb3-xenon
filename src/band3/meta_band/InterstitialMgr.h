#pragma once
#include "net/Synchronize.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include <hash_map>

// Retail X360 stores the interstitial tables in STLport hash_maps, not
// std::maps: GetInterstitialsFromScreen iterates the mapped container as a
// NULL-terminated slist (head at container+0x4, node = {next@0x0, key@0x4,
// value@0x8}) and operator[] resolves through the shared hashtable find
// COMDAT fn_82557770 (bucket vector at container+0x8/+0xc, `divwu` on the
// interned char* word). sizeof(hashtable) = 0x1c, identical to the padded
// std::map that used to sit here, so member offsets are unchanged.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class InterstitialMgr : public Synchronizable, public Hmx::Object {
public:
    InterstitialMgr();
    virtual ~InterstitialMgr() {}
    virtual void SyncSave(BinStream &, unsigned int) const;
    virtual void SyncLoad(BinStream &, unsigned int);
    virtual bool HasSyncPermission() const;
    virtual DataNode Handle(DataArray *, bool);

    void SetFromConfig();
    void RefreshRandomSelection();
    void GetInterstitialsFromScreen(UIScreen *, std::vector<UIPanel *> &);
    UIPanel *PickInterstitialBetweenScreens(const char *, const char *);
    UIScreen *CurrentInterstitialToScreen(UIScreen *) const;
    void PrintOverlay(UIScreen *, UIScreen *);
    void CycleRandomOverride();

    std::hash_map<Symbol, std::hash_map<Symbol, DataArray *> >
        mScreenInterstitialMap; // 0x48 (hashtable, 0x1c)
    std::hash_map<Symbol, UIScreen *> mCurrentInterstitials; // 0x64 (hashtable, 0x1c)
    // mRandomSelection is the LAST member in retail: `new InterstitialMgr` in
    // BandUI::Init is `li r3, 0x84` = 0x48 (bases) + 0x1c + 0x1c + 4. The
    // rb3-Wii `mRandomOverride` member does not exist here — retail never
    // touches this+0x84 anywhere in the unit (every 0x84 reference in the target
    // is an EH frame slot off r31). It lives as the file-static
    // `sRandomOverride` in InterstitialMgr.cpp instead; CycleRandomOverride
    // itself IS present in retail (its two guarded local statics are what give
    // the unit's ??__F atexit thunks their guard-bit indices 0 and 1).
    int mRandomSelection; // 0x80
};