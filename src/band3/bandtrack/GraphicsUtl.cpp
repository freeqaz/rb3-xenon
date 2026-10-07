#include "GraphicsUtl.h"
#include "rndobj/Group.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"

// rndobj/Utl.cpp defines this; no header declares it.
RndGroup *GroupOwner(Hmx::Object *);

// Detach an object from everything that holds it for drawing: every group it
// is a member of, and its transform parent. Tail and GemRepTemplate call this
// before reusing or freeing a mesh they copied out of the track's template.
//
// Not yet compiled by the X360 build (no objects.json entry and no retail
// address identified); the native build links it.
void UnhookGroupParents(Hmx::Object *obj) {
    do {
        RndGroup *g = GroupOwner(obj);
        if (g == NULL)
            break;
        g->RemoveObject(obj);
    } while (true);
}

void UnhookAllParents(Hmx::Object *obj) {
    UnhookGroupParents(obj);
    RndTransformable *trans = dynamic_cast<RndTransformable *>(obj);
    if (trans)
        trans->SetTransParent(NULL, false);
}
