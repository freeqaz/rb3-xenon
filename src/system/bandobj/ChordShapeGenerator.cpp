#include "bandobj/ChordShapeGenerator.h"
#include "beatmatch/RGUtl.h"
#include "math/Rot.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "os/Timer.h"
#include "utl/MakeString.h"
#define t t_reserved_symbol_extern
#include "utl/Symbols.h"
#undef t
#include <cmath>
#include <math.h>

using std::abs;

ChordShapeGenerator::RevsT ChordShapeGenerator::gRevs = {0, 0};

static Transform t;

ChordShapeGenerator::ChordShapeGenerator()
    : mFingerSrcMesh(this, 0), mChordSrcMesh(this, 0), mBaseXSection(this, 0),
      mContourXSection(this, 0), mBaseHeight(this, 0), mNumSlots(6), mString0(this, 0),
      mString1(this, 0), mString2(this, 0), mString3(this, 0), mString4(this, 0),
      mString5(this, 0), mSource(0), mBaseXVal(-1.0f), mContourXVal(1.0f), mBaseHeightVal(0.2f) {
    mFretHeights.resize(7);
    for (int i = 0; i < 7; i++)
        mFretHeights[i] = 1.0f;
    mGradeDistances.resize(6);
    for (int i = 0; i < 6; i++)
        mGradeDistances[i] = 0.33f;
    mStringFrets.resize(6);
    for (int i = 0; i < 6; i++)
        mStringFrets[i] = -1;
    unk64.resize(6);
    for (int i = 0; i < 6; i++)
        unk64[i] = 1;
}

BEGIN_COPYS(ChordShapeGenerator)
    CREATE_COPY(ChordShapeGenerator)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mChordSrcMesh)
        COPY_MEMBER(mFingerSrcMesh)
        COPY_MEMBER(mBaseXSection)
        COPY_MEMBER(mContourXSection)
        COPY_MEMBER(mBaseHeight)
        COPY_MEMBER(mNumSlots)
        COPY_MEMBER(mString0)
        COPY_MEMBER(mString1)
        COPY_MEMBER(mString2)
        COPY_MEMBER(mString3)
        COPY_MEMBER(mString4)
        COPY_MEMBER(mString5)
        COPY_MEMBER(mFretHeights)
        COPY_MEMBER(mGradeDistances)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_SAVES(ChordShapeGenerator)
    SAVE_REVS(1, 0)
    bs << mChordSrcMesh;
    bs << mFingerSrcMesh;
    bs << mBaseXSection;
    bs << mContourXSection;
    bs << mBaseHeight;
    bs << mNumSlots;
    bs << mStringFrets;
    bs << mString0;
    bs << mString1;
    bs << mString2;
    bs << mString3;
    bs << mString4;
    bs << mString5;
    bs << mFretHeights;
    bs << mGradeDistances;
END_SAVES

#define kMaxFretHeights 6

#define gAltRev gRevs.altRev
#define gRev gRevs.rev
BEGIN_LOADS(ChordShapeGenerator)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    bs >> mChordSrcMesh;
    if (gRev != 0)
        bs >> mFingerSrcMesh;
    bs >> mBaseXSection;
    bs >> mContourXSection;
    bs >> mBaseHeight;
    bs >> mNumSlots;
    bs >> mStringFrets;
    bs >> mString0;
    bs >> mString1;
    bs >> mString2;
    bs >> mString3;
    bs >> mString4;
    bs >> mString5;
    bs >> mFretHeights;
    bs >> mGradeDistances;
    MILO_ASSERT(mFretHeights.size() <= (kMaxFretHeights + 1), 0x70);
    MILO_ASSERT(mGradeDistances.size() <= kMaxFretHeights, 0x71);
    while (mFretHeights.size() < 7) {
        mFretHeights.push_back(mFretHeights.back());
    }
    while (mGradeDistances.size() < 6)
        mGradeDistances.push_back(0);
END_LOADS
#undef gRev
#undef gAltRev

RndMesh *NewCopyMesh(const RndMesh *mesh) {
    RndMesh *ret = Hmx::Object::New<RndMesh>();
    ret->Copy(mesh, Hmx::Object::kCopyDeep);
    return ret;
}

