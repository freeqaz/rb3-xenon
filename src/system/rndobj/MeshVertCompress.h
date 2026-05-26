#pragma once
#include "rndobj/Mesh.h"
#include "math/Vec.h"
#include "utl/BinStream.h"

struct CompressedVertex_Xbox {
    int mPosX;
    int mPosY;
    int mPosZ;
    int mColor; // 0xc - packed color
    int mNormal;
    int mTangent;
    int mBinormal;
    int mBoneIndices;
    int mBoneWeights;
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
void FillCompressedVertex(CompressedVertex_Xbox &, const RndMesh::Vert &, bool);
void SaveCompressedVertex(const CompressedVertex_Xbox &, BinStream &);
