#pragma once
#include "rndobj/Mesh.h"
#include "ui/UIListProvider.h"
#include "obj/Object.h"

class LessonProvider : public UIListProvider, public Hmx::Object {
public:
    class LessonEntry {
    public:
        LessonEntry() {}
        LessonEntry(Symbol s, bool b) : unk0(s), unk4(b) {}
        Symbol unk0;
        bool unk4;
    };

    LessonProvider();
    virtual ~LessonProvider();
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual void Custom(int, int, class UIListCustom *, Hmx::Object *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;
    virtual bool IsActive(int) const;
    virtual void InitData(RndDir *);

    void Update(Symbol);
    const LessonEntry &GetLessonEntry(int) const;

    std::vector<Symbol> *mCategories; // 0x2c
    std::vector<Symbol> *mLessons; // 0x30
    RndMat *mCategoryMat; // 0x34
    RndMat *mLessonMat; // 0x38
    std::vector<LessonEntry> mLessonEntries; // 0x3c
};