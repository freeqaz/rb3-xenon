// Label3d (bandobj/Label3d.cpp), MSVC X360. Written from retail bytes,
// 0x822EFBE0-0x822F18A8; see Label3d.h for how the class was recovered.
#include "bandobj/Label3d.h"
#include "math/Rand.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/Locale.h"
#include "utl/MakeString.h"

// Retail stores the two rev halves through one base register (+0 alt, +4 rev),
// which takes an internal-linkage adjacent pair.
static unsigned short gAltRev = 0;
static unsigned short gRev = 0;

// Constructor (0x822F0B90): mAllCaps is left unset.
Label3d::Label3d()
    : mText((const char *)0), mResource((const char *)0), mMaxWidth(0), mLocalize(true),
      mDir(0), mJitterDepth(0), mJitterHeight(0), mMeshes(this, kObjListNoNull) {
    mTrans = Hmx::Object::New<RndTransformable>();
    mTrans->SetTransParent(this, false);
}

// Destructor (0x822F10E8): retail deletes the parent transform, then deletes
// every character mesh before the members unwind.
Label3d::~Label3d() {
    RELEASE(mTrans);
    mMeshes.DeleteAll();
}

BEGIN_HANDLERS(Label3d)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_CHECK(0)
END_HANDLERS

BEGIN_PROPSYNCS(Label3d)
    SYNC_PROP_MODIFY_ALT(text, mText, SyncText())
    SYNC_PROP_MODIFY_ALT(localize, mLocalize, SyncText())
    SYNC_PROP_MODIFY_ALT(resource, mResource, LoadResource())
    SYNC_PROP_MODIFY_ALT(all_caps, mAllCaps, SyncText())
    SYNC_PROP_MODIFY_ALT(max_width, mMaxWidth, SyncText())
    SYNC_PROP_MODIFY_ALT(jitter_depth, mJitterDepth, SyncText())
    SYNC_PROP_MODIFY_ALT(jitter_height, mJitterHeight, SyncText())
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(RndDrawable)
END_PROPSYNCS

// Save (0x822EFE58): Label3d's own fields come first, then the superclasses.
BEGIN_SAVES(Label3d)
    SAVE_REVS(1, 0)
    bs << mText;
    bs << mResource;
    bs << mMaxWidth;
    bs << mAllCaps;
    bs << mJitterDepth;
    bs << mJitterHeight;
    bs << mLocalize;
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    SAVE_SUPERCLASS(RndDrawable)
END_SAVES

// Copy (0x822EFD98): retail has no null test on the cast.
void Label3d::Copy(const Hmx::Object *o, Hmx::Object::CopyType ty) {
    const Label3d *c = dynamic_cast<const Label3d *>(o);
    Hmx::Object::Copy(o, ty);
    RndTransformable::Copy(o, ty);
    RndDrawable::Copy(o, ty);
    mText = c->mText;
    mResource = c->mResource;
    mMaxWidth = c->mMaxWidth;
    mAllCaps = c->mAllCaps;
    mJitterDepth = c->mJitterDepth;
    mJitterHeight = c->mJitterHeight;
    mLocalize = c->mLocalize;
}

BEGIN_LOADS(Label3d)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void Label3d::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(1, 0);
    bs >> mText;
    bs >> mResource;
    bs >> mMaxWidth;
    bs >> mAllCaps;
    bs >> mJitterDepth;
    bs >> mJitterHeight;
    if (gRev != 0)
        bs >> mLocalize;
    else
        mLocalize = true;
    mDir.LoadFile(mResource, true, true, kLoadFront, false);
}

void Label3d::PostLoad(BinStream &bs) {
    Hmx::Object::Load(bs);
    RndTransformable::Load(bs);
    RndDrawable::Load(bs);
    mDir.PostLoad(nullptr);
    SyncText();
}

void Label3d::LoadResource() {
    mDir.LoadFile(mResource, false, true, kLoadFront, false);
    SyncText();
}

bool Label3d::MakeWorldSphere(Sphere &s, bool) {
    if (mSphere.GetRadius()) {
        Multiply(mSphere, WorldXfm(), s);
        return true;
    }
    return false;
}

void Label3d::MeshXExtent(float &minX, float &maxX, RndMesh *mesh) {
    if (!mesh->Mutable() && !mesh->GetKeepMeshData()) {
        MILO_WARN("%s: mesh data not kept", PathName(mesh));
    }
    minX = 1e30f;
    maxX = -1e30f;
    // The vertex is re-read after each store: minX/maxX are references.
    for (int i = 0; i < mesh->Verts().size(); i++) {
        RndMesh::Vert &v = mesh->Verts()[i];
        if (v.pos.x < minX)
            minX = v.pos.x;
        if (v.pos.x > maxX)
            maxX = v.pos.x;
    }
}

