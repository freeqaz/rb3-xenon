#include "game/FretHand.h"
#include "beatmatch/GameGem.h"
#include "os/Debug.h"

FretHand::~FretHand() {}

void FretHand::SetFinger(uint finger, int fret, int lowstr, int highstr) {
    MILO_ASSERT(finger < kNumFingers, 24);
    FretFinger &thefinger = mFinger[finger];
    thefinger.mFret = fret;
    thefinger.mLowString = lowstr;
    thefinger.mHighString = highstr;
}

void FretHand::GetFinger(uint finger, int &fret, int &lowstr, int &highstr) const {
    MILO_ASSERT(finger < kNumFingers, 33);
    fret = mFinger[finger].mFret;
    lowstr = mFinger[finger].mLowString;
    highstr = mFinger[finger].mHighString;
}

int FretHand::GetFret(int str) const {
    int ret = 0;
    for (int i = 0; i < kNumFingers; i++) {
        const FretFinger &f = mFinger[i];
        if (f.mLowString == str)
            return f.mFret;
        if (f.mLowString < str && str <= f.mHighString)
            ret = f.mFret;
    }
    return ret;
}

bool FretHand::BarAll(const GameGem &gem) {
    int first = -1;
    int last = -1;
    for (unsigned int i = 0; i < 6; i++) {
        if (gem.GetFret(i) > 0) {
            if (first == -1)
                first = i;
            last = i;
        }
    }
    if (first == -1)
        return false;
    int bareFret = -1;
    for (; first <= last; first++) {
        if (bareFret >= 0) {
            if (gem.GetFret(first) < 0)
                return false;
        } else if (gem.GetFret(first) > 0) {
            bareFret = gem.GetFret(first);
            continue;
        }
        if (gem.GetFret(first) >= 0 && bareFret != gem.GetFret(first))
            return false;
    }
    int lowStr = -1;
    int highStr = -1;
    for (unsigned int i = 0; i < 6; i++) {
        if (gem.GetFret(i) > 0) {
            if (lowStr == -1)
                lowStr = i;
            highStr = i;
        }
    }
    SetFinger(0, bareFret, lowStr, highStr);
    return true;
}

void FretHand::Reset() {
    for (int i = 0; i < kNumFingers; i++)
        SetFinger(i, -1, -1, -1);
}

void FretHand::SetFingers(const GameGem &gem) {
    Reset();
    if (!BarAll(gem)) {
        int handpos = gem.GetHandPosition();
        if (handpos < 1)
            handpos = 1;
        unsigned int finger = 0;
        int numFingers = gem.GetNumFingers();
        int maxPos = handpos + 5;
        bool advanced = false;
        for (int pos = handpos; pos < maxPos && finger < 4 && numFingers != 0; pos++) {
            int lastStr = -1;
            bool placed = false;
            for (unsigned int str = 0; str < 6 && numFingers != 0; str++) {
                if (gem.GetFret(str) == pos) {
                    if ((unsigned)numFingers <= 4 - finger && lastStr == -1) {
                        SetFinger(finger, pos, str, -1);
                        placed = true;
                        advanced = true;
                        finger++;
                        numFingers--;
                    } else if (lastStr == -1) {
                        SetFinger(finger, pos, str, -1);
                        lastStr = str;
                        placed = true;
                        advanced = true;
                        numFingers--;
                    } else {
                        bool canMerge = true;
                        int mid = lastStr + 1;
                        while (mid < (int)str) {
                            if (gem.GetFret(mid) != -1 && gem.GetFret(mid) < pos)
                                canMerge = false;
                            mid++;
                        }
                        if (canMerge) {
                            SetFinger(finger, pos, lastStr, str);
                        } else {
                            finger++;
                            SetFinger(finger, pos, str, -1);
                            lastStr = str;
                        }
                        placed = true;
                        advanced = true;
                        numFingers--;
                    }
                }
            }
            if (lastStr != -1) {
                finger++;
            } else if (!placed && advanced && (unsigned)numFingers < 4 - finger) {
                finger++;
            }
        }
        if (numFingers != 0) {
            MILO_WARN("Unable to build fret hand chord.");
        }
    }
}
