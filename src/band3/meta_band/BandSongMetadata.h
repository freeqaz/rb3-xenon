#pragma once
#include "meta/SongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "obj/Data.h"
#include "os/DateTime.h"
#include <hash_map>

class BandSongMetadata : public SongMetadata {
public:
    BandSongMetadata(BandSongMgr *);
    BandSongMetadata(DataArray *, DataArray *, bool, BandSongMgr *);
    virtual ~BandSongMetadata() {}
    virtual DataNode Handle(DataArray *, bool);
    virtual void Save(BinStream &);
    virtual void Load(BinStream &);
    virtual bool IsVersionOK() const;

    void InitBandSongMetadata();
    const char *Title() const;
    const char *Artist() const;
    const char *Album() const;
    int AlbumTrackNum() const;
    Symbol Genre() const;
    int LengthMs() const;
    bool HasAlternatePath() const;
    bool MuteWinCues() const;
    const std::hash_map<Symbol, float> &Ranks() const;
    int Rating() const;
    float GuidePitchVolume() const;
    int VocalTonicNote() const;
    int SongKey() const;
    int SongTonality() const;
    float ScrollSpeed() const;
    float TuningOffset() const;
    const char *VocalPercussionBank() const;
    const char *DrumKitBank() const;
    bool HasAlbumArt() const;
    bool IsMasterRecording() const;
    Symbol BandFailCue() const;
    int RealGuitarTuning(int) const;
    int RealBassTuning(int) const;
    Symbol Decade() const;
    bool HasPart(Symbol) const;
    virtual bool HasPart(Symbol, bool) const;
    float Rank(Symbol) const;
    bool HasVocalHarmony() const;
    bool IsPrivate() const;
    bool IsRanked() const;
    Symbol LengthSym() const;
    Symbol RatingSym() const;
    Symbol SourceSym() const;
    Symbol VocalPartsSym() const;
    Symbol HasProGuitarSym() const;
    bool HasKeys() const;
    bool HasGuitar() const;
    bool HasBass() const;
    Symbol HasKeysSym() const;
    bool HasSolo(Symbol) const;
    Symbol HasSoloSym(Symbol) const;
    bool IsUGC() const;
    bool IsUGCPlus() const;
    const char *MidiUpdate() const;
    bool IsDownload() const;

    static int sBandSaveVer;

    // Retail X360 layout. Ground truth: retail DataArray ctor fn_82584A08
    // (member init stores), COMDAT dtor fn_8255D5E0 (String dtors @0x4c/0x58/
    // 0x64/0xd4/0xe0/0xf0, map dtor @0x9c, vector free @0x128), and Handle
    // fn_82588000 (mTitle.mStr@0x54, dates@0x72/0x78, rating short@0xb8).
    // Differences vs the rb3-Wii header:
    //  - mHasAlternatePath + mIsBonus/mIsFake/mIsTutorial/mMuteWinCues moved
    //    up, between mLengthMs and mRanks (matches dta parse order).
    //  - mRating sits AFTER mRanks, not paired with mAlbumTrackNum.
    //  - mVocalPercussionBank/mDrumKitBank/mBandFailCue are String on retail
    //    X360 (Wii used Symbol).
    //  - mIsTriFrame does not exist on retail X360 (no ctor parse, no
    //    Save/Load stream field, no member store) — removed.
    String mTitle; // 0x4c
    String mArtist; // 0x58
    String mAlbum; // 0x64
    short mAlbumTrackNum; // 0x70
    DateTime mDateRecorded; // 0x72
    DateTime mDateReleased; // 0x78
    Symbol mGenre; // 0x80
    int mAnimTempo; // 0x84
    Symbol mVocalGender; // 0x88
    int mLengthMs; // 0x8c
    bool mHasAlternatePath; // 0x90
    int mBasePoints; // 0x94
    bool mIsBonus; // 0x98
    bool mIsFake; // 0x99
    bool mIsTutorial; // 0x9a
    bool mMuteWinCues; // 0x9b
    std::hash_map<Symbol, float> mRanks; // 0x9c (STLport hash_map = 0x1c)
    short mRating; // 0xb8
    float mGuidePitchVolume; // 0xbc
    int mVocalTonicNote; // 0xc0
    int mSongKey; // 0xc4
    int mSongTonality; // 0xc8
    float mSongScrollSpeed; // 0xcc
    float mTuningOffsetCents; // 0xd0
    String mVocalPercussionBank; // 0xd4
    String mDrumKitBank; // 0xe0
    bool mHasAlbumArt; // 0xec
    bool mIsMasterRecording; // 0xed
    String mBandFailCue; // 0xf0
    int mRealGuitarTuning[6]; // 0xfc
    int mRealBassTuning[4]; // 0x114
    bool mHasDiscUpdate; // 0x124
    std::vector<Symbol> mSolos; // 0x128
    BandSongMgr *mSongMgr; // 0x134
    // total size: 0x138
};

    DECLARE_MESSAGE(MetadataLoadedMsg, "metadata_loaded")
    END_MESSAGE