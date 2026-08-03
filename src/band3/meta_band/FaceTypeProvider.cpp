#include "meta_band/FaceTypeProvider.h"
#include "meta_band/PrefabMgr.h"
#include "os/Debug.h"
#include "ui/UILabel.h"
#include "ui/UIListLabel.h"
#include "utl/Symbol.h"
#include "utl/Symbols3.h"

FaceTypeProvider::FaceTypeProvider() {
    // Retail 360 builds the gender symbol as a FUNCTION-LOCAL STATIC here
    // (guard word + guarded ??0Symbol@@QAA@PBD@Z from the "male" literal),
    // not as a reference to the Symbols3 global `male`.
    static Symbol male("male");
    Update(male);
}

void FaceTypeProvider::Update(Symbol genderSym) {
    mFaceTypes.clear();
    PrefabMgr *pPrefabMgr = PrefabMgr::GetPrefabMgr();
    MILO_ASSERT(pPrefabMgr, 0x1C);
    pPrefabMgr->GetFaceTypes(mFaceTypes, genderSym);
}

void FaceTypeProvider::Text(int, int idx, UIListLabel *slot, UILabel *label) const {
    MILO_ASSERT(slot, 0x23);
    MILO_ASSERT(label, 0x24);
    if (slot->Matches("option")) {
        label->SetTextToken(DataSymbol(idx));
    } else
        label->SetTextToken(gNullStr);
}

Symbol FaceTypeProvider::DataSymbol(int data) const {
    MILO_ASSERT_RANGE(data, 0, NumData(), 0x32);
    return mFaceTypes[data];
}

int FaceTypeProvider::NumData() const { return mFaceTypes.size(); }