const Transform &ChordShapeGenerator::SlotXfm(int idx) const {
    switch (idx) {
    case 0:
        return mString0->WorldXfm();
    case 1:
        return mString1->WorldXfm();
    case 2:
        return mString2->WorldXfm();
    case 3:
        return mString3->WorldXfm();
    case 4:
        return mString4->WorldXfm();
    case 5:
        return mString5->WorldXfm();
    default:
        MILO_WARN("string index %d out of range", idx);
        return t;
    }
}

bool ChordShapeGenerator::CheckParams() const {
    bool missing = false;
    if (!mChordSrcMesh) {
        MILO_WARN("%s is missing", "source chord mesh");
        missing = true;
    }
    if (!mFingerSrcMesh) {
        MILO_WARN("%s is missing", "source finger mesh");
        missing = true;
    }
    if (!mBaseXSection) {
        MILO_WARN("%s is missing", "base cross section transform");
        missing = true;
    }
    if (!mContourXSection) {
        MILO_WARN("%s is missing", "contour cross section transform");
        missing = true;
    }
    if (!mBaseHeight) {
        MILO_WARN("%s is missing", "base height transform");
        missing = true;
    }
    if (!mString0) {
        MILO_WARN("%s is missing", "smasher 0");
        missing = true;
    }
    if (!mString1) {
        MILO_WARN("%s is missing", "smasher 1");
        missing = true;
    }
    if (!mString2) {
        MILO_WARN("%s is missing", "smasher 2");
        missing = true;
    }
    if (!mString3) {
        MILO_WARN("%s is missing", "smasher 3");
        missing = true;
    }
    if (!mString4) {
        MILO_WARN("%s is missing", "smasher 4");
        missing = true;
    }
    if (!mString5) {
        MILO_WARN("%s is missing", "smasher 5");
        missing = true;
    }
    return missing;
}

int shapesGenerated;
unsigned int cycles;

void ChordShapeGenerator::DumpChordGenData() {
    if (shapesGenerated > 0) {
        MILO_LOG(
            "Chord Shape Generator: built %d shapes in %.2f mS\n",
            shapesGenerated,
            Timer::CyclesToMs(cycles)
        ); // probably the wrong timer func
    }
    shapesGenerated = 0;
    cycles = 0;
}

int kMaxVerts = 400;
int kMaxFaces = 600;

int vertIt;
unsigned int faceIt;

RndMesh *ChordShapeGenerator::BuildChordMesh(unsigned int ui, int i) {
    RGUnpackChordShapeID(ui, mStringFrets, &unk64);
    shapesGenerated++;
    TIMER_GET_CYCLES(startCycles);
    RndMesh *ret = BuildChordMesh();
    TIMER_GET_CYCLES(endCycles);
    cycles += endCycles - startCycles;
    return ret;
}

RndMesh *ChordShapeGenerator::MakeInvertedMesh(const RndMesh *mesh) {
    RndMesh *ret = NewCopyMesh(mesh);
    for (int i = 0; i < (int)ret->Verts().size(); i++) {
        RndMesh::Vert &curvert = ret->Verts()[i];
        curvert.pos.x = -curvert.pos.x;
        curvert.norm.x = -curvert.norm.x;
    }
    for (int i = 0; i < ret->Faces().size(); i++) {
        RndMesh::Face &curface = ret->Faces()[i];
        int temp = curface.v2;
        curface.v2 = curface.v3;
        curface.v3 = temp;
    }
    return ret;
}

