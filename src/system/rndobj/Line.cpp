#include "rndobj/Line.h"
#include "obj/Object.h"
#include "math/Rot.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "world/Spotlight.h"

void Spotlight::UpdateSphere() {
    Sphere s;
    MakeWorldSphere(s, true);
    Transform xfm;
    FastInvert(WorldXfm(), xfm);
    Multiply(s, xfm, s);
    SetSphere(s);
}

RndLine *gLine;

#pragma region Hmx::Object

RndLine::RndLine()
    : mWidth(1), mHasCaps(true), mLinePairs(false), mFoldAngle(PI / 2), mMat(this),
      mLineUpdate(true) {
    mMesh = Hmx::Object::New<RndMesh>();
    mMesh->SetMutable(0x1F);
    mMesh->SetTransParent(this, false);
    UpdateInternal();
}

BEGIN_HANDLERS(RndLine)
    HANDLE_EXPR(num_points, NumPoints())
    HANDLE_ACTION(
        set_point_pos,
        SetPointPos(_msg->Int(2), Vector3(_msg->Float(3), _msg->Float(4), _msg->Float(5)))
    )
    HANDLE_EXPR(point_color, mPoints[_msg->Int(2)].color.PackAlpha())
    HANDLE_ACTION(
        set_point_color,
        SetPointColor(
            _msg->Int(2),
            Hmx::Color(_msg->Float(3), _msg->Float(4), _msg->Float(5), _msg->Float(6)),
            true
        )
    )
    HANDLE_ACTION(
        set_points_color,
        SetPointsColor(
            _msg->Int(2),
            _msg->Int(3),
            Hmx::Color(_msg->Float(4), _msg->Float(5), _msg->Float(6), _msg->Float(7))
        )
    )
    HANDLE_ACTION(set_update, SetUpdate(_msg->Int(2)))
    HANDLE(set_mat, OnSetMat)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(RndLine::Point)
    SYNC_PROP(point, o.point)
    SYNC_PROP_MODIFY(color, o.color, gLine->UpdatePointColor(_prop->Int(_i - 1), true))
    SYNC_PROP_MODIFY(
        alpha, o.color.alpha, gLine->UpdatePointColor(_prop->Int(_i - 1), true)
    )
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(RndLine)
    gLine = this;
    SYNC_PROP_SET(mat, mMat.Ptr(), SetMat(_val.Obj<RndMat>()))
    SYNC_PROP(width, mWidth)
    SYNC_PROP_SET(fold_angle, mFoldAngle * RAD2DEG, SetFoldAngle(_val.Float() * DEG2RAD))
    SYNC_PROP_MODIFY(has_caps, mHasCaps, SetNumPoints(NumPoints()))
    SYNC_PROP_MODIFY(line_pairs, mLinePairs, SetNumPoints(NumPoints()))
    SYNC_PROP_SET(num_points, NumPoints(), SetNumPoints(_val.Int()))
    SYNC_PROP_MODIFY(points, mPoints, SetNumPoints(NumPoints()))
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const RndLine::Point &pt) {
    bs << pt.point << pt.color;
    return bs;
}

BEGIN_SAVES(RndLine)
    SAVE_REVS(4, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mMat << mPoints << mWidth << mFoldAngle << mHasCaps;
    bs << mLinePairs;
END_SAVES

BEGIN_COPYS(RndLine)
    CREATE_COPY_AS(RndLine, d);
    MILO_ASSERT(d, 0x2D2);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndTransformable)
    COPY_MEMBER_FROM(d, mMat)
    COPY_MEMBER_FROM(d, mPoints)
    COPY_MEMBER_FROM(d, mWidth)
    COPY_MEMBER_FROM(d, mFoldAngle)
    COPY_MEMBER_FROM(d, mHasCaps)
    COPY_MEMBER_FROM(d, mLinePairs)
    UpdateInternal();
END_COPYS

BinStreamRev &operator>>(BinStreamRev &d, RndLine::Point &pt) {
    d >> pt.point >> pt.color;
    return d;
}

INIT_REVS(4, 0)

