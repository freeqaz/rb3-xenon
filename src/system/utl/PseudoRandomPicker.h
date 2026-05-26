#pragma once
#include "os/Debug.h"
#include <cstdlib>
#include <list>
#include <vector>

template <class T>
class PseudoRandomPicker {
public:
    PseudoRandomPicker() : mMode(1), mSeed(0.5), mNumGets(0) {}
    ~PseudoRandomPicker() {}

    void SetMode(int mode) { mMode = mode; }
    void AddItem(const T item) { mItems.push_back(item); }
    void SetNumGets(int i) { mNumGets = i; }

    void AddItems(const std::vector<T> &itemVec) {
        for (int i = 0; i < itemVec.size(); i++) {
            T cur = itemVec[i];
            mItems.push_back(cur);
        }
    }

    void Randomize() {
        MILO_ASSERT(Size() > 0, 0x83);
        int size = Size();
        for (int n = Size(); n != 0; n--) {
            // Pop the first element
            typename std::list<T>::iterator first = mItems.begin();
            T val = *first;
            mItems.erase(first);
            Size();
            // Pick a random position and insert there
            int idx = rand() % size;
            typename std::list<T>::iterator it = mItems.begin();
            if (idx != 0) {
                for (int i = idx; i != 0; i--) {
                    ++it;
                }
            }
            mItems.insert(it, val);
            Size();
        }
    }

    int Size() const { return mItems.size(); }
    void Clear() { mItems.resize(0); }

    const T GetItem(int idx) {
        if (idx >= 0) {
            // Count list size
            int size = 0;
            typename std::list<T>::iterator it = mItems.begin();
            for (; it != mItems.end(); ++it) {
                size++;
            }
            if ((unsigned int)idx <= (unsigned int)size) {
                // Walk to idx-th node
                it = mItems.begin();
                if (idx != 0) {
                    for (int i = idx; i != 0; i--) {
                        ++it;
                    }
                }
                T val = *it;
                mItems.erase(it);
                mItems.insert(mItems.end(), val);
                return val;
            }
        }
        return T(0);
    }

    const T GetNext() {
        MILO_ASSERT(Size() > 0, 0x52);
        int idx;
        switch (mMode) {
        case 0:
            idx = 0;
            break;
        case 2: {
            int s = Size();
            idx = rand() % (s - mNumGets % s);
            break;
        }
        case 3:
            idx = rand() % Size();
            break;
        default: {
            int i1 = mSeed * (float)Size() + 1.01f;
            idx = rand() % i1;
            break;
        }
        }
        mNumGets++;
        return GetItem(idx);
    }

public:
    std::list<T> mItems; // 0x0 - items?
    int mMode; // 0x8
    float mSeed; // 0xc
    int mNumGets; // 0x10
};
