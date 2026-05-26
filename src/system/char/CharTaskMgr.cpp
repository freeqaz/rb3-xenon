#include "char/CharTaskMgr.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"

bool CharTaskMgr::sShowGraph = false;
int CharTaskMgr::sNumInstances;
float CharTaskMgr::sGraphPosY;

// does nothing, doesn't get called anywhere
// this func only exists to spawn the MakeString symbol in this TU
void CharTaskMgrDummyTransFunc() { MakeString("%d%f%f%f", 0, 1.0f, 1.0f, 1.0f); }

namespace {
    static DataNode OnToggleCharTaskGraph(DataArray *arr) {
        CharTaskMgr::sShowGraph = !CharTaskMgr::sShowGraph;
        return DataNode(CharTaskMgr::sShowGraph);
    }
}

void CharTaskMgr::Init() {
    DataRegisterFunc("toggle_char_task_graph", OnToggleCharTaskGraph);
}
