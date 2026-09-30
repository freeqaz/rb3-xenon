#include "meta/Achievements.h"
#include "obj/Dir.h"

#include "obj/Object.h"
#include "os/PlatformMgr.h"
#include "os/User.h"
#include "os/ThreadCall.h"

Achievements *TheAchievements;
std::vector<XUSER_ACHIEVEMENT> Achievements::gThreadAchievements;

void Achievements::Terminate() { RELEASE(TheAchievements); }

BEGIN_HANDLERS(Achievements)
    HANDLE_ACTION(set_allow_achievements, SetAllowAchievements(_msg->Int(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

Achievements::Achievements() : unk2c(0), mAllowAchievements(true) {}

void Achievements::SubmitAchievementsCallback(int) { gThreadAchievements.clear(); }

void Achievements::Init() {
    TheAchievements = new Achievements();
    TheAchievements->SetName("achievements", ObjectDir::Main());
    PlatformInit();
}

void Achievements::Poll() {
    if (!mAchieved.empty() && gThreadAchievements.empty() && mAllowAchievements) {
        gThreadAchievements = mAchieved;
        mAchieved.clear();
        ThreadCall(SubmitAchievementsFunc, SubmitAchievementsCallback);
    }
}

#ifdef HX_NATIVE
void Achievements::PlatformInit() {
    // Xbox achievements not available on native
}

XUSER_ACHIEVEMENT Achievements::GetAchievementData(int padNum, int achievementId) {
    XUSER_ACHIEVEMENT data;
    memset(&data, 0, sizeof(data));
    return data;
}

int Achievements::SubmitAchievementsFunc() {
    return 0;
}
#endif

void Achievements::Submit(LocalUser *l, Symbol s2, int i3) {
#ifdef HX_NATIVE
    int i1 = l->GetPadNum();
    if (ThePlatformMgr.IsPadNumSignedIn(i1)) {
        MILO_LOG("Achievement awarded: %s (id:%d, pad:%i)\n", s2, i3, i1);
        mAchieved.push_back(GetAchievementData(i1, i3));
    }
#else
    // retail (0x827A2830): IsUserSignedIn(l), a discarded User::UserName()
    // (the stripped log's argument), then GetPadNum at the push_back.
    if (ThePlatformMgr.IsUserSignedIn(l)) {
        MILO_LOG("Achievement awarded: %s for %s\n", s2, l->UserName());
        mAchieved.push_back(GetAchievementData(l->GetPadNum(), i3));
    }
#endif
}
