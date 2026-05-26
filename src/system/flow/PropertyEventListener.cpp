#include "flow/PropertyEventListener.h"
#include "flow/FlowNode.h"
#include "flow/DrivenPropertyEntry.h"
#include "obj/Object.h"

PropertyEventListener::PropertyEventListener(Hmx::Object *owner)
    : mAutoPropEntries(owner), mEventsRegistered(0) {}

void PropertyEventListener::RegisterEvents(FlowNode *node) {
    static Symbol reactivate("reactivate");
    if (!mEventsRegistered) {
        if (!mAutoPropEntries.empty()) {
            for (ObjVector<AutoPropEntry>::iterator it = mAutoPropEntries.begin();
                 it != mAutoPropEntries.end();
                 ++it) {
                if (it->mProvider) {
                    it->mProvider->AddPropertySink(node, it->mPropertyArray, reactivate);
                }
            }
        }
    }
    mEventsRegistered = true;
}

void PropertyEventListener::UnregisterEvents(FlowNode *node) {
    if (mEventsRegistered) {
        if (!mAutoPropEntries.empty()) {
            for (ObjVector<AutoPropEntry>::iterator it = mAutoPropEntries.begin();
                 it != mAutoPropEntries.end();
                 ++it) {
                if (it->mProvider) {
                    it->mProvider->RemovePropertySink(node, it->mPropertyArray);
                }
            }
        }
    }
    mEventsRegistered = false;
}

void PropertyEventListener::GenerateAutoNames(FlowNode *node, bool clear) {
    if (clear) {
        mAutoPropEntries.clear();
    }

    const auto &entries = node->DrivenPropEntries();
    FOREACH (entry_it, entries) {
        const auto &ops = entry_it->MathOps();
        FOREACH (op_it, ops) {
            AutoPropEntry entry(node);
            mAutoPropEntries.push_back(entry);
        }
    }
}
