#include "flow/DrivenPropertyEntry.h"
#include "flow/FlowNode.h"
#include "obj/Object.h"
#include "utl/BinStream.h"

DrivenPropertyEntry::DrivenPropertyEntry(Hmx::Object *owner) : mMathOps(owner) {
    static Symbol none("none");
    mNode = none;
}

DrivenPropertyEntry::~DrivenPropertyEntry() { mMathOps.clear(); }

void DrivenPropertyEntry::Load(BinStream &bs, FlowNode *node) {
    static const unsigned short gRevs[4] = { 0, 0, 0, 0 };
    ObjectDir *dir = node->Dir();

    int rev;
    bs >> rev;

    int revLow = (int)(u16)rev;
    int revHigh = (unsigned int)rev >> 16;

    if (revLow > 0) {
        MILO_FAIL(
            "%s can't load new %s version %d > %d",
            PathName(dir),
            "DrivenPropertyEntry",
            revLow,
            gRevs[0]
        );
    }
    if (revHigh > 0) {
        MILO_FAIL(
            "%s can't load new %s alt version %d > %d",
            PathName(dir),
            "DrivenPropertyEntry",
            revHigh,
            gRevs[2]
        );
    }

    bs >> mNode;
    int numOps;
    bs >> numOps;

    mMathOps.clear();
    for (int i = 0; i < numOps; i++) {
        FlowMathOp op(node);
        op.Load(bs, dir);
        mMathOps.push_back(op);
    }
}

void DrivenPropertyEntry::Save(BinStream &bs) {
    bs << 0;
    bs << mNode;
    bs << mMathOps.size();
    for (ObjVector<FlowMathOp>::iterator it = mMathOps.begin(); it != mMathOps.end();
         ++it) {
        it->Save(bs);
    }
}