RndMesh *ChordShapeGenerator::BuildChordMesh() {
    RndMesh * &_ref0 = mSource;
    _ref0 = mChordSrcMesh;
    if (CheckParams()) {
        TheDebug.Notify(MakeString(
            "Could not create chord shape because some references are missing"
        ));
        return 0;
    }
    mBaseXVal = mBaseXSection->WorldXfm().v.x;
    mContourXVal = mContourXSection->WorldXfm().v.x;
    mBaseHeightVal = mBaseHeight->WorldXfm().v.z;
    GetCrossSection(mBaseXVal, sec2);
    GetCrossSection(mContourXVal, sec1);
    RndMesh *mesh = NewCopyMesh(_ref0);
    mesh->SetMutable(0x3F);
    mesh->Verts().resize(0);
    mesh->Faces().clear();
    mesh->Verts().resize(kMaxVerts);
    mesh->Faces().resize(kMaxFaces, RndMesh::Face());
    vertIt = 0;
    faceIt = 0;
    std::map<unsigned short, unsigned short> connectingVerts;
    Hmx::Color32 onColor(0xFFFFFFFF);
    Hmx::Color32 offColor(0xFF000000);
    for (int i = 0; i < mNumSlots; i++) {
        const Hmx::Color32& col = unk64[i] ? onColor : offColor;
        const Hmx::Color32& colPrev = (i == 0)
            ? col
            : (unk64[i - 1] ? onColor : offColor);
        int fret = mStringFrets[i];
        if (fret == -1) {
            if (i != 0 && mStringFrets[i - 1] != -1) {
                BuildEndCap(
                    mesh, connectingVerts, mStringFrets[i - 1], SlotXfm(i - 1), right,
                    Hmx::Color32(col)
                );
            }
        } else if (i == 0 || mStringFrets[i - 1] == -1) {
            BuildEndCap(
                mesh, connectingVerts, mStringFrets[i], SlotXfm(i), left,
                Hmx::Color32(col)
            );
        } else if (fret == 0) {
            if (mStringFrets[i - 1] != 0) {
                BuildContourCap(
                    mesh, connectingVerts, mStringFrets[i - 1], SlotXfm(i - 1),
                    SlotXfm(i), right, Hmx::Color32(col), Hmx::Color32(colPrev)
                );
            } else {
                BuildSpan(
                    mesh, connectingVerts, mStringFrets[i - 1], mStringFrets[i],
                    SlotXfm(i - 1), SlotXfm(i), Hmx::Color32(col),
                    Hmx::Color32(colPrev)
                );
            }
        } else if (mStringFrets[i - 1] == 0) {
            BuildContourCap(
                mesh, connectingVerts, mStringFrets[i], SlotXfm(i - 1), SlotXfm(i),
                left, Hmx::Color32(col), Hmx::Color32(colPrev)
            );
        } else {
            BuildSpan(
                mesh, connectingVerts, mStringFrets[i - 1], mStringFrets[i],
                SlotXfm(i - 1), SlotXfm(i), Hmx::Color32(col), Hmx::Color32(colPrev)
            );
        }
    }
    int last = mNumSlots - 1;
    if (mStringFrets[last] != -1) {
        Hmx::Color32 col = unk64[last] ? onColor : offColor;
        BuildEndCap(
            mesh, connectingVerts, mStringFrets[mNumSlots - 1],
            SlotXfm(mNumSlots - 1), right, Hmx::Color32(col)
        );
    }
    MILO_ASSERT(connectingVerts.empty(), 0x168);
    mesh->Verts().resize(vertIt);
    mesh->Faces().resize(faceIt, RndMesh::Face());
    if (LOADMGR_EDITMODE) {
        mesh->Sync(0x3F);
        mesh->SetMutable(0);
    }
    return mesh;
}

void ChordShapeGenerator::GetCrossSection(float xOffset, CrossSec &cs) {
    MILO_ASSERT(mSource, 0x17C);
    cs.mEdges.clear();
    cs.mVerts.clear();
    cs.mXOffset = xOffset;
    RndMesh::VertVector &verts = mSource->Verts();
    std::vector<RndMesh::Face> faces(mSource->Faces());
    float hi = xOffset + 0.1f;
    float lo = xOffset - 0.1f;
    for (unsigned int i = 0; i < faces.size(); i++) {
        RndMesh::Face &f = faces[i];
        bool outOfBand = false;
        int outsideVert = -1;
        for (int j = 0; j < 3; j++) {
            float x = verts[f[j]].pos.x;
            if (x < lo) {
                outOfBand = true;
                break;
            } else if (x > hi) {
                if (outsideVert != -1) {
                    outOfBand = true;
                    break;
                }
                outsideVert = f[j];
            }
        }
        if (!outOfBand && outsideVert != -1) {
            Edge edge;
            for (int j = 0; j < 3; j++) {
                if (f[j] == outsideVert) {
                    edge.mV0 = f[(j + 1) % 3];
                    edge.mV1 = f[(j + 2) % 3];
                }
            }
            cs.mEdges.push_back(edge);
            cs.mVerts.insert(edge.mV0);
            cs.mVerts.insert(edge.mV1);
        }
    }
}

