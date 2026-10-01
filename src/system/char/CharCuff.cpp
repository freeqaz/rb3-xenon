// Retail inlines the ObjPtr two-arg ctor at this TU's member-init sites; the
// in-class (plain inline) definition lets MSVC choose per site, as retail did.
#define RB3_OBJPTR_INLINE_TWOARG_CTOR
#include "char/CharCuff.h"
#include "char/FileMerger.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "rndobj/Rnd.h"
#include "math/Trig.h"
#include <cmath>

CharCuff::CharCuff() : mOpenEnd(0), mIgnore(this), mBone(this), mEccentricity(1) {
    mShape[0].offset = -2.9;
    mShape[0].radius = 1.9;
    mShape[1].offset = 0;
    mShape[1].radius = 2.6;
    mShape[2].offset = 2.0;
    mShape[2].radius = 3.5;
    mOuterRadius = 3.1;
}


BEGIN_HANDLERS(CharCuff)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharCuff)
    SYNC_PROP(offset0, mShape[0].offset)
    SYNC_PROP(radius0, mShape[0].radius)
    SYNC_PROP(offset1, mShape[1].offset)
    SYNC_PROP(radius1, mShape[1].radius)
    SYNC_PROP(offset2, mShape[2].offset)
    SYNC_PROP(radius2, mShape[2].radius)
    SYNC_PROP(outer_radius, mOuterRadius)
    SYNC_PROP(open_end, mOpenEnd)
    SYNC_PROP(bone, mBone)
    SYNC_PROP(eccentricity, mEccentricity)
    SYNC_PROP(category, mCategory)
    SYNC_PROP(ignore, mIgnore)
    SYNC_SUPERCLASS(RndTransformable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharCuff)
    SAVE_REVS(8, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    for (int i = 0; i < 3; i++) {
        bs << mShape[i].radius;
        bs << mShape[i].offset;
    }
    bs << mOuterRadius;
    bs << mOpenEnd;
    bs << mBone;
    bs << mEccentricity;
    bs << mCategory;
    bs << mIgnore;
END_SAVES

BEGIN_COPYS(CharCuff)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(CharCuff)
    BEGIN_COPYING_MEMBERS
        memcpy(mShape, c->mShape, sizeof(mShape));
        COPY_MEMBER(mOuterRadius)
        COPY_MEMBER(mOpenEnd)
        COPY_MEMBER(mBone)
        COPY_MEMBER(mEccentricity)
        COPY_MEMBER(mCategory)
        COPY_MEMBER(mIgnore)
    END_COPYING_MEMBERS
END_COPYS

// Retail Load keeps no BinStreamRev: it splits the packed rev into one aligned
// file-scope aggregate (altRev +0, rev +4) and reads everything from the raw
// stream.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharCuff;

BEGIN_LOADS(CharCuff)
    int rev;
    bs >> rev;
    gRevs_CharCuff.rev = getHmxRev(rev);
    gRevs_CharCuff.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    RndTransformable::Load(bs);
    for (int i = 0; i < 3; i++) {
        bs >> mShape[i].radius >> mShape[i].offset;
    }
    if (gRevs_CharCuff.rev > 1)
        bs >> mOuterRadius;
    else
        mOuterRadius = mShape[1].radius + 0.5f;
    if (gRevs_CharCuff.rev > 2)
        bs >> mOpenEnd;
    else
        mOpenEnd = false;
    if (gRevs_CharCuff.rev > 3)
        bs >> mBone;
    else
        mBone = TransParent();
    if (gRevs_CharCuff.rev > 4)
        bs >> mEccentricity;
    else
        mEccentricity = 1.0f;
    if (gRevs_CharCuff.rev > 5)
        bs >> mCategory;
    else {
        Symbol empty("");
        mCategory = empty;
    }
    if (gRevs_CharCuff.rev > 7)
        bs >> mIgnore;
    if (gRevs_CharCuff.rev < 7)
        MILO_NOTIFY("%s old CharCuff, must convert, see James", PathName(this));
END_LOADS

float CharCuff::Eccentricity(const Vector2 &v) const {
    float f1 = v.y * v.y;
    float f2 = v.x * v.x;
    return std::sqrt((f1 + f2) / (f1 * (1.0f / (mEccentricity * mEccentricity)) + f2));
}

// Retail 0x8239E290: out of line and recursive; Deform inlines its first level.
#ifdef HX_NATIVE
void AddBoneChildren(
#else
__forceinline void AddBoneChildren(
#endif
    std::list<RndTransformable *> &tlist, RndTransformable *trans) {
    if (strncmp(trans->Name(), "bone_", 5) == 0) {
        tlist.push_back(trans);
        for (std::list<RndTransformable *>::const_iterator it =
                 trans->TransChildren().begin();
             it != trans->TransChildren().end();
             ++it) {
            AddBoneChildren(tlist, *it);
        }
    }
}

static int BoneMask(std::list<RndTransformable *> &tlist, RndMesh *mesh) {
    int mask = 0;
    for (int i = 0; i < mesh->NumBones(); i++) {
        if (std::find(tlist.begin(), tlist.end(), mesh->BoneTransAt(i)) != tlist.end()) {
            mask |= 1 << i;
        }
    }
    return mask;
}

void CharCuff::Deform(SyncMeshCB *cb, FileMerger *fm) {
    if (!mBone)
        return;
    std::list<RndMesh *> meshes;
    for (ObjDirItr<CharCuff> it(Dir(), false); it != nullptr; ++it) {
        if (it != this && it->mBone == mBone) {
            if (it->mOuterRadius > mOuterRadius)
                return;
            if (it->mOuterRadius == mOuterRadius && strcmp(it->Name(), Name()) > 0)
                return;
            for (ObjPtrList<RndMesh>::iterator m = it->mIgnore.begin();
                 m != it->mIgnore.end();
                 ++m) {
                meshes.push_back(*m);
            }
        }
    }
    FileMerger::Merger *merger = nullptr;
    if (fm) {
        for (int i = 0; i < fm->Mergers().size(); i++) {
            FileMerger::Merger &cur = fm->Mergers()[i];
            if (strstr(cur.mName.Str(), mCategory.Str()) && cur.mLoadedObjects.size() != 0
                && cur.mLoadedObjects.front()->Dir() == Dir()) {
                merger = &cur;
                break;
            }
        }
    }
    if (!merger)
        return;
    std::list<RndTransformable *> transes;
    AddBoneChildren(transes, mBone);
    for (ObjPtrList<Hmx::Object>::iterator it = merger->mLoadedObjects.begin();
         it != merger->mLoadedObjects.end();
         ++it) {
        RndMesh *mesh = dynamic_cast<RndMesh *>(*it);
        if (mesh && std::find(meshes.begin(), meshes.end(), mesh) == meshes.end()) {
            int mask = BoneMask(transes, mesh);
            if (mask != 0) {
                meshes.push_back(mesh);
                DeformMesh(mesh, mask, cb);
            }
        }
    }
}

void CharCuff::DeformMesh(RndMesh *mesh, int boneMask, SyncMeshCB *cb) {
    float eccInvSq = 1.0f / (mEccentricity * mEccentricity);
    bool synced = false;
    RndMesh::VertVector &verts = mesh->Verts();
    Transform xfm;
    if (TransParent() && TransParent()->Name() != Dir()->Name()) {
        if (!mesh->NumBones())
            return;
        Transform boneInv;
        FastInvert(mesh->BoneTransAt(0)->WorldXfm(), boneInv);
        Multiply(WorldXfm(), boneInv, boneInv);
        FastInvert(mesh->BoneOffsetAt(0), xfm);
        Multiply(boneInv, xfm, xfm);
    } else {
        xfm = mLocalXfm;
    }
    float axisX = xfm.m.z.x;
    float axisY = xfm.m.z.y;
    float axisZ = xfm.m.z.z;
    float planeD = -(xfm.v.x * axisX + (xfm.v.y * axisY + xfm.v.z * axisZ));
    for (int i = 0; i < verts.size(); i++) {
        RndMesh::Vert &vert = verts[i];
        if (!(((1 << vert.boneIndices[3]) | (1 << vert.boneIndices[2])
               | (1 << vert.boneIndices[1]) | (1 << vert.boneIndices[0]))
              & boneMask))
            continue;
        float along =
            (axisX * vert.pos.x + (axisY * vert.pos.y + axisZ * vert.pos.z)) + planeD;
        if (along >= mShape[2].offset)
            continue;
        float projY = xfm.m.z.y * along + xfm.v.y;
        float projZ = xfm.m.z.z * along + xfm.v.z;
        float projX = xfm.m.z.x * along + xfm.v.x;
        float dy = vert.pos.y - projY;
        float dz = vert.pos.z - projZ;
        float dx = vert.pos.x - projX;
        float u = dx * xfm.m.x.x + (dz * xfm.m.x.z + xfm.m.x.y * dy);
        float v = dx * xfm.m.y.x + (dz * xfm.m.y.z + xfm.m.y.y * dy);
        float lenSq = dx * dx + (dz * dz + dy * dy);
        u *= u;
        v *= v;
        float distSq = ((v * eccInvSq + u) / (v + u)) * lenSq;
        if (along < mShape[0].offset) {
            if (mOpenEnd)
                continue;
            if (!synced) {
                cb->SyncMesh(mesh, 0xBF);
                synced = true;
            }
            float t = mShape[0].offset;
            vert.pos.x = xfm.m.z.x * t + xfm.v.x;
            vert.pos.y = xfm.m.z.y * t + xfm.v.y;
            vert.pos.z = xfm.m.z.z * t + xfm.v.z;
            float scale = mShape[0].radius / sqrtf(distSq);
            vert.pos.x += scale * dx;
            vert.pos.y += dy * scale;
            vert.pos.z += dz * scale;
        } else {
            float r;
            if (along < mShape[1].offset) {
                r = ((along - mShape[1].offset) / (mShape[0].offset - mShape[1].offset))
                        * (mShape[0].radius - mShape[1].radius)
                    + mShape[1].radius;
            } else {
                r = ((along - mShape[2].offset) / (mShape[1].offset - mShape[2].offset))
                        * (mShape[1].radius - mShape[2].radius)
                    + mShape[2].radius;
            }
            if (r * r >= distSq)
                continue;
            if (!synced) {
                synced = true;
                cb->SyncMesh(mesh, 0xBF);
            }
            float scale = r / sqrtf(distSq);
            vert.pos.x = scale * dx + projX;
            vert.pos.y = dy * scale + projY;
            vert.pos.z = dz * scale + projZ;
        }
    }
    if (mOpenEnd)
        return;
    std::vector<RndMesh::Face> &faces = mesh->Faces();
    int last = faces.size() - 1;
    for (int f = 0; f <= last; f++) {
        RndMesh::Face &face = faces[f];
        int j = 0;
        for (; j < 3; j++) {
            RndMesh::Vert &vert = verts[face[j]];
            if (!(((1 << vert.boneIndices[3]) | (1 << vert.boneIndices[2])
                   | (1 << vert.boneIndices[1]) | (1 << vert.boneIndices[0]))
                  & boneMask))
                break;
            float along =
                (vert.pos.x * axisX + (vert.pos.z * axisZ + vert.pos.y * axisY)) + planeD;
            if (along > mShape[0].offset + 0.01f)
                break;
        }
        if (j == 3) {
            if (!synced) {
                cb->SyncMesh(mesh, 0xBF);
                synced = true;
            }
            face = faces[last];
            last--;
            f--;
        }
    }
    faces.resize(last + 1);
}

void CharCuff::Highlight() {
    Hmx::Color white(1, 1, 1, 1);
    const float kTwoPi = 6.2831855f;
    const float kInv32 = 1.0f / 32.0f;
    for (int shapeIdx = 0; shapeIdx < 2; shapeIdx++) {
        for (int pointIdx = 0; pointIdx < 32; pointIdx++) {
            float angle = kTwoPi * pointIdx * kInv32;
            Vector3 innerPt(Sine(angle), Cosine(angle), mShape[shapeIdx].offset);
            Vector3 outerPt(Sine(angle), Cosine(angle), mShape[shapeIdx + 1].offset);
            (Vector2 &)innerPt *= mShape[shapeIdx].radius * Eccentricity((Vector2 &)innerPt);
            (Vector2 &)outerPt *= mShape[shapeIdx + 1].radius * Eccentricity((Vector2 &)outerPt);
            Vector3 worldInner;
            Multiply(innerPt, WorldXfm(), worldInner);
            Vector3 worldOuter;
            Multiply(outerPt, WorldXfm(), worldOuter);
            TheRnd.DrawLine(worldInner, worldOuter, white, false);
            if (shapeIdx < 2) {
                float nextAngle = (kInv32 * (kTwoPi * (pointIdx + 1)));
                innerPt.Set(Sine(nextAngle), Cosine(nextAngle), mShape[shapeIdx].offset);
                (Vector2 &)innerPt *= mShape[shapeIdx].radius * Eccentricity((Vector2 &)innerPt);
                Multiply(innerPt, WorldXfm(), worldOuter);
                TheRnd.DrawLine(worldInner, worldOuter, white, false);
            }
            if (shapeIdx == 1) {
                Vector3 boundaryPt(Sine(angle), Cosine(angle), mShape[shapeIdx].offset);
                (Vector2 &)boundaryPt *= mOuterRadius;
                Multiply(boundaryPt, WorldXfm(), worldInner);
                float nextAngle = (kInv32 * (kTwoPi * (pointIdx + 1)));
                outerPt.Set(Sine(nextAngle), Cosine(nextAngle), mShape[shapeIdx].offset);
                (Vector2 &)outerPt *= mOuterRadius;
                Multiply(outerPt, WorldXfm(), worldOuter);
                TheRnd.DrawLine(worldInner, worldOuter, white, false);
            }
        }
    }
}
