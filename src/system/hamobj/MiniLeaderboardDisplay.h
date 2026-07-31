#pragma once
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "ui/ResourceDirPtr.h"
#include "ui/UIComponent.h"
#include "utl/MemMgr.h"

/** "Mini Leaderboard Display" */
class MiniLeaderboardDisplay : public UIComponent {
public:
    // Hmx::Object
    virtual ~MiniLeaderboardDisplay();
    OBJ_CLASSNAME(MiniLeaderboardDisplay);
    OBJ_SET_TYPE(MiniLeaderboardDisplay);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    // UIComponent
    UICOMP_DC3_VIRTUAL void OldResourcePreload(BinStream &);

    OBJ_MEM_OVERLOAD(0x11)
    NEW_OBJ(MiniLeaderboardDisplay)
    static void Init();

protected:
    MiniLeaderboardDisplay();

    virtual void Update();
    // NOTE(lane NCCC-0731-ab7e/f8/sonnet): retail RB3 has no MiniLeaderboardDisplay
    // ::mResourceDir of its own -- it reaches the dir through the INHERITED
    // UIComponent::mResource (a UIResource* at 0x108) via mResource->Dir(), same
    // treatment lane BQ-2 gave MeterDisplay (see hamobj/MeterDisplay.h). Evidence:
    // ?SetType@MiniLeaderboardDisplay@@ had 12 diff_arg mismatches, all the same
    // uniform stack-offset shift (retail 0x144/0x148 vs our then-0x150/0x154) -- a
    // vbase-displacement defect from an own member sitting before the Object
    // virtual base. Dropping the 16-byte ResourceDirPtr overshot by 4 bytes (shift
    // flipped to a uniform +4/-4), so retail keeps a small 4-byte-aligned own
    // member here. The rb3-Wii dev oracle's MiniLeaderboardDisplay.h has exactly
    // one own data member -- `bool mAllowSoloScores;` -- which pads to 4 bytes
    // before the vbase, matching the residual shift precisely.
    bool mAllowSoloScores;
};
