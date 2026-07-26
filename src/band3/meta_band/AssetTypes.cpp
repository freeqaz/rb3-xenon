#include "meta_band/AssetTypes.h"
#include "os/Debug.h"
#include "system/utl/Symbols.h"
#include "system/utl/Symbols2.h"
#include "system/utl/Symbols3.h"
#include "system/utl/Symbols4.h"

Symbol GetSymbolFromAssetType(AssetType asset_type) {
    // Retail declares all 19 of these as function-local statics at the TOP of the
    // body (guard word 0x82E001F0, bits 0x1..0x40000 in this exact order); the
    // rb3-Wii oracle used file-scope Symbols*.h globals instead.
    static Symbol bandana("bandana");
    static Symbol bass("bass");
    static Symbol drum("drum");
    static Symbol earrings("earrings");
    static Symbol eyebrows("eyebrows");
    static Symbol facehair("facehair");
    static Symbol feet("feet");
    static Symbol glasses("glasses");
    static Symbol guitar("guitar");
    static Symbol hair("hair");
    static Symbol hands("hands");
    static Symbol hat("hat");
    static Symbol keyboard("keyboard");
    static Symbol legs("legs");
    static Symbol mic("mic");
    static Symbol piercings("piercings");
    static Symbol rings("rings");
    static Symbol torso("torso");
    static Symbol wrist("wrist");
    Symbol symbol = gNullStr;
    switch (asset_type) {
    case kAssetType_None:
        break;
    case kAssetType_Bandana:
        symbol = bandana;
        break;
    case kAssetType_Bass:
        symbol = bass;
        break;
    case kAssetType_Drum:
        symbol = drum;
        break;
    case kAssetType_Earrings:
        symbol = earrings;
        break;
    case kAssetType_Eyebrows:
        symbol = eyebrows;
        break;
    case kAssetType_FaceHair:
        symbol = facehair;
        break;
    case kAssetType_Feet:
        symbol = feet;
        break;
    case kAssetType_GlassesAndMasks:
        symbol = glasses;
        break;
    case kAssetType_Gloves:
        symbol = hands;
        break;
    case kAssetType_Guitar:
        symbol = guitar;
        break;
    case kAssetType_Hair:
        symbol = hair;
        break;
    case kAssetType_Hat:
        symbol = hat;
        break;
    case kAssetType_Keyboard:
        symbol = keyboard;
        break;
    case kAssetType_Legs:
        symbol = legs;
        break;
    case kAssetType_Mic:
        symbol = mic;
        break;
    case kAssetType_Piercings:
        symbol = piercings;
        break;
    case kAssetType_Rings:
        symbol = rings;
        break;
    case kAssetType_Torso:
        symbol = torso;
        break;
    case kAssetType_Wrists:
        symbol = wrist;
        break;
    default:
        MILO_ASSERT(false, 0x61);
    }
    return symbol;
}

AssetType GetAssetTypeFromSymbol(Symbol symbol) {
    for (int i = 0; i < 20; i++) {
        Symbol assetType = GetSymbolFromAssetType((AssetType)i);
        if (assetType == symbol) {
            return (AssetType)i;
        }
    }
    MILO_WARN("AssetType: (%s) not found.", symbol);
    return (AssetType)0;
}

AssetGender GetAssetGenderFromSymbol(Symbol symbol) {
    static Symbol male("male");
    static Symbol female("female");
    if (symbol == gNullStr) {
        return kAssetGender_None;
    }

    if (symbol == male) {
        return kAssetGender_Male;
    } else if (symbol == female) {
        return kAssetGender_Female;
    }

    MILO_WARN("AssetGender: (%s) not found.", symbol);
    return kAssetGender_None;
}

