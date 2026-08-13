#pragma once
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "math/Vec.h"
#include "math/Mtx.h"
#include "math/Color.h"
#include "math/Geo.h"
#include "rndobj/TransAnim.h"
#include <vector>
#include "MultiMesh.h"

struct BuildPoly {
    BuildPoly();
    ~BuildPoly() {}

    Hmx::Polygon mPoly; // 0x0
    Transform mTransform; // 0xc
};

void SetLocalScale(RndTransformable *, const Vector3 &);

DataNode GetNormalMapTextures(ObjectDir *);
DataNode GetRenderTextures(ObjectDir *);
DataNode GetRenderTexturesNoZ(ObjectDir *);
DataNode OnTestDrawGroups(DataArray *);

void ResetColors(std::vector<Hmx::Color> &colors, int newNumColors);
void RndScaleObject(Hmx::Object *, float, float);
bool AnimContains(const RndAnimatable *anim1, const RndAnimatable *anim2);
float ConvertFov(float, float);
void PreMultiplyAlpha(Hmx::Color &);
void RandomPointOnMesh(RndMesh *, Vector3 &, Vector3 &);

bool AnimContains(const RndAnimatable *, const RndAnimatable *);
bool SortDraws(RndDrawable *, RndDrawable *);
bool SortPolls(const RndPollable *, const RndPollable *);

void ScrambleXfms(RndMultiMesh *);
void DistributeXfms(RndMultiMesh *, int, float);
void MoveXfms(RndMultiMesh *, const Vector3 &);
void ScaleXfms(RndMultiMesh *, const Vector3 &);
void SortXfms(RndMultiMesh *, const Vector3 &);
bool XfmSort(RndMultiMesh::Instance &, RndMultiMesh::Instance &);
void RandomXfms(RndMultiMesh *);

#ifdef HX_NATIVE
// RB3-360 retail has no MetaMaterial and no CreateAndSetMetaMat: the function needs
// RndMat::CreateMetaMaterial, which needs the string "MetaMaterial" -- 0 occurrences
// in orig/45410914/band.exe (lane METAMAT-1). It is DELETED from the X360 match build
// (whose cflags define no HX_NATIVE) and survives ONLY as a no-op so that
// milo-native-engine's src/platform/Rnd_Wgpu.cpp, which calls it four times, keeps
// compiling. Removing those four calls is an ENGINE change request, not ours to land.
void CreateAndSetMetaMat(RndMat *);
#endif

void FixVertOrder(const RndMesh *, RndMesh *);

void UtilDrawSphere(const Vector3 &, float, const Hmx::Color &);
void UtilDrawLine(const Vector2 &, const Vector2 &, const Hmx::Color &);
void UtilDrawString(const char *, const Vector3 &, const Hmx::Color &);
void UtilDrawAxes(const Transform &, float, const Hmx::Color &);
void UtilDrawBox(const Transform &tf, const Box &box, const Hmx::Color &col, bool b4);
void UtilDrawRect2D(const Vector2 &v1, const Vector2 &v2, const Hmx::Color &color);
void UtilDrawCircle2D(const Vector2 &, float, const Hmx::Color &, int);
void UtilDrawCylinder(const Transform &, float, float, const Hmx::Color &, int);
void UtilDrawPlane(const Plane &, const Vector3 &, const Hmx::Color &, int, float, bool);
void UtilDrawCigar(const Transform &, const float *const, const float *const, const Hmx::Color &, int);

void TransformKeys(RndTransAnim *, const Transform &);
void SpliceKeys(RndTransAnim *, RndTransAnim *, float, float);
void LinearizeKeys(RndTransAnim *, float, float, float, float, float);

void TestTextureSize(ObjectDir *, int, int, int, int, int);
void TestTexturePaths(ObjectDir *);
void TestMaterialTextures(ObjectDir *);

void RndUtlPreInit();
void RndUtlInit();
void RndUtlTerminate();
void RndSplasherPoll();
void RndSplasherSuspend();
void RndSplasherResume();

typedef void (*SplashFunc)(void);
void SetRndSplasherCallback(SplashFunc func1, SplashFunc func2, SplashFunc func3);

void MakeTangentsLate(RndMesh *);
void CalcBox(RndMesh *, Box &);
void CalcSphere(RndTransAnim *, Sphere &);
MatShaderOptions GetDefaultMatShaderOpts(const Hmx::Object *, RndMat *);
void ResetNormals(RndMesh *);
void TessellateMesh(RndMesh *);
void ClearAO(RndMesh *);
void BurnXfm(RndMesh *, bool keepTranslation);
void AttachMesh(RndMesh *, RndMesh *);
void RandomPointOnMesh(RndMesh *, Vector3 &, Vector3 &);
void BuildFromBSP(RndMesh *);
void ConvertBonesToTranses(ObjectDir *, bool);

const char *CacheResource(const char *, const Hmx::Object *);

int GenerationCount(RndTransformable *, RndTransformable *);
bool GroupedUnder(RndGroup *, Hmx::Object *);

RndAnimatable *AnimController(Hmx::Object *);
void AddMotionSphere(RndTransformable *, Sphere &);

void EndianSwapBitmap(RndBitmap &bmap);
void SwapDxtEndianness(RndBitmap *bmap);

void Clip(BuildPoly &, const Plane &, bool);

typedef void (*SplashFunc)(void);
void SetRndSplasherCallback(
    SplashFunc pollFunc, SplashFunc suspendFunc, SplashFunc resumeFunc
);