BEGIN_LOADS(RndLine)
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    if (d.rev > 3) {
        Hmx::Object::Load(bs);
    }
    RndDrawable::Load(bs);
    if (d.rev < 3) {
        ObjPtrList<Hmx::Object> objList(this);
        int x;
        bs >> x >> objList;
    }
    RndTransformable::Load(bs);
    bs >> mMat;
    d >> mPoints;
    bs >> mWidth;
    if (d.rev > 0) {
        bs >> mFoldAngle;
        d >> mHasCaps;
    }
    if (d.rev > 1) {
        d >> mLinePairs;
    }
    UpdateInternal();
END_LOADS

inline TextStream &operator<<(TextStream &ts, const RndLine::Point &pt) {
    ts << "\n\tv:" << pt.point << "\n\tc:" << pt.color;
    return ts;
}

void RndLine::Print() {
    TheDebug << "   points: " << mPoints << "\n";
    TheDebug << "   width: " << mWidth << "\n";
    TheDebug << "   foldAngle: " << mFoldAngle << "\n";
    TheDebug << "   hasCaps: " << mHasCaps << "\n";
    TheDebug << "   linePairs:" << mLinePairs << "\n";
}

#pragma endregion
#pragma region RndDrawable

void RndLine::UpdateSphere() {
    Sphere s;
    MakeWorldSphere(s, true);
    Transform xfm;
    FastInvert(WorldXfm(), xfm);
    Multiply(s, xfm, s);
    SetSphere(s);
}

float RndLine::GetDistanceToPlane(const Plane &p, Vector3 &v3) {
    if (mPoints.empty())
        return 0;
    WorldXfm();
    float ret = 0.0f;
    bool first = true;
    FOREACH (it, mPoints) {
        float t1 = p.a * it->point.x;
        float t2 = p.b * it->point.y;
        float t3 = p.c * it->point.z;
        float dot = t1 + t2 + t3 + p.d;
        if (first || std::fabs(dot) < std::fabs(ret)) {
            first = false;
            ret = dot;
            v3 = it->point;
        }
    }
    return ret;
}

bool RndLine::MakeWorldSphere(Sphere &s, bool b2) {
    if (b2) {
        s.Zero();
        FOREACH (it, mPoints) {
            s.GrowToContain(Sphere(it->point, mWidth));
        }
        return true;
    } else {
        if (mSphere.GetRadius()) {
            Multiply(mSphere, WorldXfm(), s);
            return true;
        } else
            return false;
    }
}

void RndLine::Mats(std::list<class RndMat *> &mats, bool) {
    if (mMat) {
        mats.push_back(mMat);
    }
}

void RndLine::DrawShowing() {
    if (mPoints.size() >= 2) {
        if (mLineUpdate) {
            RndCam *cam = RndCam::Current();
            UpdateLine(cam->WorldXfm(), cam->NearPlane());
            mMesh->SetWorldXfm(cam->WorldXfm());
        }
        mMesh->DrawShowing();
    }
}

RndDrawable *RndLine::CollideShowing(const Segment &s, float &f, Plane &p) {
    RndDrawable *d = mMesh->Collide(s, f, p);
    return d ? this : d;
}

int RndLine::CollidePlane(const Plane &p) { return mMesh->CollidePlane(p); }

#pragma endregion
#pragma region RndLine

void RndLine::SetMat(RndMat *mat) {
    mMat = mat;
    mMesh->SetMat(mat);
}

void RndLine::SetUpdate(bool b1) {
    mLineUpdate = b1;
    if (!mLineUpdate) {
        Transform xfm(WorldXfm());
        static Vector3 offset(0, -1, 0);
        Multiply(offset, xfm, xfm.v);
        UpdateLine(xfm, 0);
        mMesh->SetLocalPos(offset);
    }
}

void RndLine::SetPointPos(int i, const Vector3 &pos) {
    MILO_ASSERT((i >= 0) && (i < mPoints.size()), 0x1CE);
    mPoints[i].point = pos;
}

void RndLine::SetPointColor(int i, const Hmx::Color &color, bool sync) {
    MILO_ASSERT((i >= 0) && (i < mPoints.size()), 0x1D5);
    mPoints[i].color = color;
    UpdatePointColor(i, sync);
}

