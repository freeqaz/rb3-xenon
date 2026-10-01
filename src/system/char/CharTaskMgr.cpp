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

// Empty in retail: CharInit calls an empty function at this point, and the
// "toggle_char_task_graph" string is absent from the image.
void CharTaskMgr::Init() {
#ifdef HX_NATIVE
    DataRegisterFunc("toggle_char_task_graph", OnToggleCharTaskGraph);
#endif
}