void ChordShapeGenerator::BuildEndCap(
    RndMesh *mesh,
    std::map<unsigned short, unsigned short> &connectingVerts,
    int mFret,
    const Transform &xfm,
    Symbol orient,
    Hmx::Color32 col
) {
    bool contour = mFret > 0;
    if (orient == right) {
        unsigned int expectedVerts =
            contour ? sec1.mVerts.size() : sec2.mVerts.size();
        MILO_ASSERT(connectingVerts.size() == expectedVerts, 0x1C4);
    } else {
        MILO_ASSERT(orient == left, 0x1C8);
        connectingVerts.clear();
        AddVertProfile(
            mesh, xfm, mFretHeights[mFret], contour ? sec1 : sec2, connectingVerts,
            Hmx::Color32(col)
        );
    }
    std::map<unsigned short, unsigned short> capMap;
    RndMesh::VertVector &srcVerts = mSource->Verts();
    RndMesh::VertVector &meshVerts = mesh->Verts();
    for (int i = 0; i < (int)srcVerts.size(); i++) {
        float sx = srcVerts[i].pos.x;
        if (contour ? (sx < mContourXVal + 0.1f) : (sx > mBaseXVal - 0.1f)) {
            capMap[i] = vertIt++;
        }
    }
    if (vertIt > (int)meshVerts.size()) {
        unsigned int newsize = meshVerts.size() * 2;
        MILO_LOG("RG: too few verts for chord shape - increasing to %d", newsize);
        meshVerts.resize(newsize);
    }
    float xOffset = contour ? mContourXVal : mBaseXVal;
    float xScale = contour ? -1.0f : 1.0f;
    float fretHeight = mFretHeights[mFret];
    std::map<unsigned short, unsigned short>::const_iterator vit = capMap.begin();
    std::map<unsigned short, unsigned short>::const_iterator vend = capMap.end();
    for (; vit != vend; ++vit) {
        RndMesh::Vert &curvert = meshVerts[vit->second];
        curvert = srcVerts[vit->first];
        TransformVert(curvert, xOffset, xScale, fretHeight, xfm, Hmx::Color32(col));
    }
    capMap.insert(connectingVerts.begin(), connectingVerts.end());
    std::vector<RndMesh::Face> &srcFaces = mSource->Faces();
    std::vector<RndMesh::Face> &meshFaces = mesh->Faces();
    for (unsigned int i = 0; i < srcFaces.size(); i++) {
        const RndMesh::Face &f = srcFaces[i];
        if (contour) {
            float minX = srcVerts[f.v1].pos.x;
            MinEq(minX, srcVerts[f.v2].pos.x);
            MinEq(minX, srcVerts[f.v3].pos.x);
            if (!(minX < mContourXVal - 0.1f))
                continue;
        } else {
            float maxX = srcVerts[f.v1].pos.x;
            MaxEq(maxX, srcVerts[f.v2].pos.x);
            MaxEq(maxX, srcVerts[f.v3].pos.x);
            if (!(maxX > mBaseXVal + 0.1f))
                continue;
        }
        if (faceIt >= meshFaces.size()) {
            unsigned int newsize = meshFaces.size() * 2;
            MILO_LOG("RG: too few faces for chord shape - increasing to %d", (int)newsize);
            meshFaces.resize(newsize, RndMesh::Face());
        }
        RndMesh::Face &mf = meshFaces[faceIt++];
        MILO_ASSERT(
            capMap.find(f.v1) != capMap.end() && capMap.find(f.v2) != capMap.end()
                && capMap.find(f.v3) != capMap.end(),
            0x223
        );
        if (contour) {
            mf.Set(capMap[f.v1], capMap[f.v3], capMap[f.v2]);
        } else {
            mf.Set(capMap[f.v1], capMap[f.v2], capMap[f.v3]);
        }
    }
    connectingVerts.clear();
}

void ChordShapeGenerator::TransformVert(
    RndMesh::Vert &vert,
    float xOffset,
    float xScale,
    float fretHeight,
    const Transform &tf,
    Hmx::Color32 col
) {
    float px = vert.pos.x;
    float pz = vert.pos.z;
    px -= xOffset;
    vert.pos.x = px * xScale;
    if (pz > mBaseHeightVal) {
        vert.pos.z = fretHeight * (pz - mBaseHeightVal) + mBaseHeightVal;
    }
    vert.color.UnpackAlpha(col.FullColor());
    Multiply(vert.pos, tf, vert.pos);
}

