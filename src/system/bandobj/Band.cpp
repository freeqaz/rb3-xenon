#include "Band.h"
#include "obj/Object.h"
#include "obj/ObjMacros.h"
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


// Classes whose TUs are not yet ported in-tree. Minimal local declarations keep
// the BandInit call sequence (and therefore its relocation layout) identical to
// retail; the Init symbols stay undefined externals, which objdiff pairs fine.
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
};
class DialogDisplay { public: static void Init(); };
class InstrumentDifficultyDisplay { public: static void Init(); };
class MicInputArrow { public: static void Init(); };
class PatchRenderer { public: static void Init(); static void Terminate(); };
class PlayerDiffIcon { public: static void Init(); };
class ScrollbarDisplay { public: static void Init(); };

DataNode OnPaletteSync(DataArray *array) {
    // TODO: this engine revision tracks refs with the intrusive ObjRef ring
    // (Hmx::Object::Refs() -> const ObjRef&), not rb3-Wii's vector<ObjRef*>,
    // so the oracle's reverse-iterator walk does not port directly. Body left
    // unimplemented; only BandInit/BandTerminate are being matched here.
    ColorPalette *colpal = array->Obj<ColorPalette>(1);
    (void)colpal;
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
        // Retail inlines a standalone REGISTER_OBJ_FACTORY(ObjectDir) here (6-instr
        // StaticClassName+RegisterFactory shape, address-confirmed via Ghidra decompile
        // of 0x8227ACC8 + target .s at instrs 103-108) that neither dc3 nor rb3-Wii's
        // BandInit() source shows -- an older-revision leftover. Call target identity
        // is score-invisible (functionRelocDiffs=none), so matching the shape here is
        // what matters.
        REGISTER_OBJ_FACTORY(ObjectDir);
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
