#include "rndobj/DOFProc.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Cam.h"

DOFProc *TheDOFProc;

DOFProc::DOFProc() {}
DOFProc::~DOFProc() {}

void DOFProc::Init() {
#ifdef HX_NATIVE
    // No factory registration here (lane A55): retail registers DOFProc in
    // Rnd::PreInit and NgRnd::PreInit then registers NgDOFProc under the same
    // class name, so this New<DOFProc>() makes retail's NgDOFProc (the depth
    // of field CameraShot sets per shot). Re-registering DOFProc here made the
    // native singleton the plain DOFProc, whose Enabled() is always false, so
    // the engine's depth of field (DofPass) never ran.
    if (!TheDOFProc) {
        TheDOFProc = Hmx::Object::New<DOFProc>();
        static DataNode &n = DataVariable("the_dof_proc");
        n = TheDOFProc;
    }
#else
    // RB3 retail's DOFProc::Init (0x82466298) is 60 B: it creates the
    // singleton and nothing else -- no factory registration, no DataVariable.
    if (!TheDOFProc)
        TheDOFProc = Hmx::Object::New<DOFProc>();
#endif
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

#ifdef HX_NATIVE
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
#endif