Symbol GetSymbolFromAssetBoutique(AssetBoutique boutique) {
    // guard word 0x82E00214, bits 0x1..0x100 in declaration order
    static Symbol boutique_boss("boutique_boss");
    static Symbol boutique_romantic("boutique_romantic");
    static Symbol boutique_scrapper("boutique_scrapper");
    static Symbol boutique_sheathed("boutique_sheathed");
    static Symbol boutique_showman("boutique_showman");
    static Symbol boutique_thatstore("boutique_thatstore");
    static Symbol boutique_warrior("boutique_warrior");
    static Symbol boutique_tshirts("boutique_tshirts");
    static Symbol boutique_premium("boutique_premium");
    Symbol symbol = gNullStr;
    switch (boutique) {
    case kAssetBoutique_None:
        break;
    case kAssetBoutique_Boss:
        symbol = boutique_boss;
        break;
    case kAssetBoutique_Romantic:
        symbol = boutique_romantic;
        break;
    case kAssetBoutique_Scrapper:
        symbol = boutique_scrapper;
        break;
    case kAssetBoutique_Sheathed:
        symbol = boutique_sheathed;
        break;
    case kAssetBoutique_Showman:
        symbol = boutique_showman;
        break;
    case kAssetBoutique_ThatStore:
        symbol = boutique_thatstore;
        break;
    case kAssetBoutique_Warrior:
        symbol = boutique_warrior;
        break;
    case kAssetBoutique_TShirts:
        symbol = boutique_tshirts;
        break;
    case kAssetBoutique_Premium:
        symbol = boutique_premium;
        break;
    default:
        MILO_ASSERT(false, 0xb6);
    }
    return symbol;
}

AssetBoutique GetAssetBoutiqueFromSymbol(Symbol symbol) {
    for (int i = 0; i < 10; i++) {
        Symbol assetType = GetSymbolFromAssetBoutique((AssetBoutique)i);
        if (assetType == symbol) {
            return (AssetBoutique)i;
        }
    }
    MILO_WARN("AssetBoutique: (%s) not found.", symbol);
    return kAssetBoutique_None;
}

const char *GetConfigNameFromAssetType(AssetType assetType) {
    const char *name = gNullStr;
    switch (assetType) {
    case kAssetType_None:
        break;
    case kAssetType_Bandana:
        name = "facehair.cfg";
        break;
    case kAssetType_Bass:
        name = "bass.cfg";
        break;
    case kAssetType_Drum:
        name = "drum.cfg";
        break;
    case kAssetType_Earrings:
        name = "earrings.cfg";
        break;
    case kAssetType_Eyebrows:
        name = "eyebrows.cfg";
        break;
    case kAssetType_FaceHair:
        name = "facehair.cfg";
        break;
    case kAssetType_Feet:
        name = "feet.cfg";
        break;
    case kAssetType_GlassesAndMasks:
        name = "glasses.cfg";
        break;
    case kAssetType_Gloves:
        name = "hands.cfg";
        break;
    case kAssetType_Guitar:
        name = "guitar.cfg";
        break;
    case kAssetType_Hair:
        name = "hair.cfg";
        break;
    case kAssetType_Hat:
        name = "hair.cfg";
        break;
    case kAssetType_Keyboard:
        name = "keyboard.cfg";
        break;
    case kAssetType_Legs:
        name = "legs.cfg";
        break;
    case kAssetType_Mic:
        name = "mic.cfg";
        break;
    case kAssetType_Piercings:
        name = "piercings.cfg";
        break;
    case kAssetType_Rings:
        name = "rings.cfg";
        break;
    case kAssetType_Torso:
        name = "torso.cfg";
        break;
    case kAssetType_Wrists:
        name = "wrist.cfg";
        break;
    default:
        MILO_ASSERT(false, 0x10f);
    }
    return name;
}

