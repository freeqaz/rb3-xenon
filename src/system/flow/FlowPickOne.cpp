#include "flow/FlowPickOne.h"
#include "flow/FlowNode.h"
#include "flow/DrivenPropertyEntry.h"
#include "math/Rand.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include <algorithm>
#include <vector>

FlowPickOne::FlowPickOne()
    : mChoiceHistory(this), mChoiceType(kChoiceRandom), mIndex(0), mChance(1) {}
FlowPickOne::~FlowPickOne() {}

BEGIN_HANDLERS(FlowPickOne)
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

BEGIN_PROPSYNCS(FlowPickOne)
    SYNC_PROP_MODIFY(choice_type, (int &)mChoiceType, OnChoiceTypeChanged())
    SYNC_PROP(index, mIndex)
    SYNC_PROP(chance, mChance)
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_SAVES(FlowPickOne)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mChoiceType;
    bs << mChance;
END_SAVES

BEGIN_COPYS(FlowPickOne)
    COPY_SUPERCLASS(FlowNode)
    CREATE_COPY(FlowPickOne)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mChoiceType)
        COPY_MEMBER(mChance)
    END_COPYING_MEMBERS
END_COPYS

bool FlowPickOne::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    PushDrivenProperties();

    if (mChance != (int)1.0f) {
        unsigned int r = rand() % 100;
        if ((float)(int)r > mChance * 100.0f) {
            return false;
        }
    }

    if (mChildNodes.empty())
        return false;

    FlowNode *chosen = nullptr;

    switch (mChoiceType) {
    case kChoiceOrdered: {
        int numChildren = (int)mChildNodes.size();
        if (mIndex < 0 || mIndex >= numChildren)
            mIndex = 0;
        ActivateChild(mChildNodes[mIndex]);
        mIndex++;
        return !mRunningNodes.empty();
    }
    case kChoiceRandom: {
        int numChildren = (int)mChildNodes.size();
        mIndex = RandomInt(0, numChildren);
        chosen = mChildNodes[mIndex];
        break;
    }
    case kChoiceRandomNoRepeat: {
        int numChildren = (int)mChildNodes.size();
        if (numChildren > 1) {
            int newIndex;
            do {
                newIndex = RandomInt(0, (int)mChildNodes.size());
            } while (newIndex == mIndex);
            mIndex = newIndex;
        } else {
            mIndex = 0;
        }
        chosen = mChildNodes[mIndex];
        break;
    }
    case kChoiceRandomJukeBox: {
        int numChildren = (int)mChildNodes.size();
        if (numChildren <= 1) {
            if (numChildren == 1)
                chosen = mChildNodes[0];
            break;
        }
        if (mIndex < 0) {
            goto jukebox_shuffle;
        }
        {
            int historySize = (int)mChoiceHistory.size();
            if (mIndex >= historySize) {
                jukebox_shuffle:
                FlowNode *lastChosen = nullptr;
                if (!mChoiceHistory.empty()) {
                    lastChosen = mChoiceHistory[(int)mChoiceHistory.size() - 1];
                }
                mChoiceHistory.clear();
                std::vector<FlowNode *> items;
                FOREACH (it, mChildNodes) {
                    items.push_back(it->Obj());
                }
                RandomShuffle(items.begin(), items.end());
                mIndex = 0;
                FlowNode **p = items.end();
                if (p - items.begin() != 0) {
                    do {
                        mChoiceHistory.push_back(*--p);
                    } while (p - items.begin() != 0);
                }
                if (lastChosen) {
                    if (mChoiceHistory[0] == lastChosen) {
                        mIndex = 1;
                    }
                }
            }
        }
        ActivateChild(mChoiceHistory[mIndex]);
        mIndex++;
        return !mRunningNodes.empty();
    }
    case kChoiceUseIndex: {
        int numChildren = (int)mChildNodes.size();
        int adjustedIndex = mIndex % numChildren;
        mIndex = adjustedIndex;
        ActivateChild(mChildNodes[adjustedIndex]);
        mIndex++;
        return !mRunningNodes.empty();
    }
    default:
        MILO_NOTIFY_ONCE("FlowPickOne: bad picking type");
        return !mRunningNodes.empty();
    }

    ActivateChild(chosen);
    return !mRunningNodes.empty();
}

void FlowPickOne::OnChoiceTypeChanged() {
    if (mChoiceType != kChoiceUseIndex) {
        FOREACH (it, mDrivenPropEntries) {
            DataArray *arr = it->Node().Array();
            if (arr->Sym(0) == "index") {
                mDrivenPropEntries.erase(it);
                return;
            }
        }
    }
}

INIT_REVS(1, 0)

BEGIN_LOADS(FlowPickOne)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    LOAD_SUPERCLASS(FlowNode)
    d >> (int &)mChoiceType;
    if (d.rev > 0) {
        d >> mChance;
    }
END_LOADS
