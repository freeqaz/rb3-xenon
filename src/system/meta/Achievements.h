#pragma once
#include "obj/Object.h"
#include "xdk/XAPILIB.h"
#include <vector>

class LocalUser;

/**
 * @brief Handles Achievements.
 */
// sizeof = 0x3c. NOT 0x40, and the per-member offsets below were all stale by +4.
// Compiler-verified (scripts/harvest/class_layout_report.py Achievements) AND
// confirmed against retail bytes three ways: the dynamic initializer allocates
// `li r3, 0x3c`; the ctor stores 0 at 0x28, 1 at 0x2c, and zeroes the vector's
// three words at 0x30/0x34/0x38 (ending exactly at 0x3c). No layout change was
// needed -- only these comments were wrong.
class Achievements : public Hmx::Object {
private:
    Achievements();

    int unk2c; // 0x28 (name kept for churn-safety; the real offset is 0x28)
    bool mAllowAchievements; // 0x2c
    std::vector<XUSER_ACHIEVEMENT> mAchieved; // 0x30

    XUSER_ACHIEVEMENT GetAchievementData(int, int);

    static std::vector<XUSER_ACHIEVEMENT> gThreadAchievements;
    static int SubmitAchievementsFunc();
    static void SubmitAchievementsCallback(int);

public:
    virtual DataNode Handle(DataArray *, bool);

    void Poll();
    void Submit(LocalUser *, Symbol, int);
    void SetAllowAchievements(bool allow) { mAllowAchievements = allow; }
    // ProfileMgr::Init() publishes the profile save-buffer size here (retail
    // 0x82548418 stores it to TheAchievements+0x28 right before handing the same
    // size to TheMemcardMgr.SetProfileSaveBuffer). unk2c is written nowhere else
    // in the binary and read nowhere inside Achievements itself, so the consumer
    // is presumably the Xbox platform TU.
    void SetProfileSaveSize(int size) { unk2c = size; }

    static void Init();
    static void PlatformInit();
    static void Terminate();
};

extern Achievements *TheAchievements;
