#include "Band.h"
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "beatmatch/BeatMaster.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/SongData.h"
#include "meta/DataArraySongInfo.h"
#include "obj/DataUtl.h"
#include "bandobj/BandButton.h"
#include "bandobj/BandCamShot.h"
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandConfiguration.h"
#include "bandobj/BandCrowdMeter.h"
#include "bandobj/BandDirector.h"
#include "bandobj/BandFaceDeform.h"
#include "bandobj/BandHeadShaper.h"
#include "bandobj/BandHighlight.h"
#include "bandobj/BandIKEffector.h"
#include "bandobj/BandLeadMeter.h"
#include "bandobj/BandList.h"
#include "bandobj/BandRetargetVignette.h"
#include "bandobj/BandScoreboard.h"
#include "bandobj/BandSongPref.h"
#include "bandobj/BandStarDisplay.h"
#include "bandobj/Label3d.h"
#include "bandobj/BandSwatch.h"
#include "bandobj/BandWardrobe.h"
#include "bandobj/CharKeyHandMidi.h"
#include "bandobj/CheckboxDisplay.h"
#include "bandobj/ChordShapeGenerator.h"
#include "bandobj/CrowdAudio.h"
#include "bandobj/CrowdMeterIcon.h"
#include "bandobj/EndingBonus.h"
#include "bandobj/GemTrackDir.h"
#include "bandobj/InlineHelp.h"
#include "bandobj/LayerDir.h"
#include "bandobj/MeterDisplay.h"
#include "bandobj/MiniLeaderboardDisplay.h"
#include "bandobj/OutfitConfig.h"
#include "bandobj/OverdriveMeter.h"
#include "bandobj/OvershellDir.h"
#include "bandobj/PitchArrow.h"
#include "bandobj/ReviewDisplay.h"
#include "bandobj/ScoreDisplay.h"
#include "bandobj/SongSectionController.h"
#include "bandobj/StarDisplay.h"
#include "bandobj/StreakMeter.h"
#include "bandobj/TrackPanelDir.h"
#include "bandobj/UnisonIcon.h"
#include "bandobj/VocalTrackDir.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "ui/UILabel.h"
#include "utl/Song.h"
#include "world/ColorPalette.h"


// Every class BandInit() registers is now declared by its own header. Until
// lane W16-PZ five of them (DialogDisplay, InstrumentDifficultyDisplay,
// MicInputArrow, PlayerDiffIcon, ScrollbarDisplay) were empty local stubs
// `class X { public: static void Init(); };`, i.e. 1-byte classes, while the
// real definitions are 88/432/500/440/452 B (retail factory allocations). Band.cpp
// is scatter-included into BandCharacter.cpp, so that TU carried a second,
// conflicting definition of each class. Their Init() is out-of-line in every
// real header, so BandInit's call sequence is unchanged.
//
// X6: BandConfiguration's factory-only shim is GONE -- the real TU is ported
// (bandobj/BandConfiguration.{h,cpp}), and its header is included above. The
// shim existed because retail's BandConfiguration::Init() is a trivial
// `{ Register(); }` one-liner *defined in its own header*, so it is visible to
// this TU (via Band.cpp's scatter-include into BandCharacter.cpp) and /Ob2
// inlines the whole StaticClassName+RegisterFactory pattern directly into
// BandInit() -- exactly like BandCamShot/BandCrowdMeter alongside it. An
// external-call stub would desync BandInit's instruction sequence. The real
// header keeps Init() inline for precisely that reason, so BandInit's shape is
// preserved; verified by rebuild at symbol granularity, not by whole-file cmp.
//
// BandSong is still a shim, and is the same case: retail's Init() is a
// header-inline `{ Register(); }` one-liner, so it inlines into BandInit()
// here too. Factory-only shim over the real base (Song, already ported).
class BandSong : public Song {
public:
    OBJ_CLASSNAME(BandSong);
    OBJ_SET_TYPE(BandSong);
    NEW_OBJ(BandSong)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(BandSong)

private:
    // Retail overrides Song's null-returning CreateSong (BandSong primary-table
    // slot 10 = 0x8229CC10, Song's = 0x827C6F38). Without it a BandSong builds
    // no song data or master at all.
    virtual void CreateSong(Symbol, DataArray *, HxSongData **, HxMaster **);
};

