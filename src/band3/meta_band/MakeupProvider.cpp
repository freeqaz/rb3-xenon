#include "meta_band/MakeupProvider.h"
#include "os/Debug.h"
#include "ui/UILabel.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

MakeupProvider::MakeupProvider(Symbol gender) : mCurrentMakeupList(0) {
    mMakeupEyes.push_back(none_makeup);
    mMakeupLips.push_back(none_makeup);
    int eyecount, lipcount;
    if (gender == male) {
        eyecount = 16;
        lipcount = 10;
    } else {
        eyecount = 29;
        lipcount = 17;
    }
    for (int i = 1; i <= eyecount; i++) {
        Symbol eyeSym(MakeString("%s_makeup_eyes_%i", gender.Str(), i));
        mMakeupEyes.push_back(eyeSym);
    }
    for (int i = 1; i <= lipcount; i++) {
        Symbol lipSym(MakeString("%s_makeup_lips_%i", gender.Str(), i));
        mMakeupLips.push_back(lipSym);
    }
    mCurrentMakeupList = &mMakeupEyes;
}

void MakeupProvider::Update(Symbol type) {
    MILO_ASSERT(type == eyes || type == lips, 0x3D);
    if (type == eyes) {
        mCurrentMakeupList = &mMakeupEyes;
    } else
        mCurrentMakeupList = &mMakeupLips;
}

void MakeupProvider::Text(int, int data, UIListLabel *slot, UILabel *label) const {
    MILO_ASSERT(slot, 0x47);
    MILO_ASSERT(label, 0x48);
    if (slot->Matches("name")) {
        // Retail reloads the Symbol from its own stack slot (lwz r4, 0x50(r1))
        // instead of re-deriving it from the sret pointer -- i.e. a named local.
        Symbol sym = DataSymbol(data);
        label->SetTextToken(sym);
    } else {
        label->SetTextToken(gNullStr);
    }
}

RndMat *MakeupProvider::Mat(int, int data, UIListMesh *slot) const {
    MILO_ASSERT(data < NumData(), 0x57);
    if (slot->Matches("new_bg")) {
        return nullptr;
    } else
        return slot->DefaultMat();
}

void MakeupProvider::UpdateExtendedText(int, int i_iData, UILabel *label) const {
    MILO_ASSERT(i_iData < NumData(), 99);
    if (strcmp(label->Name(), "asset_desc_makeup.lbl") == 0) {
        Symbol data = DataSymbol(i_iData);
        Symbol descSym(MakeString("%s_desc", data.Str()));
        label->SetTextToken(descSym);
    } else if (strcmp(label->Name(), "asset_progress_makeup.lbl") == 0) {
        // Retail: function-local static Symbol (guard-bit test + inline
        // ??0Symbol ctor), not the Symbols*.h global the Wii dev tree uses.
        static Symbol customize_asset_progress("customize_asset_progress");
        label->SetTokenFmt(customize_asset_progress, i_iData + 1, NumData());
    } else
        label->SetTextToken(gNullStr);
}

Symbol MakeupProvider::DataSymbol(int data) const {
    MILO_ASSERT_RANGE(data, 0, NumData(), 0x7C);
    return mCurrentMakeupList->at(data);
}

int MakeupProvider::NumData() const { return mCurrentMakeupList->size(); }
