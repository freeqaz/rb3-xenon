#include "rndobj/DOFProc.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Cam.h"

DOFProc *TheDOFProc;

DOFProc::DOFProc() {}
DOFProc::~DOFProc() {}

void DOFProc::Init() {
    REGISTER_OBJ_FACTORY(DOFProc);
    if (!TheDOFProc) {
        TheDOFProc = Hmx::Object::New<DOFProc>();
        static DataNode &n = DataVariable("the_dof_proc");
        n = TheDOFProc;
    }
}

void DOFProc::Terminate() {
    RELEASE(TheDOFProc);
#ifdef HX_NATIVE
    // DC3-era addition; RB3 retail's DOFProc::Terminate (0x82466080) is 80 B
    // and does nothing but RELEASE(TheDOFProc) -- no DataVariable, no static
    // guard, and no EH funclet.  Kept for native behaviour only.
    static DataNode &n = DataVariable("the_dof_proc");
    n = NULL_OBJ;
#endif
}

BEGIN_HANDLERS(DOFProc)
    HANDLE_ACTION(unset, UnSet())
    HANDLE_ACTION(
        set,
        Set(_msg->Obj<RndCam>(2),
            _msg->Float(3),
            _msg->Float(4),
            _msg->Float(5),
            _msg->Float(6))
    )
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
