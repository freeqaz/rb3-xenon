#include "obj/ObjMacros.h"
#include "meta_band/RetryAudioPanel.h"
#include "ContextChecker.h"
#include "meta_band/VoiceoverPanel.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/System.h"
#include "ui/UIPanel.h"
#include "utl/Messages2.h"

RetryAudioPanel::RetryAudioPanel() : mFinished(0) {}

RetryAudioPanel::~RetryAudioPanel() {}

void RetryAudioPanel::PollForLoading() {
    UpdateVoiceoverState();
    bool done = !DoneLoading();
    if (!mFinished && done) {
        mFinished = true;
        Handle(handle_audio_finished_msg, true);
    }
}

bool RetryAudioPanel::IsLoaded() const {
    if (!UIPanel::IsLoaded())
        return false;
    return mFinished;
}

inline static const char *VOContextStr() { return "vo_retry_context"; }

inline Symbol RetryAudioPanel::RandomVOContextItem() {
    DataArray *cfg = SystemConfig(VOContextStr());
    return RandomContextSensitiveItem(cfg, false);
}

void RetryAudioPanel::Load() {
    UIPanel::Load();
    mFinished = false;
    Symbol item = RandomVOContextItem();
    if (item != "") {
        SetVoiceoverSymbol(item);
        // No SetPreLoad here: that was the rb3-Wii BinkClip preload flag.  Retail
        // 0x826305B0 goes straight from SetVoiceoverSymbol (0x8262FDE8) to
        // PlayVoiceover (0x8262F6B0) with nothing in between.
        PlayVoiceover();
    }
}

void RetryAudioPanel::Enter() { UIPanel::Enter(); }

BEGIN_HANDLERS(RetryAudioPanel)
    HANDLE_SUPERCLASS(VoiceoverPanel)
    HANDLE_CHECK(0x6B)
END_HANDLERS