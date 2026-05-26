#pragma once

class IdentityInfo {
public:
    IdentityInfo() : mIdentified(0), mEnrollmentIdx(-1), mProfileMatched(0), unk9(0), unkc(-1) {}
    void PostUpdate();

    bool ProfileMatched() const { return mProfileMatched; }
    int EnrollmentIndex() const { return mEnrollmentIdx; }
    void Init() { unkc = 0; }
    void Init(int skeletonIdx) { unkc = skeletonIdx; }
    void SetIdentified(bool b1) { mIdentified = b1; }
    void SetProfileMatched(bool b1) { mProfileMatched = b1; }
    void SetEnrollmentIndex(int enrollmentIdx) {
        if (mEnrollmentIdx != enrollmentIdx) {
            mEnrollmentIdx = enrollmentIdx;
            unk9 = true;
        }
    }
    void Reset(int skeletonIdx) {
        mIdentified = false;
        mEnrollmentIdx = -1;
        mProfileMatched = false;
        unk9 = false;
        unkc = skeletonIdx;
    }

private:
    void Identified(unsigned int);

    bool mIdentified; // 0x0
    int mEnrollmentIdx; // 0x4
    bool mProfileMatched; // 0x8
    bool unk9; // 0x9
    int unkc; // 0xc
};