void RndLine::UpdatePointColor(int i, bool sync) {
    Point *pt = &mPoints[i];
    VertsMap vmap;
    MapVerts(i, vmap);
    vmap.v++->color = pt->color;
    vmap.v++->color = pt->color;
    if (vmap.t) {
        vmap.v++->color = pt->color;
        vmap.v++->color = pt->color;
    }
    if (sync)
        mMesh->Sync(0x1F);
}

void RndLine::UpdateInternal() {
    mFoldCos = cos(mFoldAngle);
    mMesh->SetMat(mMat);
    SetNumPoints(mPoints.size());
}

void RndLine::SetNumPoints(int num) {
    mPoints.resize(num);
    if ((int)num >= 1) {
        int i1 = num;
        if (mHasCaps) {
            i1 = num + 2;
            if (mLinePairs) {
                i1 = (num & 0x7ffffffeU) * 2;
            }
        }
        mMesh->Verts().resize(i1 * 2);
        int numPoints = mPoints.size();
        for (int i = 0; (unsigned int)i < numPoints; i++) {
            VertsMap vmap;
            MapVerts(i, vmap);
            if (vmap.t == 1) {
                vmap.v->tex.Set(0, 1);
                vmap.v++->color = mPoints[i].color;
                vmap.v->tex.Set(0, 0);
                vmap.v++->color = mPoints[i].color;
            }
            vmap.v->tex.Set(1, 1);
            vmap.v++->color = mPoints[i].color;
            vmap.v->tex.Set(0, 1);
            vmap.v++->color = mPoints[i].color;
            if (vmap.t == 2) {
                vmap.v->tex.Set(1, 1);
                vmap.v++->color = mPoints[i].color;
                vmap.v->tex.Set(1, 0);
                vmap.v++->color = mPoints[i].color;
            }
        }

        if (mLinePairs) {
            if (mHasCaps)
                i1 = i1 * 3 >> 1;
        } else
            i1 = (i1 - 1) * 2;
        mMesh->Faces().resize(i1);
        for (int i5 = i1 - 2; i5 >= 0; i5 -= 2) {
            int i7 = i5;
            if (mLinePairs) {
                if (mHasCaps) {
                    i7 = i5 % 6 + (i5 / 6) * 8;
                } else
                    i7 = i5 * 2;
            }
            mMesh->Faces(i5).Set(i7, i7 + 2, i7 + 1);
            mMesh->Faces(i1 - 1).Set(i7 + 1, i7 + 2, i7 + 3);
            i1 = i5;
        }
        mMesh->Sync(0x13F);
    }
}

DataNode RndLine::OnSetMat(const DataArray *array) {
    RndMat *mat = array->Obj<RndMat>(2);
    SetMat(mat);
    SetShowing(mat);
    return 0;
}

void RndLine::MapVerts(int idx, VertsMap &vmap) {
    if (mHasCaps) {
        if (mLinePairs) {
            vmap.t = (idx & 1) + 1;
            vmap.v = &mMesh->Verts()[idx * 4];
        } else {
            if (0 == idx) {
                vmap.t = 1;
                vmap.v = &mMesh->Verts()[0];
            } else {
                int lastIdx = (int)mPoints.size() - 1;
                if ((unsigned int)idx == lastIdx) {
                    vmap.t = 2;
                    vmap.v = &mMesh->Verts()[(int)mMesh->Verts().size() - 4];
                } else {
                    vmap.t = 0;
                    vmap.v = &mMesh->Verts()[(idx + 1) * 2];
                }
            }
        }
    } else {
        vmap.t = 0;
        vmap.v = &mMesh->Verts()[idx * 2];
    }
}

void RndLine::SetPointsColor(int start, int end, const Hmx::Color &color) {
    MILO_ASSERT((start >= 0) && (start < mPoints.size()) && (end >= 0) && (end < mPoints.size()), 0x1F2);
    if (end < start) {
        int tmp = start;
        start = end;
        end = tmp;
    }
    for (int i = start; i <= end; i++) {
        mPoints[i].color = color;
        VertsMap vmap;
        MapVerts(i, vmap);
        vmap.v++->color = color;
        vmap.v++->color = color;
        if (vmap.t != 0) {
            vmap.v++->color = color;
            vmap.v++->color = color;
        }
    }
    mMesh->Sync(0x1F);
}