// retail 0x8229CC10: new SongData (0x174), new BeatMaster(sdata, 1) between
// DataMacroWarning(false/true), then BeatMaster::Load of the song's DataArray
// with this song's MidiReceiver as the only receiver.
void BandSong::CreateSong(
    Symbol s, DataArray *arr, HxSongData **songdata, HxMaster **hxmaster
) {
    SongData *sdata = new SongData();
    *songdata = sdata;
    DataMacroWarning(false);
    BeatMaster *bmaster = new BeatMaster(sdata, 1);
    *hxmaster = bmaster;
    DataMacroWarning(true);
    std::vector<MidiReceiver *> mreceivers;
    mreceivers.push_back(this);
    PlayerTrackConfigList plist(0);
    DataArraySongInfo info(arr, 0, s);
    bmaster->Load(&info, 4, &plist, true, kSongData_NoValidation, &mreceivers);
}
#include "bandobj/DialogDisplay.h"
#include "bandobj/InstrumentDifficultyDisplay.h"
#include "bandobj/MicInputArrow.h"
#include "bandobj/PatchRenderer.h"
#include "bandobj/PlayerDiffIcon.h"
#include "bandobj/ScrollbarDisplay.h"

DataNode OnPaletteSync(DataArray *array) {
    // Every outfit config and swatch referencing the palette picks up its
    // new colours.
    ColorPalette *colpal = array->Obj<ColorPalette>(1);
    for (ObjRefList::const_iterator it = colpal->Refs().begin(); it != colpal->Refs().end();
         ++it) {
        Hmx::Object *owner = RefPtrOf(it)->RefOwner();
        OutfitConfig *cfg = dynamic_cast<OutfitConfig *>(owner);
        if (cfg)
            cfg->Recompose();
        BandSwatch *swatch = dynamic_cast<BandSwatch *>(owner);
        if (swatch)
            swatch->SetColors(colpal);
    }
    return 0;
}

void BandInit() {
    if (DataGetMacro("INIT_BAND")) {
        BandButton::Init();
        BandCamShot::Init();
        BandConfiguration::Init();
        BandCrowdMeter::Init();
        BandHighlight::Init();
        BandIKEffector::Init();
        BandRetargetVignette::Init();
        BandLabel::Init();
        BandLeadMeter::Init();
        BandList::Init();
        BandScoreboard::Init();
        BandStarDisplay::Init();
        BandCharacter::Init();
        BandCharDesc::Init();
        OutfitConfig::Init();
        BandDirector::Init();
        BandFaceDeform::Init();
        BandSong::Init();
        BandWardrobe::Init();
        DialogDisplay::Init();
        BandSwatch::Init();
        CrowdMeterIcon::Init();
        EndingBonus::Init();
        GemTrackDir::Init();
        // Retail registers Label3d here: 0x8227ACC8 is Label3d::StaticClassName (it
        // builds the Symbol "Label3d") and its NewObject is 0x8227BD60.
        Label3d::Init();
        LayerDir::Init();
        PatchRenderer::Init();
        PitchArrow::Init();
        PlayerDiffIcon::Init();
        InstrumentDifficultyDisplay::Init();
        ScrollbarDisplay::Init();
        CheckboxDisplay::Init();
        ScoreDisplay::Init();
        ReviewDisplay::Init();
        StarDisplay::Init();
        MeterDisplay::Init();
        MiniLeaderboardDisplay::Init();
        MicInputArrow::Init();
        InlineHelp::Init();
        StreakMeter::Init();
        OverdriveMeter::Init();
        TrackPanelDir::Init();
        VocalTrackDir::Init();
        ChordShapeGenerator::Init();
        UnisonIcon::Init();
        OvershellDir::Init();
        CharKeyHandMidi::Init();
        SongSectionController::Init();
        BandSongPref::Init();
        BandHeadShaper::Init();
        CrowdAudio::Init();

        TheDebug.AddExitCallback(BandTerminate);
        static DataNode &mode = DataVariable("band.play_mode");
        mode = DataNode(kDataSymbol, Symbol("coop_bg").Str());
        DataRegisterFunc("palette_sync", OnPaletteSync);
        PreloadSharedSubdirs("band");
    }
}

void BandTerminate() {
    if (DataGetMacro("INIT_BAND")) {
        UILabel::Terminate();
        BandHeadShaper::Terminate();
        PatchRenderer::Terminate();
        BandSwatch::Terminate();
        OutfitConfig::Terminate();
        BandDirector::Terminate();
        BandCharacter::Terminate();
    }
}
