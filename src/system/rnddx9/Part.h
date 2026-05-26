#pragma once
#include "rnddx9/Object.h"
#include "rndobj/Part.h"
#include "xdk/D3D9.h"

class DxParticleSys : public RndParticleSys, public DxObject {
public:
    virtual ~DxParticleSys() {}
    OBJ_CLASSNAME(ParticleSys);
    OBJ_SET_TYPE(ParticleSys);
    virtual void DrawShowing();
    virtual void SetPool(int x, Type t) { RndParticleSys::SetPool(x, t); }

    static void Init();

protected:
    DxParticleSys();

    static D3DVertexDeclaration *sVertexDecl;
};
