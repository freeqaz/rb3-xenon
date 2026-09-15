#pragma once

#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "ui/UIComponent.h"
#include "utl/BinStream.h"

/** "Mini Leaderboard Display" -- RB3 (band3) layout.
 *  NOTE: this is the RB3 game's bandobj MiniLeaderboardDisplay, NOT the DC3
 *  hamobj version (which carries a DC3-only OldResourcePreload virtual and a
 *  mResourceDir member). AppMiniLeaderboardDisplay.h pulls this in via
 *  "bandobj/MiniLeaderboardDisplay.h" so it resolves to the clean RB3 layout.
 */
class MiniLeaderboardDisplay : public UIComponent {
public:
    MiniLeaderboardDisplay();
    virtual ~MiniLeaderboardDisplay();
    OBJ_CLASSNAME(MiniLeaderboardDisplay)
    OBJ_SET_TYPE(MiniLeaderboardDisplay)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void DrawShowing();

    bool mAllowSoloScores; // 0x10c

    // Retail's ?NewObject@AppMiniLeaderboardDisplay@@ at 0x8264c828 calls
    // ?StaticClassName@MiniLeaderboardDisplay@@, not @UIComponent, so retail
    // gives THIS class its own operator new and the derived App class inherits
    // it. Without it our row's only charged site was that relocation name
    // (25/28 words equal, fuzzy 99.821). Positive control: the same-named
    // hamobj/MiniLeaderboardDisplay already carries
    // OBJ_MEM_OVERLOAD_INLINE_DEL(0x11) and its NewObject row is fuzzy 100.0.
    //
    // operator new ONLY, for the reason spelled out in StarDisplay.h: declaring
    // an owned operator delete here lets MSVC inline it into the NewObject
    // unwind funclet, where retail calls the out-of-line ICF survivor
    // ??3BinStream@@SAXPAX@Z. Delete stays inherited from UIComponent, so
    // ??_GAppMiniLeaderboardDisplay stays at fuzzy 100. Body form is
    // MemMgr.h's OBJ_MEM_OVERLOAD verbatim (`.Str()` + named `mem`).
    static void *operator new(unsigned int s) {
        (void)StaticClassName().Str();
        void *mem = (MemAlloc)(s, 0);
        return mem;
    }
    static void *operator new(unsigned int s, void *place) { return place; }

    NEW_OBJ(MiniLeaderboardDisplay)
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(MiniLeaderboardDisplay) }

    DECLARE_REVS;
};
