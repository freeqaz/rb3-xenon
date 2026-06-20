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

    NEW_OBJ(MiniLeaderboardDisplay)
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(MiniLeaderboardDisplay) }

    DECLARE_REVS;
};
