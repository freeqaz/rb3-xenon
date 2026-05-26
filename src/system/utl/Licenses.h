#pragma once

class Licenses {
public:
    enum Requirement {
        kRequirementNotification,
        kRequirementDoNotDistribute
    };
    Licenses(const char *, Requirement);

    static void PrintAll(void);

protected:
    static Licenses *sFront;
    static int sInited;

private:
    Requirement mRequirement; // 0x0
    const char *mName; // 0x4
    Licenses *mNext; // 0x8
};