Symbol GetDefaultAssetFromAssetType(AssetType assetType, AssetGender assetGender) {
    // guard word 0x82E0025C, bits 0x1..0x8000 in declaration order (note retail
    // declares the gendered pairs FIRST, then the none_* set)
    static Symbol male_torso_naked("male_torso_naked");
    static Symbol femalebra_cotton("femalebra_cotton");
    static Symbol male_hands_naked("male_hands_naked");
    static Symbol female_hands_naked("female_hands_naked");
    static Symbol male_feet_naked("male_feet_naked");
    static Symbol female_feet_naked("female_feet_naked");
    static Symbol none_bandana("none_bandana");
    static Symbol none_earrings("none_earrings");
    static Symbol none_eyebrows("none_eyebrows");
    static Symbol none_facehair("none_facehair");
    static Symbol none_glasses("none_glasses");
    static Symbol none_hair("none_hair");
    static Symbol none_hat("none_hat");
    static Symbol none_piercings("none_piercings");
    static Symbol none_rings("none_rings");
    static Symbol none_wrists("none_wrists");
    Symbol asset = gNullStr;
    switch (assetType) {
    case kAssetType_None:
    case kAssetType_Bass:
    case kAssetType_Drum:
    case kAssetType_Guitar:
    case kAssetType_Keyboard:
    case kAssetType_Legs:
    case kAssetType_Mic:
        break;
    case kAssetType_Bandana:
        asset = none_bandana;
        break;
    case kAssetType_Earrings:
        asset = none_earrings;
        break;
    case kAssetType_Eyebrows:
        asset = none_eyebrows;
        break;
    case kAssetType_FaceHair:
        asset = none_facehair;
        break;
    case kAssetType_Feet:
        if (assetGender == kAssetGender_Male) {
            asset = male_feet_naked;
        } else if (assetGender == kAssetGender_Female) {
            asset = female_feet_naked;
        }
        break;
    case kAssetType_GlassesAndMasks:
        asset = none_glasses;
        break;
    case kAssetType_Gloves:
        if (assetGender == kAssetGender_Male) {
            asset = male_hands_naked;
        } else if (assetGender == kAssetGender_Female) {
            asset = female_hands_naked;
        }
        break;
    case kAssetType_Hair:
        asset = none_hair;
        break;
    case kAssetType_Hat:
        asset = none_hat;
        break;
    case kAssetType_Piercings:
        asset = none_piercings;
        break;
    case kAssetType_Rings:
        asset = none_rings;
        break;
    case kAssetType_Torso:
        if (assetGender == kAssetGender_Male) {
            asset = male_torso_naked;
        } else if (assetGender == kAssetGender_Female) {
            asset = femalebra_cotton;
        }
        break;
    case kAssetType_Wrists:
        asset = none_wrists;
        break;
    default:
        MILO_ASSERT(false, 0x17e);
    }
    return asset;
}

BandCharDesc::Patch::Category GetPatchCategoryFromAssetType(AssetType assetType) {
    BandCharDesc::Patch::Category result = BandCharDesc::Patch::kPatchNone;
    switch (assetType) {
    case kAssetType_Bass:
        result = BandCharDesc::Patch::kPatchBass;
        break;
    case kAssetType_Drum:
        result = BandCharDesc::Patch::kPatchDrum;
        break;
    case kAssetType_Feet:
        result = BandCharDesc::Patch::kPatchFeet;
        break;
    case kAssetType_Guitar:
        result = BandCharDesc::Patch::kPatchGuitar;
        break;
    case kAssetType_Hair:
        result = BandCharDesc::Patch::kPatchHair;
        break;
    case kAssetType_Keyboard:
        result = BandCharDesc::Patch::kPatchKeyboard;
        break;
    case kAssetType_Legs:
        result = BandCharDesc::Patch::kPatchTorso; // should be legs, i think this was a
                                                   // bug
        break;
    case kAssetType_Mic:
        result = BandCharDesc::Patch::kPatchMic;
        break;
    case kAssetType_Torso:
        result = BandCharDesc::Patch::kPatchTorso;
    }
    return result;
}

bool IsInstrumentAssetType(Symbol symbol) {
    if (symbol == guitar || symbol == bass || symbol == drum || symbol == mic
        || symbol == keyboard) {
        return true;
    }
    return false;
}
