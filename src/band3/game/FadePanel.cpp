#include "game/FadePanel.h"
#include "math/Geo.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "rndobj/Rnd.h"
#include "synth/Faders.h"
#include "synth/Synth.h"
#include "ui/UIPanel.h"
#include "utl/Symbols.h"
#include <cstddef>

FadePanel::FadePanel()
    : unk_0x38(0), mColor(0, 0, 0, 1), mFadeSynth(0),
      mSavedVolume(TheSynth->GetMasterVolume()), mFadeOut(true) {}

void FadePanel::Unload() {
    if (mFadeSynth)
        TheSynth->SetMasterVolume(mSavedVolume);
    UIPanel::Unload();
}

void FadePanel::StartFade(
    float vol, const Hmx::Color &color, bool fade_synth, bool fade_out
) {
    unk_0x38 = true;
    mTimer.Restart();
    mVolume = vol;
    unk_0x84 = 0;
    mColor = color;
    mFadeOut = fade_out;
    mFadeSynth = fade_synth;
    mSavedVolume = TheSynth->GetMasterVolume();
    if (fade_synth) {
        TheSynth->MasterFader()->DoFade(-96.f, vol);
    }
}

void FadePanel::Enter() {
    UIPanel::Enter();
    mFadeSynth = false;
    mFadeOut = true;
    unk_0x38 = false;
}

void FadePanel::Poll() {
    UIPanel::Poll();
    mTimer.Split();
    if (unk_0x38 != 0) {
        float ratio;
        if (mTimer.Ms() > mVolume) {
            ratio = 1.0f;
        } else {
            ratio = mTimer.Ms() / mVolume;
        }
        unk_0x84 = ratio;
        if (!mFadeOut) {
            unk_0x84 = 1.0f - ratio;
        }
    } else {
        unk_0x84 = mFadeOut ? 0.0f : 1.0f;
    }
}

void FadePanel::Draw() {
    UIPanel::Draw();
    static Hmx::Rect screen_rect(0.0f, 0.0f, (float)TheRnd.Width(), (float)TheRnd.Height());
    if (unk_0x38) {
        mColor.alpha = unk_0x84;
        TheRnd.DrawRect(screen_rect, mColor, NULL, NULL, NULL);
    }
}

BEGIN_HANDLERS(FadePanel)
    HANDLE(start_fade, OnStartFade)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(98)
END_HANDLERS

DataNode FadePanel::OnStartFade(DataArray *array) {
    Hmx::Color col(0, 0, 0, 1);
    array->FindData("color", col, false);
    bool fade_synth = true;
    array->FindData("fade_synth", fade_synth, false);
    bool fade_out = true;
    array->FindData("fade_out", fade_out, false);

    StartFade(array->Float(2), col, fade_synth, fade_out);
    return 0;
}
