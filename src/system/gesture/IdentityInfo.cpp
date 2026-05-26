#include "gesture/IdentityInfo.h"
#include "gesture/GestureMgr.h"
#include "meta_ham/SkeletonIdentifier.h"

SkeletonIdentifiedMsg::SkeletonIdentifiedMsg(int arg1, int arg2)
    : Message(Type(), arg1, arg2) {}

void IdentityInfo::Identified(unsigned int enrollmentIdx) {
    GestureMgr::sIdentityOpInProgress = false;
    switch ((int)enrollmentIdx) {
    case -5:
    case -4:
    case -1:
        enrollmentIdx = (unsigned int)-2;
        break;
    case -2:
        enrollmentIdx = (unsigned int)-1;
        break;
    }
    SkeletonIdentifiedMsg msg(enrollmentIdx, unkc);
    TheGestureMgr->Export(msg, true);
}

void IdentityInfo::PostUpdate() {
    if (mIdentified) {
        Identified(mEnrollmentIdx);
        mIdentified = false;
    }
    if (unk9) {
        unk9 = false;
        static SkeletonEnrollmentChangedMsg msg;
        TheGestureMgr->Export(msg, true);
    }
}