void RndLine::UpdateLinePair(RndLine::Point *pt1, RndLine::Point *pt2) {
    VertsMap vmap;
    MapVerts((pt1 - &mPoints[0]), vmap);

    if (pt1 == pt2) {
        if (mHasCaps) {
            *(Vector3 *)&vmap.v->pos = *(Vector3 *)&pt1->unk[0];
            vmap.v++;
            *(Vector3 *)&vmap.v->pos = *(Vector3 *)&pt1->unk[0];
            vmap.v++;
        }
        *(Vector3 *)&vmap.v->pos = *(Vector3 *)&pt1->unk[0];
        vmap.v++;
        *(Vector3 *)&vmap.v->pos = *(Vector3 *)&pt1->unk[0];
        vmap.v++;
        *(Vector3 *)&vmap.v->pos = *(Vector3 *)&pt2->unk[0];
        vmap.v++;
        *(Vector3 *)&vmap.v->pos = *(Vector3 *)&pt2->unk[0];
        vmap.v++;
        if (mHasCaps) {
            *(Vector3 *)&vmap.v->pos = *(Vector3 *)&pt2->unk[0];
            vmap.v++;
            *(Vector3 *)&vmap.v->pos = *(Vector3 *)&pt2->unk[0];
        }
    } else {
        float *viewPos1 = (float *)&pt1->unk[0];
        float *viewPos2 = (float *)&pt2->unk[0];
        float *proj1 = (float *)&pt1->unk[4];
        float *dir1 = (float *)&pt1->unk[6];
        float *side1 = (float *)&pt1->unk[8];
        float *proj2 = (float *)&pt2->unk[4];
        float *side2 = (float *)&pt2->unk[8];

        float invY1 = 1.0f / viewPos1[1];
        proj1[1] = viewPos1[2] * invY1;
        proj1[0] = viewPos1[0] * invY1;
        float invY2 = 1.0f / viewPos2[1];
        proj2[0] = viewPos2[0] * invY2;
        proj2[1] = viewPos2[2] * invY2;

        float dirZ = proj2[1] - proj1[1];
        dir1[1] = dirZ;
        float dirX = proj2[0] - proj1[0];
        dir1[0] = dirX;
        float len = std::sqrt(dirX * dirX + dirZ * dirZ);
        float invLen = 0.0f;
        if (len != 0.0f) {
            invLen = 1.0f / len;
        }
        dir1[1] = invLen * dir1[1];
        dir1[0] = invLen * dirX;

        side1[1] = dir1[0];
        side1[0] = -dir1[1];
        float width = mWidth;
        side1[1] = side1[1] * width;
        side1[0] = side1[0] * width;
        ((int *)side2)[0] = ((int *)side1)[0];
        ((int *)side2)[1] = ((int *)side1)[1];

        float sideZ = side1[1];
        float sideX = side1[0];

        if (mHasCaps) {
            vmap.v->pos.x = viewPos1[0] - sideX;
            vmap.v->pos.y = viewPos1[1];
            vmap.v->pos.z = viewPos1[2] - sideZ;
            vmap.v->pos.z = vmap.v->pos.z + sideX;
            vmap.v->pos.x = vmap.v->pos.x + -sideZ;
            vmap.v++;
            vmap.v->pos.x = viewPos1[0] + sideX;
            vmap.v->pos.y = viewPos1[1];
            vmap.v->pos.z = viewPos1[2] + sideZ;
            vmap.v->pos.z = vmap.v->pos.z + sideX;
            vmap.v->pos.x = vmap.v->pos.x + -sideZ;
            vmap.v++;
        }

        vmap.v->pos.Set(viewPos1[0] - sideX, viewPos1[1], viewPos1[2] - sideZ);
        vmap.v++;
        vmap.v->pos.Set(viewPos1[0] + sideX, viewPos1[1], viewPos1[2] + sideZ);
        vmap.v++;
        vmap.v->pos.Set(viewPos2[0] - side2[0], viewPos2[1], viewPos2[2] - side2[1]);
        vmap.v++;
        vmap.v->pos.Set(viewPos2[0] + side2[0], viewPos2[1], viewPos2[2] + side2[1]);
        vmap.v++;

        if (mHasCaps) {
            vmap.v->pos.x = viewPos2[0] - side2[0];
            vmap.v->pos.y = viewPos2[1];
            vmap.v->pos.z = viewPos2[2] - side2[1];
            vmap.v->pos.x = vmap.v->pos.x + side2[1];
            vmap.v->pos.z = vmap.v->pos.z + -side2[0];
            vmap.v++;
            vmap.v->pos.x = viewPos2[0] + side2[0];
            vmap.v->pos.y = viewPos2[1];
            vmap.v->pos.z = viewPos2[2] + side2[1];
            vmap.v->pos.x = vmap.v->pos.x + side2[1];
            vmap.v->pos.z = vmap.v->pos.z + -side2[0];
        }
    }
}

