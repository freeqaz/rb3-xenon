#pragma once
#include "ui/UIListProvider.h"
#include "obj/Object.h"

class FaceHairProvider : public UIListProvider, public Hmx::Object {
public:
    FaceHairProvider();
    virtual ~FaceHairProvider() {}
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;

    std::vector<Symbol> mMaleFaceHair; // 0x2c
    std::vector<Symbol> mFemaleFaceHair; // 0x38
    std::vector<Symbol> *mFaceHair; // 0x44
};