void ChordShapeGenerator::BuildContourCap(
    RndMesh *mesh,
    std::map<unsigned short, unsigned short> &connectingVerts,
    int iii,
    const Transform &tf1,
    const Transform &tf2,
    Symbol sym,
    Hmx::Color32 col1,
    Hmx::Color32 col2
) {
    MILO_ASSERT(connectingVerts.size(), 0x24B);
    const RndMesh::VertVector &srcVerts = mSource->Verts();
    std::map<unsigned short, unsigned short> capMap;
    for (int i = 0; i < srcVerts.size(); i++) {
        float sx = srcVerts[i].pos.x;
        if (sx > mBaseXVal + 0.1f && sx < mContourXVal - 0.1f) {
            capMap[i] = vertIt++;
        }
    }
    bool invert = sym == right;
    RndMesh::VertVector &meshVerts = mesh->Verts();
    if (vertIt > meshVerts.size()) {
        unsigned int newsize = meshVerts.size() * 2;
        MILO_LOG("RG: too few verts for chord shape - increasing to %d", newsize);
        meshVerts.resize(newsize);
    }
    Transform trisectA;
    Transform trisectB;
    Transform midPt;
    InterpolateXfm(tf1, tf2, 0.33f, trisectA);
    InterpolateXfm(tf1, tf2, 0.67f, trisectB);
    InterpolateXfm(tf1, tf2, 0.50f, midPt);
    midPt.v = trisectA.v;
    midPt.v += trisectB.v;
    midPt.v /= 2.0f;
    float capTessA = (mBaseXVal * 2.0f + mContourXVal) / 3.0f;
    float capTessB = (mContourXVal * 2.0f + mBaseXVal) / 3.0f;
    float xMid = (mBaseXVal + mContourXVal) * 0.5f;
    float xScale = (tf2.v.x - tf1.v.x) / (mContourXVal - mBaseXVal);
    float fretHeight = mFretHeights[iii];
    std::map<unsigned short, unsigned short>::const_iterator vit = capMap.begin();
    std::map<unsigned short, unsigned short>::const_iterator vend = capMap.end();
    float zScale = -xScale;
    for (; vit != vend; ++vit) {
        RndMesh::Vert &curvert = meshVerts[vit->second];
        curvert = srcVerts[vit->first];
        if (invert) {
            if (curvert.pos.x < capTessA) {
                TransformVert(curvert, mBaseXVal, zScale, fretHeight, tf2, Hmx::Color32(col2));
            } else if (curvert.pos.x < capTessB) {
                TransformVert(curvert, xMid, zScale, fretHeight, midPt, Hmx::Color32(col2));
            } else {
                TransformVert(curvert, mContourXVal, zScale, fretHeight, tf1, Hmx::Color32(col2));
            }
        } else {
            if (curvert.pos.x < capTessA) {
                TransformVert(curvert, mBaseXVal, xScale, fretHeight, tf1, Hmx::Color32(col1));
            } else if (curvert.pos.x < capTessB) {
                TransformVert(curvert, xMid, xScale, fretHeight, midPt, Hmx::Color32(col1));
            } else {
                TransformVert(curvert, mContourXVal, xScale, fretHeight, tf2, Hmx::Color32(col1));
            }
        }
    }
    std::map<unsigned short, unsigned short> endVerts;
    Hmx::Color32 endColor(invert ? col1 : col2);
    AddVertProfile(
        mesh,
        tf2,
        xMid,
        invert ? sec2 : sec1,
        endVerts,
        Hmx::Color32(endColor)
    );
    capMap.insert(connectingVerts.begin(), connectingVerts.end());
    capMap.insert(endVerts.begin(), endVerts.end());
    const std::vector<RndMesh::Face> &srcFaces = mSource->Faces();
    std::vector<RndMesh::Face> &meshFaces = mesh->Faces();
    for (unsigned int i = 0; i < srcFaces.size(); i++) {
        const RndMesh::Face &f = srcFaces[i];
        float minX = srcVerts[f.v1].pos.x;
        MinEq(minX, srcVerts[f.v2].pos.x);
        MinEq(minX, srcVerts[f.v3].pos.x);
        if (minX < mBaseXVal - 0.1f) continue;
        float maxX = srcVerts[f.v1].pos.x;
        MaxEq(maxX, srcVerts[f.v2].pos.x);
        MaxEq(maxX, srcVerts[f.v3].pos.x);
        if (maxX > mContourXVal + 0.1f) continue;
        if (faceIt >= meshFaces.size()) {
            unsigned int newsize = meshFaces.size() * 2;
            MILO_LOG("RG: too few faces for chord shape - increasing to %d", (int)newsize);
            meshFaces.resize(newsize, RndMesh::Face());
        }
        RndMesh::Face &mf = meshFaces[faceIt++];
        MILO_ASSERT(
            capMap.find(f.v1) != capMap.end() && capMap.find(f.v2) != capMap.end()
                && capMap.find(f.v3) != capMap.end(),
            0x2BC
        );
        if (invert) {
            mf.Set(capMap[f.v1], capMap[f.v3], capMap[f.v2]);
        } else {
            mf.Set(capMap[f.v1], capMap[f.v2], capMap[f.v3]);
        }
    }
    connectingVerts.swap(endVerts);
}