template <class _T>
__declspec(noinline) auto _outline_back(_T* _obj) -> decltype(_obj->back()) {
    return _obj->back();
}

void RndLine::UpdateLine(RndLine::Point *start, RndLine::Point *end) {
    // Phase 1: Project all points (divide x,z by y in view space)
    for (Point *pt = start; pt <= end; pt++) {
        float *viewPos = (float *)&pt->unk[0];
        float *proj = (float *)&pt->unk[4];
        float invY = 1.0f / viewPos[1];
        proj[0] = viewPos[0] * invY;
        proj[1] = viewPos[2] * invY;
    }

    // Phase 2: Compute direction and side vectors between adjacent points
    Point *lastPt = start;
    if (start != end) {
        for (Point *pt = start; pt != end; pt++) {
            float *proj = (float *)&pt->unk[4];
            float *dir = (float *)&pt->unk[6];
            Point *next = pt + 1;
            float *nextProj = (float *)&next->unk[4];

            float dirZ = nextProj[1] - proj[1];
            dir[1] = dirZ;
            float dirX = nextProj[0] - proj[0];
            dir[0] = dirX;

            float len = std::sqrt(dirX * dirX + dirZ * dirZ);
            float invLen = 0.0f;
            if (len != 0.0f) {
                invLen = 1.0f / len;
            }
            float normDirZ = dirZ * invLen;
            float normDirX = dirX * invLen;
            dir[1] = normDirZ;
            dir[0] = normDirX;

            lastPt = pt + 1;

            // Side vector: perpendicular to direction, scaled by width
            float *side = (float *)&pt->unk[8];
            side[1] = normDirX;
            side[0] = -normDirZ;
            float width = mWidth;
            side[1] = normDirX * width;
            side[0] = side[0] * width;
        }
    }

    // Copy direction/side from second-to-last point to last point (integer copy)
    int *lastWords = &lastPt->unk[6];
    int *prevWords = &(lastPt - 1)->unk[6];
    lastWords[1] = prevWords[1];
    lastWords[2] = prevWords[2];
    lastWords[3] = prevWords[3];
    lastWords[0] = prevWords[0];

    // Phase 3: Handle fold angles at interior points
    Point *secondPt = start + 1;
    bool flipped = false;

    // Initialize prevRay: base = projected pos + side, dir = direction
    float *startProj = (float *)&start->unk[4];
    float *startDir = (float *)&start->unk[6];
    float *startSide = (float *)&start->unk[8];

    Hmx::Ray prevRay;
    prevRay.base.Set(startSide[0] + startProj[0], startSide[1] + startProj[1]);
    prevRay.dir.Set(startDir[0], startDir[1]);

    if (secondPt != end) {
        for (Point *pt = secondPt; pt != end; pt++) {
            float *dir = (float *)&pt->unk[6];
            float *side = (float *)&pt->unk[8];
            float *proj = (float *)&pt->unk[4];
            Point *prevP = pt - 1;
            float *prevDir2 = (float *)&prevP->unk[6];

            // Dot product of adjacent direction vectors
            float dot = prevDir2[0] * dir[0] + prevDir2[1] * dir[1];

            // Check if fold angle exceeded
            if (dot < mFoldCos) {
                flipped = !flipped;
            }

            // If flipped, negate the side vector
            if (flipped) {
                side[1] = -side[1];
                side[0] = -side[0];
            }

            // Save old prevRay, then update prevRay with current data
            Hmx::Ray oldPrevRay = prevRay;
            prevRay.base.Set(proj[0] + side[0], proj[1] + side[1]);
            *(long long *)&prevRay.dir = *(long long *)dir;

            // If angle is sharp enough, intersect adjacent rays for smooth corner
            if (dot < 0.9998499751091003f) {
                Intersect(prevRay, oldPrevRay, *(Vector2 *)side);
                side[1] = side[1] - proj[1];
                side[0] = side[0] - proj[0];
            }
        }
    }

    // If still flipped at the end, negate the last point's side vector
    if (flipped) {
        float *endSide = (float *)&end->unk[8];
        endSide[0] = -endSide[0];
        endSide[1] = -endSide[1];
    }

    // Phase 4: Copy side vectors for points outside the visible range
    Point *pointsBegin = &mPoints[0];
    Point *pointsEnd = &_outline_back(&mPoints);

    if (pointsBegin == start) {
        // Start is at the beginning; copy end's data to points after end
        if (end + 1 <= pointsEnd) {
            int *endSide = &end->unk[8];
            int *endView = &end->unk[0];
            for (Point *pt = end + 1; pt <= pointsEnd; pt++) {
                int *ptData = &pt->unk[0];
                ptData[8] = endSide[0];
                ptData[9] = endSide[1];
                ptData[0] = endView[0];
                ptData[1] = endView[1];
                ptData[2] = endView[2];
                ptData[3] = endView[3];
            }
        }
    } else if (pointsBegin < start) {
        // Copy start's side/view data to points before start
        int *startSide2 = &start->unk[8];
        int *startView = &start->unk[0];
        for (Point *pt = pointsBegin; pt < start; pt++) {
            int *ptData = &pt->unk[0];
            ptData[8] = startSide2[0];
            ptData[9] = startSide2[1];
            ptData[0] = startView[0];
            ptData[1] = startView[1];
            ptData[2] = startView[2];
            ptData[3] = startView[3];
        }
    }

    // Phase 5: Write vertex positions
    VertsMap vmap;
    MapVerts(0, vmap);

    // Start cap
    if (mHasCaps) {
        float *viewPos = (float *)&pointsBegin->unk[0];
        float *sideV = (float *)&pointsBegin->unk[8];
        float capSideX = -sideV[0];
        float capSideZ = -sideV[1];

        float x1 = viewPos[0] - sideV[0];
        float y1 = viewPos[1];
        float z1 = viewPos[2] - sideV[1];
        vmap.v->pos.Set(x1, y1, z1);
        vmap.v++;
        vmap.v->pos.Set(x1 + capSideZ, y1, z1 + capSideX);
        vmap.v++;

        float x2 = viewPos[0] + sideV[0];
        float z2 = viewPos[2] + sideV[1];
        vmap.v->pos.Set(x2, y1, z2);
        vmap.v++;
        vmap.v->pos.Set(x2 + capSideZ, y1, z2 + capSideX);
        vmap.v++;
    }

    // Main line vertices
    for (Point *pt = pointsBegin; pt <= pointsEnd; pt++) {
        float *viewPos = (float *)&pt->unk[0];
        float *sideV = (float *)&pt->unk[8];
        vmap.v->pos.Set(viewPos[0] - sideV[0], viewPos[1], viewPos[2] - sideV[1]);
        vmap.v++;
        vmap.v->pos.Set(viewPos[0] + sideV[0], viewPos[1], viewPos[2] + sideV[1]);
        vmap.v++;
    }

    // End cap
    if (mHasCaps) {
        float *viewPos = (float *)&pointsEnd->unk[0];
        float *sideV = (float *)&pointsEnd->unk[8];
        float capSideZ, capSideX;
        if (flipped) {
            capSideZ = -sideV[1];
            capSideX = sideV[0];
        } else {
            capSideZ = -sideV[0];
            capSideX = sideV[1];
        }

        float x1 = viewPos[0] - sideV[0];
        float y1 = viewPos[1];
        float z1 = viewPos[2] - sideV[1];
        vmap.v->pos.Set(x1, y1, z1);
        vmap.v++;
        vmap.v->pos.Set(x1 + capSideZ, y1, z1 + capSideX);
        vmap.v++;

        float x2 = viewPos[0] + sideV[0];
        float z2 = viewPos[2] + sideV[1];
        vmap.v->pos.Set(x2, y1, z2);
        vmap.v++;
        vmap.v->pos.Set(x2 + capSideZ, y1, z2 + capSideX);
    }
}

