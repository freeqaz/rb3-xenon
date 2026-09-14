#include "net_band/DataResults.h"
#include "net/JsonUtils.h"
#include "os/Debug.h"
#include "utl/Std.h"

// Ported from rb3-Wii (dev) DataResults.cpp. Retail RB3-360 TU spans
// [0x8250af88, 0x8250bf98): Clear, GetDataResultValue, GetDataResult, ctor,
// dtor, scalar-deleting dtor, Update, plus the map/list template COMDATs and
// EH funclets this TU is the first to instantiate. Retail carries no Print
// body (unreferenced COMDAT dropped by /OPT:REF), the source still has one.

bool DataResult::GetDataResultValue(String str, DataNode &node) const {
    bool found = false;
    std::map<String, DataNode>::const_iterator it = mDataMap.find(str);
    if (it != mDataMap.end()) {
        found = true;
        node = it->second;
    }
    return found;
}

DataResultList::DataResultList() {
    mQDataResultString = new Quazal::String();
    mUpdated = false;
}

DataResultList::~DataResultList() { delete mQDataResultString; }

void DataResultList::Update(Message *msg) {
    mDataResultList.clear();
    String str(mQDataResultString->m_szContent);
    if (!str.empty() && str[0] == '[') {
        JsonConverter jc;
        JsonObject *jsonObj = jc.LoadFromString(str);
        MILO_ASSERT(jsonObj->GetType() == JsonObject::kType_Array, 0x3B);
        JsonArray *jsonArr = (JsonArray *)jsonObj;
        for (uint i = 0; i < jsonArr->GetSize(); i++) {
            JsonObject *tmpJsonObject = jc.GetValue(jsonArr, i);
            MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_Array, 0x45);
            JsonArray *tmpJsonArray = (JsonArray *)tmpJsonObject;
            MILO_ASSERT(tmpJsonArray->GetSize() == 4, 0x49);

            tmpJsonObject = jc.GetValue(tmpJsonArray, 0);
            MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_String, 0x4D);
            String jsonAsStr = tmpJsonObject->Str();

            tmpJsonObject = jc.GetValue(tmpJsonArray, 1);
            MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_String, 0x52);
            String jsonStr1 = tmpJsonObject->Str();
            uint nNumFields = jsonStr1.length();

            tmpJsonObject = jc.GetValue(tmpJsonArray, 2);
            MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_Array, 0x58);
            JsonArray *jsonFieldNames = (JsonArray *)tmpJsonObject;
            MILO_ASSERT(jsonFieldNames->GetSize() == nNumFields, 0x5A);

            tmpJsonObject = jc.GetValue(tmpJsonArray, 3);
            MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_Array, 0x5E);
            JsonArray *jsonDataArray = (JsonArray *)tmpJsonObject;
            int size = jsonDataArray->GetSize();

            for (uint j = 0; j < size; j++) {
                tmpJsonObject = jc.GetValue(jsonDataArray, j);
                MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_Array, 0x65);
                JsonArray *currentRow = (JsonArray *)tmpJsonObject;
                DataResult res;
                res.mUrl = jsonAsStr;
                for (uint k = 0; k < nNumFields; k++) {
                    DataNode ne0;
                    tmpJsonObject = jc.GetValue(currentRow, k);
                    switch (jsonStr1[k]) {
                    case 'd':
                        MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_Int, 0x74);
                        ne0 = tmpJsonObject->Int();
                        break;
                    case 'f':
                        MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_Double, 0x79);
                        ne0 = (float)tmpJsonObject->Double();
                        break;
                    case 's':
                        MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_String, 0x7E);
                        ne0 = tmpJsonObject->Str();
                        break;
                    default:
                        MILO_FAIL("Unsupported type!");
                        break;
                    }
                    tmpJsonObject = jc.GetValue(jsonFieldNames, k);
                    MILO_ASSERT(tmpJsonObject->GetType() == JsonObject::kType_String, 0x88);
                    String pairStr = tmpJsonObject->Str();
                    res.mDataMap.insert(std::make_pair(pairStr, ne0));
                }
                mDataResultList.push_back(res);
            }
        }
    }
    mUpdated = true;
}

void DataResultList::Clear() {
    *mQDataResultString = 0;
    mUpdated = false;
}

DataResult *DataResultList::GetDataResult(int ix) const {
    MILO_ASSERT(mDataResultList.size() > 0, 0xA3);
    MILO_ASSERT_RANGE(ix, 0, mDataResultList.size(), 0xA4);
    std::list<DataResult>::const_iterator it = mDataResultList.begin();
    std::advance(it, ix);
    return (DataResult *)&*it;
}

void DataResultList::Print(TextStream &ts) { ts.Print(mQDataResultString->m_szContent); }
