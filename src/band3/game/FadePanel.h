#pragma once
#include "obj/Data.h"
#include "obj/ObjMacros.h" // DECLARE_REVS
#include "ui/UIPanel.h"

class FadePanel : public UIPanel {
public:
    FadePanel();
    // NO destructor declared -- retail's ??_GFadePanel has NO derived vptr-restore
    // (lane W16-X 2026-09-14). A user-declared dtor, even `{}`, makes MSVC emit
    // that prologue in ??1FadePanel, bloating ??_DFadePanel past the ??_G inline threshold:
    // 80 B via ??_D instead of retail's direct ??1UIPanel + ??1Hmx::Object.
    OBJ_CLASSNAME(FadePanel)
    OBJ_SET_TYPE(FadePanel)
    virtual DataNode Handle(DataArray *, bool);

    virtual void Unload();
    virtual void Enter();
    virtual void Poll();
    virtual void Draw();

    void StartFade(float, const Hmx::Color &color, bool fade_synth, bool fade_out);
    DataNode OnStartFade(DataArray *);

    DECLARE_REVS;
    NEW_OBJ(FadePanel)
    static void Init() { REGISTER_OBJ_FACTORY(FadePanel) }

    u8 unk_0x38;
    Timer mTimer; // 0x40
    float mVolume; // 0x70
    Hmx::Color mColor; // 0x74
    float unk_0x84;

    /** Whether this panel should also fade audio. */
    bool mFadeSynth; // 0x88

    /** The original master volume pre-fade. */
    float mSavedVolume; // 0x8C
    bool mFadeOut; // 0x90
};