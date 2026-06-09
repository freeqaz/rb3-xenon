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
void FillCompressedVertex(CompressedVertex_Xbox &, const RndMesh::Vert &, bool);
void SaveCompressedVertex(const CompressedVertex_Xbox &, BinStream &);
