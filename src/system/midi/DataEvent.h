#pragma once
#include "obj/Data.h"
#include "utl/VectorSizeDefs.h"
#include <vector>
#ifdef HX_NATIVE
#include <cstdint>
#endif

class DataEvent {
public:
    DataEvent() : start(0), end(0), mMsg(0) {}
    DataEvent(float s, float e, DataArray *da) : start(s), end(e), mMsg(da) {
        if (mMsg)
            mMsg->AddRef();
    }
    DataEvent(const DataEvent &e) : mMsg(0) { *this = e; }
    ~DataEvent() {
        if (mMsg)
            mMsg->Release();
    }

    void SetMsg(DataArray *da) {
        if (mMsg)
            mMsg->Release();
        mMsg = da;
        if (mMsg)
            mMsg->AddRef();
    }
    DataArray *Msg() const { return mMsg; }
    DataEvent &operator=(const DataEvent &e) {
        start = e.start;
        end = e.end;
        SetMsg(e.mMsg);
        return *this;
    }

    float start; // 0x0
    float end; // 0x4
    DataArray *mMsg; // 0x8
};

class DataEventList {
public:
    struct CompEv {
        float start;
        float end;
#ifdef HX_NATIVE
        // LP64: a compressed Symbol event stores the Symbol's `const char *mStr`,
        // which is 8 bytes on 64-bit. The original 32-bit `int value` truncated
        // that pointer (hi32 lost) -> dangling Symbol::mStr -> SIGSEGV in
        // Symbol::operator==(char const*) down the MsgSource::Export dispatch.
        // Widen to pointer-width so both kDataSymbol and kDataInt comp-values
        // round-trip losslessly.
        intptr_t value;
#else
        int value;
#endif
    };

    DataEventList();
    ~DataEventList();
    void InsertEvent(float start, float end, const DataNode &ev, int at);
    void Reset(float frame);
    void Clear();
    void Compress(DataArray *temp, int element);
    void SecOffset(float);
    int FindStartFromBack(float) const;
    const DataEvent &Event(int) const;
    const DataEvent *NextEvent(float);
    float *EndPtr(int index);
    void Invert(float);
    void Compact();

    int CurIndex() const { return mCurIndex; }
    int Size() const { return mSize; }

    int mCurIndex; // 0x0
    int mSize; // 0x4
    std::vector<DataEvent> mEvents; // 0x8
    std::vector<CompEv VECTOR_SIZE_LARGE> mComps; // 0x10
    int mElement; // 0x1c
    mutable DataEvent mTemplate; // 0x20, 0x24, 0x28
    DataType mCompType; // 0x2c
#ifdef HX_NATIVE
    // Points at the full 8-byte union of the template node's mValue (offset 0),
    // so a compressed Symbol/int value writes/reads all pointer-width bits.
    intptr_t *mValue; // 0x30
#else
    int *mValue; // 0x30
#endif
};
