#pragma once
#include "tour/TourChar.h"
#include "bandobj/PatchDir.h"
#include "utl/BinStream.h"
#include <hash_map>

class TourCharLocal : public TourChar {
public:
    TourCharLocal();
    virtual DataNode Handle(DataArray *, bool);
    virtual RndTex *GetTexAtPatchIndex(int, bool) const;
    virtual ~TourCharLocal();
    virtual void SetDirty(bool, int);

    void GenerateGUID();
    void SetCharacterName(const char *);
    void SetFinalized(bool);
    void SaveDb(BinStream &);

    std::hash_map<int, PatchDir *> unk4c; // 0x4c
};