void ChordShapeGenerator::BuildSpan(
    RndMesh *mesh,
    std::map<unsigned short, unsigned short> &connectingVerts,
    int fretA,
    int fretB,
    const Transform &tfA,
    const Transform &tfB,
    Hmx::Color32 col1,
    Hmx::Color32 col2
) {
    MILO_ASSERT(connectingVerts.size(), 0x2D0);
    MILO_ASSERT(bool(fretA) == bool(fretB), 0x2D3);
    if (fretA == 0) {
        for (int i = 1; i < 3; i++) {
            ExtendProfile(
                mesh,
                connectingVerts,
                tfA,
                tfB,
                (float)i / 3.0f,
                1.0f,
                sec2,
                Hmx::Color32(col1),
                Hmx::Color32(col2)
            );
        }
        return;
    }
    float gradeDist = mGradeDistances[abs(fretB - fretA)];
    float frac0 = (1.0f - gradeDist) * 0.5f;
    float frac1 = (1.0f + gradeDist) * 0.5f;
    ExtendProfile(
        mesh, connectingVerts, tfA, tfB, frac0, mFretHeights[fretA], sec1,
        Hmx::Color32(col1), Hmx::Color32(col2)
    );
    ExtendProfile(
        mesh, connectingVerts, tfA, tfB, frac0 + 0.05f,
        0.9f * mFretHeights[fretA] + 0.1f * mFretHeights[fretB], sec1,
        Hmx::Color32(col1), Hmx::Color32(col2)
    );
    ExtendProfile(
        mesh, connectingVerts, tfA, tfB, frac1 - 0.05f,
        0.1f * mFretHeights[fretA] + 0.9f * mFretHeights[fretB], sec1,
        Hmx::Color32(col1), Hmx::Color32(col2)
    );
    ExtendProfile(
        mesh, connectingVerts, tfA, tfB, frac1, mFretHeights[fretB], sec1,
        Hmx::Color32(col1), Hmx::Color32(col2)
    );
    ExtendProfile(
        mesh, connectingVerts, tfA, tfB, 1.0f, mFretHeights[fretB], sec1,
        Hmx::Color32(col1), Hmx::Color32(col2)
    );
}

void ChordShapeGenerator::ExtendProfile(
    RndMesh *mesh,
    std::map<unsigned short, unsigned short> &connectingVerts,
    const Transform &tfA,
    const Transform &tfB,
    float t,
    float fretHeight,
    const CrossSec &crossSec,
    Hmx::Color32 col1,
    Hmx::Color32 col2
) {
    std::map<unsigned short, unsigned short> profileVerts;
    Transform interp;
    InterpolateXfm(tfA, tfB, t, interp);
    Hmx::Color32 col(col1);
    col.g = col1.g + (int)(t * (col2.g - col1.g));
    col.r = col1.r + (int)(t * (col2.r - col1.r));
    col.a = col1.a + (int)(t * (col2.a - col1.a));
    col.b = col1.b + (int)(t * (col2.b - col1.b));
    AddVertProfile(mesh, interp, fretHeight, crossSec, profileVerts, Hmx::Color32(col));
    ConnectVertProfiles(mesh, connectingVerts, profileVerts, crossSec);
    connectingVerts.swap(profileVerts);
}

