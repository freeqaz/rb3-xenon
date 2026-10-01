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
    // Retail Init (0x82740FF0) registers 0x82740C48, which MemAllocs and runs
    // ??0DxParticleSys -- without this the factory resolves to the inherited
    // RndParticleSys::NewObject and builds the wrong class.
    NEW_OBJ(DxParticleSys)
    void DrawParticles(const Hmx::Color &);

protected:
    DxParticleSys();

    static D3DVertexDeclaration *sVertexDecl;
};
