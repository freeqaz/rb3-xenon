#include "utl/DataPointMgr.h"
#include "obj/Data.h"
#include "os/System.h"
#include "os/NetworkSocket.h"
#include "utl/JsonEncode.h"
#include "utl/UrlEncode.h"
#include <utility>

DataPointMgr gDataPointMgr;
DataPointMgr &TheDataPointMgr = gDataPointMgr;

bool AddValToString(const DataNode &node, String &str, unsigned int type) {
    if (node.Type() == kDataString || node.Type() == kDataSymbol) {
        if (type == 2) {
            URLEncode(node.Str(), str, false);
            return true;
        } else if (type == 1) {
            JSONStrEncode(node.Str(), str);
            return true;
        } else {
            str += node.Str();
            return true;
        }
    } else if (node.Type() == kDataFloat) {
        str += MakeString("%f", node.Float());
        return true;
    } else if (node.Type() == kDataInt) {
        str += MakeString("%d", node.Int());
        return true;
    } else
        return false;
}

#pragma region DataPoint

DataPoint::DataPoint() {}

DataPoint::DataPoint(const char *type) {
    // someone forgot their initializer lists at home
    mType = type;
}

void DataPoint::ToJSON(String &str) const {
    std::map<Symbol, DataNode>::const_iterator it = mNameValPairs.begin();
    str += "{";
    bool firstEntry = true;
    for (; it != mNameValPairs.end(); ++it) {
        if (!firstEntry) {
            str += ",";
        }
        str += "\"";
        str += it->first;
        str += "\"";
        str += ":";
        if (it->second.Type() == kDataString || it->second.Type() == kDataSymbol) {
            str += "\"";
        }
        if (!AddValToString(it->second, str, 1)) {
            MILO_FAIL("Unsupported type %d in DataPoint::ToJSON.\n", it->second.Type());
        }
        if (it->second.Type() == kDataString || it->second.Type() == kDataSymbol) {
            str += "\"";
        }
        firstEntry = false;
    }
    str += "}";
}

void DataPoint::AddPair(const char *name, DataNode value) {
    if (strlen(name) != 0) {
        auto it = mNameValPairs.insert(std::make_pair(name, value));
        if (!it.second) {
            MILO_FAIL("Duplicate name [%s] in DP %s.", name, mType);
        }
    }
}

void DataPoint::AddPair(Symbol name, DataNode value) { AddPair(name.Str(), value); }

#pragma endregion
#pragma region DataPointMgr

DataPointMgr::DataPointMgr()
    : mDataPointRecorder(nullptr), mDebugDataPointRecorder(nullptr),
      mCrucibleHostname(nullptr), mCrucibleApp(nullptr), mCrucibleProject(nullptr),
      mCrucibleVersion(0) {}

DataPointMgr::~DataPointMgr() {}

void DataPointMgr::Init() {
    if (SystemConfig()) {
        DataArray *cfg = SystemConfig()->FindArray("crucible", false);
        if (cfg) {
            mCrucibleHostname = cfg->FindStr("hostname");
            mCrucibleApp = cfg->FindStr("app");
            mCrucibleProject = cfg->FindStr("project");
        }
        mCrucibleConfig = SystemConfig()->File();
        SystemConfig()->FindData("version", mCrucibleVersion, false);
    } else {
        mCrucibleConfig = "<unknown>";
    }
    if (!mCrucibleHostname) {
        static Symbol hostnames("hostnames");
        if (SystemConfig()) {
            mCrucibleHostname = SystemConfig(hostnames, "crucible")->Str(1);
        }
    }
    mCrucibleUser = NetworkSocket::GetHostName();
}

DataPointRecordFunc *DataPointMgr::SetDataPointRecorder(DataPointRecordFunc *recorder) {
    DataPointRecordFunc *old = mDataPointRecorder;
    mDataPointRecorder = recorder;
    return old;
}

void DataPointMgr::RecordDataPoint(DataPoint &point) {
    if (mDataPointRecorder) {
        mDataPointRecorder(point);
    }
}

void DataPointMgr::RecordDebugDataPoint(DataPoint &point) {
    if (mDebugDataPointRecorder) {
        mDebugDataPointRecorder(point);
    }
}

#pragma endregion
