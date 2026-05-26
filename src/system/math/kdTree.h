#pragma once
#include "math/Geo.h"
#include "math/Vec.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include <stdint.h>
#include <float.h>
#include <list>

// kdTree size: 0x2c
template <class T>
class kdTree {
public:
    enum SplitPlaneType {
        // mean = 0, mean = 1
        // SAH = 2
    };

    class kdTriList {
    public:
        MEM_ARRAY_OVERLOAD(kdTriList, 0xC6);

#ifdef HX_NATIVE
        typedef intptr_t IndexType;
#else
        typedef int IndexType;
#endif

        kdTriList() : mIndex(0) {}

        static IndexType EndMarker() { return (IndexType)-1; }

        static kdTriList *Allocate(unsigned int num) {
            kdTriList *list = new kdTriList[num + 1];
            list[num].mIndex = EndMarker();
            return list;
        }

        void SetItem(T *item) { mIndex = (IndexType)item; }
        T *GetItem() const { return (T *)mIndex; }
        bool IsEnd() const { return mIndex == EndMarker(); }

        IndexType mIndex; // Triangle* or sentinel
    };

    class kdTreeNode {
    public:
        struct Stack {
            kdTreeNode *node;
            float tNear;
            float tFar;
        };

        kdTreeNode() {
            SetTriList(0);
            mFlags = 0x8000;
            mData.real = 0;
            mData.index = 0;
        }
        ~kdTreeNode() {
            if (mFlags & 0x8000 && GetTriList()) {
                delete[] GetTriList();
                SetTriList(nullptr);
            }
        }

        // On PPC (ILP32), pointer/float/bitfield all fit in 4 bytes and share
        // a single union — the float's bottom 2 mantissa bits are repurposed as
        // the axis index. On LP64, the pointer is 8 bytes so it must be stored
        // separately from the 4-byte float+bitfield pack.
#ifdef HX_NATIVE
        kdTriList *mTriList; // LP64: separate 8-byte pointer
        union {
            float real;
            struct {
                unsigned int unused : 30;
                unsigned int index : 2;
            };
        } mData;
        kdTriList *GetTriList() const { return mTriList; }
        void SetTriList(kdTriList *p) { mTriList = p; }
#else
        union {
            kdTriList *triList;
            float real;
            // bitmask here? the bottom 2 bits are its own thing
            struct {
                unsigned int unused : 30;
                unsigned int index : 2;
            };
        } mData; // 0x0
        kdTriList *GetTriList() const { return mData.triList; }
        void SetTriList(kdTriList *p) { mData.triList = p; }
#endif
        short mFlags;

        unsigned short GetIsLeaf() const { return mFlags & 0x8000; }

        float EvaluateSplit(
            const Box &box,
            const std::list<Triangle *> &triangles,
            unsigned char idx,
            float threshold
        ) const {
            unsigned int axis = (unsigned int)(unsigned char)idx;
            if (threshold > box.mMax[axis] || threshold < box.mMin[axis]) {
                return FLT_MAX;
            }

            // Split box at threshold on given axis
            Box leftBox(box.mMin, box.mMax);
            Box rightBox(box.mMin, box.mMax);
            leftBox.mMax[axis] = threshold;
            rightBox.mMin[axis] = threshold;

            float totalArea = box.SurfaceArea();
            float invTotalArea = 1.0f / totalArea;
            float leftAreaFrac = (float)(leftBox.SurfaceArea() * invTotalArea);
            float rightAreaFrac = (float)(rightBox.SurfaceArea() * invTotalArea);

            float leftCount = 0.0f;
            float rightCount = 0.0f;
            for (auto it = triangles.begin(); it != triangles.end(); ++it) {
                Triangle *tri = *it;
                if (leftBox.Contains(*tri)) {
                    leftCount = leftCount + 1.0f;
                } else if (rightBox.Contains(*tri)) {
                    rightCount = rightCount + 1.0f;
                } else {
                    if (::Intersect(*tri, leftBox)) {
                        leftCount = leftCount + 0.5f;
                    }
                    if (::Intersect(*tri, rightBox)) {
                        rightCount = rightCount + 0.5f;
                    }
                }
            }

            return (float)(rightCount * rightAreaFrac) + leftCount * leftAreaFrac + 0.3f;
        }

        bool FindSplit_Mean(const Box &box, const std::list<Triangle *> &items) {
            float yDiff = box.mMax.y - box.mMin.y;
            float zDiff = box.mMax.z - box.mMin.z;

            if (box.mMax.x - box.mMin.x > yDiff) {
                mData.index = 0;
            } else {
                mData.index = 1;
            }
            if (zDiff > yDiff) {
                mData.index = 2;
            }

            unsigned int vecIdx = mData.index;
            float idxDiff = box.mMax[vecIdx] - box.mMin[vecIdx];

            unsigned int numContains = 0;
            mData.real = idxDiff / 2.0f + box.mMin[mData.index];
            mData.index = 3;

            double fsum = 0.0;
            if (!items.empty()) {
                FOREACH (it, items) {
                    Triangle *cur = *it;
                    Vector3 v[3];
                    v[0].Set(
                        cur->origin.x + cur->frame.x.x,
                        cur->origin.y + cur->frame.x.y,
                        cur->origin.z + cur->frame.x.z
                    );
                    v[1].Set(
                        cur->origin.x + cur->frame.y.x,
                        cur->origin.y + cur->frame.y.y,
                        cur->origin.z + cur->frame.y.z
                    );
                    v[2].Set(
                        cur->origin.x + cur->frame.z.x,
                        cur->origin.y + cur->frame.z.y,
                        cur->origin.z + cur->frame.z.z
                    );
                    for (int i = 0; i < 3; i++) {
                        if (box.Contains(v[i])) {
                            fsum += v[i][vecIdx];
                            numContains++;
                        }
                    }
                }
                if (numContains != 0) {
                    mData.real = (float)(fsum / numContains);
                    mData.index = 3;
                }
            }
            return true;
        }
        bool FindSplit_SAH(const Box &, const std::list<Triangle *> &);
        void Pack(
            SplitPlaneType s,
            const Box &inDimensions,
            std::list<Triangle *> &items,
            kdTreeNode *pBase,
            unsigned char uc
        );