void ChordShapeGenerator::InterpolateXfm(
    const Transform &a, const Transform &b, float t, Transform &out
) {
    if (t > 0.99f) {
        out = b;
        return;
    }
    if (t < 0.1f) {
        out = a;
        return;
    }
    float ax = a.v.x;
    float az = a.v.z;
    float bx = b.v.x;
    float bz = b.v.z;
    Vector3 eulerA;
    Vector3 eulerB;
    MakeEuler(a.m, eulerA);
    MakeEuler(b.m, eulerB);
    float rA = eulerA.y;
    float rB = eulerB.y;
    MILO_ASSERT(rA != rB, 0x331);
    MILO_ASSERT((fabs(rA) < PI) && (fabs(rB) < PI), 0x333);
    float cosA = (float)cos(rA);
    float slopeA = -(float)sin(rA) / cosA;
    float cosB = (float)cos(rB);
    float slopeB = -(float)sin(rB) / cosB;
    float interceptA = -(slopeA * ax - az);
    float midY = (a.v.y + b.v.y) * 0.5f;
    float isectX = (-(slopeB * bx - bz) - interceptA) / (slopeA - slopeB);
    float isectZ = slopeA * isectX + interceptA;
    if (t < 0.5f) {
        out.v.Set(isectX, midY, isectZ);
        out.v -= a.v;
        out.v *= 2.0f * t;
        out.v += a.v;
    } else {
        out.v.Set(isectX, midY, isectZ);
        out.v -= b.v;
        out.v *= 2.0f * (1.0f - t);
        out.v += b.v;
    }
    Vector3 midEuler(
        (eulerA.x + eulerB.x) * 0.5f,
        (eulerA.y + eulerB.y) * 0.5f,
        (eulerA.z + eulerB.z) * 0.5f
    );
    MakeRotMatrix(midEuler, out.m, true);
}

void ChordShapeGenerator::AddVertProfile(
    RndMesh *mesh,
    const Transform &xfm,
    float fretHeight,
    const CrossSec &secSrc,
    std::map<unsigned short, unsigned short> &newVertMap,
    Hmx::Color32 col
) {
    newVertMap.clear();
    int numNewVerts = secSrc.mVerts.size();
    if (numNewVerts + vertIt > (int)mesh->Verts().size()) {
        unsigned int newsize = mesh->Verts().size() * 2;
        MILO_LOG("RG: too few verts for chord shape - increasing to %d", newsize);
        mesh->Verts().resize(newsize);
    }
    RndMesh::VertVector &meshVerts = mesh->Verts();
    RndMesh::VertVector &srcVerts = mSource->Verts();
    std::set<unsigned short>::const_iterator it = secSrc.mVerts.begin();
    std::set<unsigned short>::const_iterator end = secSrc.mVerts.end();
    for (; it != end; ++it) {
        unsigned short srcIdx = *it;
        unsigned short destIdx = vertIt++;
        RndMesh::Vert &curvert = meshVerts[destIdx];
        curvert = srcVerts[srcIdx];
        curvert.pos.x -= secSrc.mXOffset;
        float pz = curvert.pos.z;
        if (pz > mBaseHeightVal) {
            curvert.pos.z = fretHeight * (pz - mBaseHeightVal) + mBaseHeightVal;
        }
        curvert.color.UnpackAlpha(col.FullColor());
        Multiply(curvert.pos, xfm, curvert.pos);
        newVertMap[srcIdx] = destIdx;
    }
}

void ChordShapeGenerator::ConnectVertProfiles(
    RndMesh *mesh,
    const std::map<unsigned short, unsigned short> &leftMap,
    const std::map<unsigned short, unsigned short> &rightMap,
    const CrossSec &crossSec
) {
    unsigned int numVerts = crossSec.mVerts.size();
    MILO_ASSERT(leftMap.size() == numVerts && rightMap.size() == numVerts, 0x393);
    unsigned int numNewFaces = crossSec.mEdges.size() * 2;
    if (faceIt + numNewFaces > mesh->Faces().size()) {
        unsigned int newsize = mesh->Faces().size() * 2;
        MILO_LOG("RG: too few faces for chord shape - increasing to %d", (int)newsize);
        mesh->Faces().resize(newsize, RndMesh::Face());
    }
    std::vector<RndMesh::Face> &meshFaces = mesh->Faces();
    std::vector<Edge>::const_iterator it = crossSec.mEdges.begin();
    std::vector<Edge>::const_iterator end = crossSec.mEdges.end();
    for (; it != end; ++it) {
        unsigned short a = it->mV0;
        unsigned short b = it->mV1;
        MILO_ASSERT(
            leftMap.find(a) != leftMap.end() && leftMap.find(b) != leftMap.end(), 0x3A8
        );
        MILO_ASSERT(
            rightMap.find(a) != rightMap.end() && rightMap.find(b) != rightMap.end(), 0x3A9
        );
        meshFaces[faceIt++].Set(
            leftMap.find(a)->second, leftMap.find(b)->second, rightMap.find(a)->second
        );
        meshFaces[faceIt++].Set(
            rightMap.find(b)->second, rightMap.find(a)->second, leftMap.find(b)->second
        );
    }
}