void Label3d::DrawShowing() {
    for (ObjPtrList<RndMesh>::iterator it = mMeshes.begin(); it != mMeshes.end(); ++it) {
        (*it)->DrawShowing();
    }
}

void Label3d::Highlight() {
    for (ObjPtrList<RndMesh>::iterator it = mMeshes.begin(); it != mMeshes.end(); ++it) {
        (*it)->Highlight();
    }
}

// Returns the distance of the mesh nearest the plane, and that mesh's point.
float Label3d::GetDistanceToPlane(const Plane &p, Vector3 &v) {
    if (mMeshes.empty())
        return 0;
    float best = 0;
    bool first = true;
    for (ObjPtrList<RndMesh>::iterator it = mMeshes.begin(); it != mMeshes.end(); ++it) {
        Vector3 vtmp;
        float d = (*it)->GetDistanceToPlane(p, vtmp);
        if (first || std::fabs(d) < std::fabs(best)) {
            best = d;
            v = vtmp;
            first = false;
        }
    }
    return best;
}

// One mesh per character: "%02x.mesh" is the glyph, "%02x_box.mesh" its
// advance box; a missing glyph advances by "20_box.mesh" (the space). The row
// is centred on mTrans, squeezed in x to mMaxWidth, and the bounding sphere
// is set around it.
void Label3d::SyncText() {
    if (!mDir || !*mText.c_str())
        return;
    String str;
    if (mLocalize) {
        str = Localize(Symbol(mText.c_str()), nullptr);
    }
    if (!*str.c_str()) {
        str = mText;
    }
    if (mAllCaps) {
        str.ToUpper();
    }
    mMeshes.DeleteAll();
    Transform xfm;
    xfm.Reset();
    mTrans->SetLocalXfm(xfm);
    float width = 0;
    for (unsigned int i = 0; i < str.length(); i++) {
        // Retail formats the character as an int (MakeString<int>), zero-extended.
        int ch = (unsigned char)str[i];
        Hmx::Object *g = mDir->FindObject(MakeString("%02x.mesh", ch), false);
        RndMesh *glyph = dynamic_cast<RndMesh *>(g);
        Hmx::Object *b = mDir->FindObject(MakeString("%02x_box.mesh", ch), false);
        RndMesh *box = dynamic_cast<RndMesh *>(b);
        if (glyph && box) {
            RndMesh *mesh = Hmx::Object::New<RndMesh>();
            mesh->Copy(glyph, kCopyShallow);
            const std::list<RndTransformable *> &children = glyph->TransChildren();
            for (std::list<RndTransformable *>::const_iterator it = children.begin();
                 it != children.end();
                 ++it) {
                RndMesh *child = Hmx::Object::New<RndMesh>();
                child->Copy(*it, kCopyShallow);
                child->SetTransParent(mesh, false);
                mMeshes.push_back(child);
            }
            mesh->SetTransParent(mTrans, false);
            float minX, maxX;
            MeshXExtent(minX, maxX, box);
            Vector3 pos = mesh->LocalXfm().v;
            // Retail advances by half a character on each side of the glyph,
            // as two separate multiply-adds.
            width += (maxX - minX) * 0.5f;
            pos.x += width;
            width += (maxX - minX) * 0.5f;
            if (mJitterDepth != 0) {
                pos.y += RandomFloat(0, mJitterDepth);
            }
            if (mJitterHeight != 0) {
                pos.z += RandomFloat(0, mJitterHeight);
            }
            mesh->SetLocalPos(pos);
            mMeshes.push_back(mesh);
        } else {
            Hmx::Object *sp = mDir->FindObject("20_box.mesh", false);
            RndMesh *space = dynamic_cast<RndMesh *>(sp);
            if (space) {
                float minX, maxX;
                MeshXExtent(minX, maxX, space);
                width += maxX - minX;
            }
        }
    }
    float half = width * 0.5f;
    for (ObjPtrList<RndMesh>::iterator it = mMeshes.begin(); it != mMeshes.end(); ++it) {
        Vector3 pos = (*it)->LocalXfm().v;
        pos.x -= half;
        (*it)->SetLocalPos(pos);
    }
    if (mMaxWidth != 0 && width > mMaxWidth) {
        Transform t = mTrans->LocalXfm();
        Scale(mTrans->LocalXfm().m, Vector3(mMaxWidth / width, 1, 1), t.m);
        mTrans->SetLocalXfm(t);
    }
    Sphere s;
    s.center = mTrans->LocalXfm().v;
    float r = width * 0.1f;
    r += width;
    s.radius = r * 0.5f;
    SetSphere(s);
}
