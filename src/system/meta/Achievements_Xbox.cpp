#include "meta/Achievements.h"
#include "xdk/XAPILIB.h"

void Achievements::PlatformInit() {}

XUSER_ACHIEVEMENT Achievements::GetAchievementData(int i1, int i2) {
    XUSER_ACHIEVEMENT a;
    a.dwAchievementId = i2;
    a.dwUserIndex = i1;
    return a;
}

int Achievements::SubmitAchievementsFunc() {
    return XUserWriteAchievements(
        gThreadAchievements.size(), gThreadAchievements.begin(), nullptr
    );
}
