#include "obj/ObjMacros.h"
#include "meta_band/GameTimePanel.h"
#include "game/GamePanel.h"
#include "obj/Task.h"
#include "ui/UIPanel.h"
#include "utl/DeJitter.h"
#include "utl/Loader.h"

GameTimePanel::GameTimePanel() : mTempo(0) {}

void GameTimePanel::Load() {
    UIPanel::Load();
    mPeriod = TheLoadMgr.SetLoaderPeriod(10.0f);
}

void GameTimePanel::Unload() {
    UIPanel::Unload();
    TheLoadMgr.SetLoaderPeriod(mPeriod);
    TheGamePanel->unk150 = true;
}

void GameTimePanel::Exit() { UIPanel::Exit(); }

void GameTimePanel::Enter() {
    UIPanel::Enter();
    mTempo = TheTaskMgr.DeltaBeat() / TheTaskMgr.DeltaSeconds();
    TheGamePanel->unk150 = false;
    mStartSeconds = TheTaskMgr.Seconds(TaskMgr::kRealTime) + TheTaskMgr.DeltaSeconds();
    mTimer.Restart();
}

void GameTimePanel::Poll() {
    if (!TheGamePanel->unk150) {
        float secs = mTimer.SplitMs() * 0.001f + mStartSeconds;
        float delta;
        float dejitteredSecs =
            TheGamePanel->mDeJitter.NewMs(secs * 1000.0f, delta) * 0.001f;
        TheTaskMgr.SetSecondsAndBeat(
            dejitteredSecs, TheTaskMgr.Beat() + mTempo * delta * 0.001f, false
        );
    }
}