DataNode ChordShapeGenerator::OnGenerate(const DataArray *da) {
    RndMesh *mesh = BuildChordMesh();
    NameMesh(mesh, false);
    return DataNode(mesh);
}

DataNode ChordShapeGenerator::OnInvert(const DataArray *da) {
    RndMesh *mesh = da->Obj<RndMesh>(2);
    if (mesh) {
        mesh = MakeInvertedMesh(mesh);
        NameMesh(mesh, true);
        return DataNode(mesh);
    } else
        return DataNode(0);
}

void ChordShapeGenerator::NameMesh(RndMesh *mesh, bool lefty) {
    MILO_ASSERT(mesh && Dir(), 0x3CF);
    const char *name = lefty ? "chord_L" : "chord";
    for (int i = 0; i < mNumSlots; i++) {
        name = MakeString("%s_%d", name, mStringFrets[i]);
    }
    if (Dir()->FindObject(MakeString("%s.mesh", name), false)) {
        int counter = 1;
        const char *newName;
        do {
            newName = MakeString("%s(%d)", name, counter);
            counter++;
        } while (Dir()->FindObject(MakeString("%s.mesh", newName), false));
        name = newName;
    }
    mesh->SetName(MakeString("%s.mesh", name), Dir());
    Dir()->SyncObjects();
    Hmx::Object *milo = ObjectDir::Main()->FindObject("milo", false);
    if (milo) {
        milo->Handle(Message("update_objects"), true);
    }
}

DataNode ChordShapeGenerator::OnSetStringFret(const DataArray *da) {
    int fret = da->Int(3);
    mStringFrets[da->Int(2)] = fret;
    return DataNode(fret);
}

DataNode ChordShapeGenerator::OnGetStringTrans(const DataArray *da) {
    int idx = da->Int(2);
    switch (idx) {
    case 0:
        return DataNode(mString0);
    case 1:
        return DataNode(mString1);
    case 2:
        return DataNode(mString2);
    case 3:
        return DataNode(mString3);
    case 4:
        return DataNode(mString4);
    case 5:
        return DataNode(mString5);
    default:
        return DataNode(0);
    }
}

BEGIN_HANDLERS(ChordShapeGenerator)
    HANDLE(generate_chord_shape, OnGenerate)
    HANDLE(invert_chord_shape, OnInvert)
    HANDLE(set_string_fret, OnSetStringFret)
    HANDLE(get_string_trans, OnGetStringTrans)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x40A)
END_HANDLERS

BEGIN_PROPSYNCS(ChordShapeGenerator)
    SYNC_PROP(chord_source_mesh, mChordSrcMesh)
    SYNC_PROP(finger_source_mesh, mFingerSrcMesh)
    SYNC_PROP(base_cross_section, mBaseXSection)
    SYNC_PROP(contour_cross_section, mContourXSection)
    SYNC_PROP(base_height, mBaseHeight)
    SYNC_PROP(num_slots, mNumSlots)
    SYNC_PROP(string_0_fret, mStringFrets[0])
    SYNC_PROP(string_1_fret, mStringFrets[1])
    SYNC_PROP(string_2_fret, mStringFrets[2])
    SYNC_PROP(string_3_fret, mStringFrets[3])
    SYNC_PROP(string_4_fret, mStringFrets[4])
    SYNC_PROP(string_5_fret, mStringFrets[5])
    SYNC_PROP(string_0, mString0)
    SYNC_PROP(string_1, mString1)
    SYNC_PROP(string_2, mString2)
    SYNC_PROP(string_3, mString3)
    SYNC_PROP(string_4, mString4)
    SYNC_PROP(string_5, mString5)
    SYNC_PROP(fret_height_1, mFretHeights[1])
    SYNC_PROP(fret_height_2, mFretHeights[2])
    SYNC_PROP(fret_height_3, mFretHeights[3])
    SYNC_PROP(fret_height_4, mFretHeights[4])
    SYNC_PROP(fret_height_5, mFretHeights[5])
    SYNC_PROP(fret_height_6, mFretHeights[6])
    SYNC_PROP(grade_distance_1, mGradeDistances[1])
    SYNC_PROP(grade_distance_2, mGradeDistances[2])
    SYNC_PROP(grade_distance_3, mGradeDistances[3])
    SYNC_PROP(grade_distance_4, mGradeDistances[4])
    SYNC_PROP(grade_distance_5, mGradeDistances[5])
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS