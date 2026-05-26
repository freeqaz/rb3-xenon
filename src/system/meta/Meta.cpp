#include "meta/Meta.h"
#include "HeldButtonPanel.h"
#include "meta/Achievements.h"
#include "meta/ConnectionStatusPanel.h"
#include "meta/DeJitterPanel.h"
#include "meta/PreloadPanel.h"
#include "obj/Object.h"
#include "os/Debug.h"

void MetaInit() {
    Achievements::Init();
    REGISTER_OBJ_FACTORY(ConnectionStatusPanel)
    REGISTER_OBJ_FACTORY(DeJitterPanel)
    REGISTER_OBJ_FACTORY(HeldButtonPanel)
    REGISTER_OBJ_FACTORY(PreloadPanel)
    TheDebug.AddExitCallback(MetaTerminate);
}

void MetaTerminate() { Achievements::Terminate(); }