void RndLine::UpdateLine(const Transform &camXfm, float nearPlane) {
    int numPts = (int)mPoints.size();
    if ((unsigned int)numPts < 2)
        return;

    // Transpose camera transform, then multiply with world transform
    Transform viewXfm;
    Transpose(camXfm, viewXfm);
    Multiply(WorldXfm(), viewXfm, viewXfm);

    // Transform points and track near-plane clipping
    int firstClipped = -1;
    int lastClipped = -1;
    int i = 0;
    float clipDist = nearPlane + 0.01f;

    numPts = (int)mPoints.size();
    for (i = 0; i < numPts; i++) {
        Point *pt = &mPoints[i];
        float *viewPos = (float *)&pt->unk[0];
        Multiply(pt->point, viewXfm, *(Vector3 *)viewPos);
        if (viewPos[1] < clipDist) {
            lastClipped = i;
            if (firstClipped == -1) {
                firstClipped = i;
            }
        }
    }
    if (firstClipped == 0 && lastClipped == numPts - 1)
        return;

    if (!mLinePairs) {
        int startIdx;
        int endIdx;
        if (lastClipped != -1) {
            if (firstClipped > numPts - lastClipped - 1) {
                Point *pt = &mPoints[firstClipped];
                float *curView = (float *)&pt->unk[0];
                float *prevView = (float *)&pt[-1].unk[0];
                Interp(*(Vector3 *)prevView, *(Vector3 *)curView,
                       (clipDist - prevView[1]) / (curView[1] - prevView[1]),
                       *(Vector3 *)curView);
                endIdx = firstClipped;
                startIdx = 0;
            } else {
                Point *pt = &mPoints[lastClipped];
                float *curView = (float *)&pt->unk[0];
                float *nextView = (float *)&pt[1].unk[0];
                Interp(*(Vector3 *)curView, *(Vector3 *)nextView,
                       (clipDist - curView[1]) / (nextView[1] - curView[1]),
                       *(Vector3 *)curView);
                endIdx = numPts - 1;
                startIdx = lastClipped;
            }
        } else {
            endIdx = numPts - 1;
            startIdx = 0;
        }
        UpdateLine(&mPoints[startIdx], &mPoints[endIdx]);
    } else {
        i = 0;
        while (i < numPts - 1) {
            Point *pt1 = &mPoints[i];
            float dist1 = ((float *)&pt1->unk[0])[1];
            Point *pt2 = pt1 + 1;
            if (dist1 < clipDist) {
                float dist2 = ((float *)&pt2->unk[0])[1];
                if (dist2 < clipDist) {
                    pt2 = pt1;
                } else {
                    float d1 = ((float *)&pt1->unk[0])[1];
                    Interp(*(Vector3 *)&pt1->unk[0], *(Vector3 *)&pt1[1].unk[0],
                           (clipDist - d1) / (dist2 - d1),
                           *(Vector3 *)&pt1->unk[0]);
                    pt2 = pt1 + 1;
                }
            } else {
                float dist2 = ((float *)&pt2->unk[0])[1];
                if (dist2 < clipDist) {
                    Interp(*(Vector3 *)&pt2->unk[0], *(Vector3 *)&pt1->unk[0],
                           (clipDist - dist2) / (dist1 - dist2),
                           *(Vector3 *)&pt2->unk[0]);
                    pt2 = pt1 + 1;
                }
            }
            UpdateLinePair(pt1, pt2);
            i += 2;
        }
    }

    mMesh->Sync(0x1F);
}
