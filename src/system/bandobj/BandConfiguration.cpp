#include "bandobj/BandConfiguration.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandWardrobe.h"
#include "obj/DataUtl.h"
#include "utl/Symbols.h"

INIT_REVS(BandConfiguration)
int BandConfiguration::TargTransforms::sNumPlayModes;

BandConfiguration::BandConfiguration() {
    for (int i = 0; i < 4; i++) {
        Waypoint *wp = Hmx::Object::New<Waypoint>();
        wp->SetRadius(1000.0f);
        wp->SetStrictRadiusDelta(0);
        mXfms[i].mWay = wp;
        for (int j = 0; j < 3; j++) {
            mXfms[i].xfms[j].xfm.Reset();
            mXfms[i].xfms[j].targName = "";
        }
    }
}

BandConfiguration::~BandConfiguration() {
    for (int i = 0; i < 4; i++) {
        delete mXfms[i].mWay;
    }
}

#define kNumPlayModes 3

int BandConfiguration::ConfigIndex() {
    Symbol playmode = TheBandWardrobe->GetPlayMode();
    DataArray *macro = DataGetMacro("BAND_PLAY_MODES");
    MILO_ASSERT(macro->Size() == kNumPlayModes, 0x33);
    for (int i = 0; i < 3; i++) {
        if (macro->Sym(i) == playmode)
            return i;
    }
    MILO_FAIL("invalid mode %s", playmode);
    return 0;
}

// ★ THE BAND-SLOT PLACEMENT PATH.
//
// This is the mechanism that puts the four band members where the venue says
// they stand. Each of the four slots carries one stored Transform PER PLAY
// MODE; SyncPlayMode picks the row for the current mode, pushes it into that
// slot's runtime Waypoint, resolves the slot's target name against the venue's
// four target names, and teleports the resolved BandCharacter onto the
// waypoint. Character::Teleport (char/Character.cpp:486) normalizes the
// waypoint's world transform into the character's local transform and hands
// the waypoint to the bone servo for continued regulation.
//
// Every transform here comes from the .milo via Load(); nothing is computed.
void BandConfiguration::SyncPlayMode() {
    int idx = ConfigIndex();
    for (int i = 0; i < 4; i++) {
        TargTransform &curtargxfm = mXfms[i].xfms[idx];
        mXfms[i].mWay->SetLocalXfm(curtargxfm.xfm);
        BandCharacter *bchar = TheBandWardrobe->FindTarget(
            curtargxfm.targName, TheBandWardrobe->mVenueNames
        );
        if (bchar) {
            bchar->Teleport(mXfms[i].mWay);
        }
#ifdef HX_NATIVE
        // Native-only diagnostic. Silence is not success: a slot whose
        // targName does not resolve leaves that band member unplaced, and
        // without this the failure is indistinguishable from "the venue has
        // no member in that slot". Wii/X360-compile-inert.
        else if (!curtargxfm.targName.Null()) {
            MILO_WARN(
                "BandConfiguration::SyncPlayMode: waypoint slot %d targName '%s' did "
                "not resolve to a BandCharacter (venue targets: '%s' '%s' '%s' '%s') -- "
                "this member will not be placed",
                i,
                curtargxfm.targName,
                TheBandWardrobe->mVenueNames.names[0],
                TheBandWardrobe->mVenueNames.names[1],
                TheBandWardrobe->mVenueNames.names[2],
                TheBandWardrobe->mVenueNames.names[3]
            );
        }
#endif
    }
}

BinStream &operator>>(BinStream &bs, BandConfiguration::TargTransforms &tts) {
    int i;
    for (i = 0; i < Min(3, BandConfiguration::TargTransforms::sNumPlayModes); i++) {
        bs >> tts.xfms[i].targName;
        bs >> tts.xfms[i].xfm;
    }
    // A file authored with MORE play modes than this build knows about still
    // has to be consumed field-for-field, or every following object in the
    // stream desyncs. Read and discard the surplus rows.
    for (; i < BandConfiguration::TargTransforms::sNumPlayModes; i++) {
        Symbol s;
        Transform t;
        bs >> s;
        bs >> t;
    }
    return bs;
}

SAVE_OBJ(BandConfiguration, 0x6E)

BEGIN_LOADS(BandConfiguration)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> TargTransforms::sNumPlayModes;
    for (int i = 0; i < 4; i++) {
        bs >> mXfms[i];
    }
    if (TheBandWardrobe) {
        TheBandWardrobe->SetModeSink(this);
    }
END_LOADS

BEGIN_COPYS(BandConfiguration)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(BandConfiguration)
    BEGIN_COPYING_MEMBERS
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 3; j++) {
                COPY_MEMBER(mXfms[i].xfms[j])
            }
        }
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(BandConfiguration)
    HANDLE(store_configuration, OnStoreConfiguration)
    HANDLE(release_configuration, OnReleaseConfiguration)
    HANDLE_ACTION(sync_play_mode, SyncPlayMode())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x9F)
END_HANDLERS

DataNode BandConfiguration::OnStoreConfiguration(DataArray *da) {
    int cfgidx = ConfigIndex();
    for (int i = 0; i < 4; i++) {
        TargTransform &curtarg = mXfms[i].xfms[cfgidx];
        BandCharacter *bchar = TheBandWardrobe->GetCharacter(i);
        if (bchar) {
            curtarg.targName = TheBandWardrobe->VenueNames().names[i];
            curtarg.xfm = bchar->LocalXfm();
        }
    }
    SyncPlayMode();
    return 0;
}

DataNode BandConfiguration::OnReleaseConfiguration(DataArray *da) {
    for (int i = 0; i < 4; i++) {
        BandCharacter *bchar = TheBandWardrobe->GetCharacter(i);
        if (bchar)
            bchar->Teleport(0);
    }
    return 0;
}

BEGIN_PROPSYNCS(BandConfiguration)
END_PROPSYNCS
