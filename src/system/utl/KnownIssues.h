#pragma once
#include "obj/Data.h"
#include "utl/Str.h"

/** Metadata for known issues, meant to be displayed on-screen. */
class KnownIssues {
public:
    KnownIssues();
    void Draw();
    void Display(String, float);
    void Init();

    static DataNode OnDisplayKnownIssues(DataArray *);
    static DataNode OnToggleLastKnownIssues(DataArray *);
    static DataNode OnToggleAllowKnownIssues(DataArray *);

private:
    /** The known issue's name. */
    String mName; // 0x0
    /** The known issue's description. */
    String mDescription; // 0x8
    float unk_0x10; // 0x10
    /** Whether or not to display the known issue. */
    bool mDisplay; // 0x14
};

extern KnownIssues TheKnownIssues;
