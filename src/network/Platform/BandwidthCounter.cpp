#include "network/Platform/BandwidthCounter.h"
#include "Platform/CriticalSection.h"
#include "Platform/ScopedCS.h"
#include "Platform/String.h"

namespace Quazal {

    qChain<BandwidthCounter *> BandwidthCounter::s_lstBWCounters;
    CriticalSection BandwidthCounter::s_cs(0x40000000);

    BandwidthCounter::BandwidthCounter(const String &str)
        : unk0(0), unk4(0), mName(str), mMin(-1), mMax(0), mTotal(0), mOccurences(0) {
        volatile ScopedCS scs(s_cs);
        s_lstBWCounters.push_back(this);
    }

    void BandwidthCounter::operator+=(unsigned int ui) {
        mTotal += ui;
        mOccurences++;
        if (ui < mMin) {
            mMin = ui;
        }
        if (ui > mMax) {
            mMax = ui;
        }
    }

    BandwidthCounterMap::BandwidthCounterMap(const String &str) : mName(str) {}

    Quazal::BandwidthCounterMap::~BandwidthCounterMap() {}

    BandwidthCounterMap::IOBandwidthCounter::IOBandwidthCounter(const String &str)
        : mIncoming(str + "/Incoming"), mOutgoing(str + "/Outgoing") {}

    BandwidthCounterMap::IOBandwidthCounter *
    BandwidthCounterMap::operator[](unsigned int key) {
        qMap<unsigned int, IOBandwidthCounter *>::iterator it = mMap.find(key);
        if (it != mMap.end())
            return it->second;
        // NOTE (lane CM-1-C): retail puts `str` at frame 0x54 and `iobc` at 0x58;
        // we emit the pair inverted (7 diff_arg sites). Reordering the two
        // declarations is a NO-OP here -- verified by recompiling with `iobc`
        // declared first, which produced a byte-identical diff -- so MSVC's /Od
        // slot assignment in this TU is not declaration-order driven. An inner
        // scope for `str` is also ruled out: retail runs the String dtor AFTER
        // the `mMap[key] = iobc` insert, matching the lifetime we already have.
        String str;
        str.Format("%s %d", (const char *)mName, key);
        IOBandwidthCounter *iobc = new (__FILE__, 0x8b) IOBandwidthCounter(str);
        mMap[key] = iobc;
        return iobc;
    }

}
