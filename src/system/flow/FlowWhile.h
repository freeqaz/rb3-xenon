#pragma once
#include "flow/FlowSwitch.h"
#include "flow/PropertyEventListener.h"

/** "A while node; behaves as if constantly evaluting it's property" */
class FlowWhile : public FlowSwitch, public PropertyEventListener {
public:
    // Hmx::Object
    virtual ~FlowWhile();
    OBJ_CLASSNAME(FlowWhile)
    OBJ_SET_TYPE(FlowWhile)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    // FlowNode
    virtual bool Activate();
    virtual void Deactivate(bool);
    virtual void RequestStop();
    virtual void RequestStopCancel();
    virtual bool IsRunning();
    virtual void MiloPreRun();

    OBJ_MEM_OVERLOAD(0x1F)
    NEW_OBJ(FlowWhile)

    void UnregisterSelf() { UnregisterEvents(this); }

protected:
    FlowWhile();

    virtual void ChildFinished(FlowNode *);
    // PropertyEventListener
    virtual void GenerateAutoNames(FlowNode *, bool);

    void ReActivate();

    unsigned char mEntryCount; // 0x8c (compiler-verified, /d1reportSingleClassLayoutFlowWhile)

    // Retail RB3 X360 carries 72 (0x48) more bytes between the
    // PropertyEventListener sub-object and the `Hmx::Object` VIRTUAL BASE than
    // the DC3-derived declaration above accounts for.
    //
    // Evidence (`?SyncProperty@FlowWhile@@UAA_N...`, the only real-bodied fn in
    // this unit): MSVC passes `this` for an Object-vbase override as the
    // *vbase* pointer, so the `SYNC_SUPERCLASS(FlowSwitch)` call materialises
    //     this - (FlowWhile::vbase_off - FlowSwitch::vbase_off).
    // Compiler layout gives FlowWhile vbase 0x94 and FlowSwitch vbase 0x78
    // => 0x1c, which is exactly what we emit. Retail emits 0x64 (100), i.e.
    // FlowWhile's vbase must sit 72 bytes further out at 0xDC.
    // Reserved as dead bytes; the real members are not reconstructed.
    char mRetailVBaseReserve[0x48];
};
