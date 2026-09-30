#pragma once
#include "rndobj/Mesh.h"
#include "math/Vec.h"
#include "utl/BinStream.h"

struct CompressedVertex_Xbox {
    float mPosX;
    float mPosY;
    float mPosZ;
    int mColor; // 0xc - packed color
    unsigned int mNormal;
    unsigned int mTangent;
    unsigned int mBinormal;
    unsigned int mBoneIndices;
    unsigned int mBoneWeights;
};

void PackVector(
    unsigned int &,
    const Vector4 &,
    unsigned char,
    unsigned char,
    unsigned char,
    unsigned char,
    bool
);
// RB3 retail 0x82737688 takes TWO args: both retail callers (RndMesh save loop
// 0x824185E0, DxMesh::Fill 0x82737990) set only r3/r4, and the body never reads
// r5 (its first touch is `li r5,0xa` for PackVector). DC3's is
// (..., bool normalize) -- a later revision; RB3 has no such parameter.
void FillCompressedVertex(CompressedVertex_Xbox &, const RndMesh::Vert &);
void SaveCompressedVertex(const CompressedVertex_Xbox &, BinStream &);