        MEM_ARRAY_OVERLOAD(kdTreeNode, 0xEC);
    };

    kdTree(const Box &box) {
        mBounds.Set(box.mMin, box.mMax);
        mNodes = new kdTreeNode[0x8000];
        for (u16 i = 0; i < 0x8000; i++) {
            mNodes[i].mFlags |= i;
        }
    }
    ~kdTree() { delete[] mNodes; }

    void Add(T *item) { mItems.push_back(item); }
    void PackNodes(SplitPlaneType s, unsigned char uc) {
        mNodes->Pack(s, mBounds, mItems, mNodes, uc);
    }

    bool Intersect(const Vector3 &, const Vector3 &, float, float &) const;

private:
    std::list<T *> mItems; // 0x0 - objects?
    kdTreeNode *mNodes; // 0x8
    Box mBounds; // 0xc - bounding box of the tree?
};

template <class T>
void kdTree<T>::kdTreeNode::Pack(
    SplitPlaneType s,
    const Box &inDimensions,
    std::list<Triangle *> &items,
    kdTreeNode *pBase,
    unsigned char uc
) {
    if (uc < 0xF) {
        typename std::list<Triangle *>::iterator it = items.begin();
        if (it != items.end()) {
            unsigned int uCount = 0;
            do {
                ++it;
                uCount++;
            } while (it != items.end());

            if (uCount >= 10) {
            bool bFound = false;
            if (s == 0) {
                bFound = FindSplit_Mean(inDimensions, items);
            } else if (s == 1) {
                bFound = FindSplit_Mean(inDimensions, items);
            } else if (s == 2) {
                bFound = FindSplit_SAH(inDimensions, items);
            } else {
                MILO_FAIL("Invalid split plane type");
            }

            if (bFound) {
                unsigned int iAxis = mData.index & 3;
                float fSplit = mData.real;
                if (fSplit < inDimensions.mMin[iAxis]) {
                } else if (fSplit > inDimensions.mMax[iAxis]) {
                } else {
                    Box minBox(inDimensions.mMin, inDimensions.mMax);
                    Box maxBox(inDimensions.mMin, inDimensions.mMax);
                    minBox.mMax[iAxis] = fSplit;
                    maxBox.mMin[iAxis] = fSplit;

                    std::list<Triangle *> leftList;
                    std::list<Triangle *> rightList;
                    bool bContinue = true;
                    for (it = items.begin(); it != items.end();) {
                        Triangle *pTri = *it;
                        ++it;

                        MILO_ASSERT(::Intersect(*pTri, inDimensions), 0x166);
                        bool bLeftIntersect = ::Intersect(*pTri, minBox);
                        bool bRightIntersect = ::Intersect(*pTri, maxBox);
                        if (!bLeftIntersect && !bRightIntersect) {
                            bContinue = false;
                            break;
                        }
                        if (bLeftIntersect) {
                            leftList.push_back(pTri);
                        }
                        if (bRightIntersect) {
                            rightList.push_back(pTri);
                        }
                    }

                    if (bContinue
                        && (unsigned short)(mFlags & 0x7fff) <= 0x3ffe) {
                        items.clear();
                        unsigned char ucNext = uc + 1;
                        mFlags &= 0x7fff;
#ifdef HX_NATIVE
                        kdTreeNode *pNode0 = pBase + ((unsigned short)mFlags * 2 + 1);
                        kdTreeNode *pNode1 = pNode0 + 1;
#else
                        kdTreeNode *pNode1 = reinterpret_cast<kdTreeNode *>(
                            reinterpret_cast<char *>(pBase)
                            + (((unsigned short)mFlags + 1) << 4)
                        );
                        reinterpret_cast<kdTreeNode *>(
                            reinterpret_cast<char *>(pBase)
                            + (((unsigned short)mFlags) << 4) + 8
                        )
                            ->Pack(s, minBox, leftList, pBase, ucNext);
#endif
#ifdef HX_NATIVE
                        pNode0->Pack(s, minBox, leftList, pBase, ucNext);
#endif
                        pNode1->Pack(s, maxBox, rightList, pBase, ucNext);
                        return;
                    }
                }
            }
        }
        }
    }

    MILO_ASSERT(GetIsLeaf(), 0x19F);
    typename std::list<Triangle *>::iterator it = items.begin();
    if (it == items.end()) {
        SetTriList(nullptr);
    } else {
        unsigned int uCount = 0;
        do {
            ++it;
            uCount++;
        } while (it != items.end());

        SetTriList(kdTriList::Allocate(uCount));
        kdTriList *pCurr = GetTriList();
        for (it = items.begin(); it != items.end();) {
            MILO_ASSERT(!pCurr->IsEnd(), 0x1AE);
            pCurr->SetItem(*it);
            it = items.erase(it);
            ++pCurr;
        }
    }
}
