#pragma once
#include "os/Debug.h"
#include "utl/CRC.h"
#include "utl/Std.h"
#include <map>

template <class T>
class RefRes {
public:
    unsigned long mRefs; // 0x0
    T *mRes; // 0x4
    RefRes() : mRefs(0), mRes(0) {}

    T *Data() const { return mRes; }
    void AddRef() { mRefs++; }
    void Release() { mRefs--; }
    void SetData(T *data) { mRes = data; }
    unsigned long NumRefs() const { return mRefs; }
};

template <class T>
class ResMgr {
public:
    virtual ~ResMgr() {}
    virtual void OnReleaseResource(void *) = 0;
    virtual void Dump() {
        MILO_LOG("Resource Count : %d \n", mResources.size());
        MILO_LOG("-------------------------------------------\n");
        FOREACH (it, mResources) {
            RefRes<T> &data = it->second;
            if (data.NumRefs()) {
                MILO_LOG("%d: %d\n", it->first.mCRC, data.NumRefs());
            }
        }
        MILO_LOG("\n\n");
    }

    T *Get(Hmx::CRC key) {
        auto it = mResources.find(key);
        if (it != mResources.end()) {
            it->second.AddRef();
            return it->second.Data();
        } else {
            return nullptr;
        }
    }

    void ReserveRes(Hmx::CRC key, T *data) {
        RefRes<T> &res = mResources[key];
        MILO_ASSERT(res.mRes == NULL, 0x50);
        res.SetData(data);
        res.AddRef();
    }

    bool ReleaseRes(Hmx::CRC key) {
        auto it = mResources.find(key);
        if (it != mResources.end()) {
            RefRes<T> &res = it->second;
            res.Release();
            if (res.NumRefs() == 0) {
                OnReleaseResource(res.Data());
                mResources.erase(it);
            }
            return true;
        }
        return false;
    }

protected:
    std::map<Hmx::CRC, RefRes<T> > mResources; // 0x4
};
