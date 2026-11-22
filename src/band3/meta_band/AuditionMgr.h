#pragma once
#include "obj/Object.h"
#include "os/ThreadCall.h"
#include "stl/_vector.h"
#include "utl/Symbol.h"

class LocalBandUser;
class SessionMgr;
class BandUserMgr;
class OvershellPanel;
class BandSongMgr;
class ModifierMgr;
class AuditionDownloader;
class FileCache;
class Message;
class Validator;
class SigninChangedMsg;
class ConnectionStatusChangedMsg;
class ServerStatusChangedMsg;

/** TU5-only Rock Band Network audition support. Neither class has a body in
    our tree and no surviving source has them; only the surface AuditionSessionPanel
    reaches is declared, read off the retail callers. Method names are ours;
    the bodies live in an unpinned retail block after AccomplishmentGroup
    (0x825EAC38 onward) and are unnamed in our map.

    AuditionSessionBuilder: retail RTTI `.?AVAuditionSessionBuilder@@`, vtable
    0x820B954C (21 slots, i.e. a plain Hmx::Object subclass). */
class AuditionSessionBuilder : public Hmx::Object {
public:
    /** retail 0x825EC118; AuditionMgr's session-start state passes
        TheSessionMgr, TheBandUserMgr, TheBandUI's overshell, TheSongMgr and
        TheModifierMgr, in that order. */
    AuditionSessionBuilder(
        SessionMgr *, BandUserMgr *, OvershellPanel *, BandSongMgr *, ModifierMgr *
    );
    /** retail 0x825EBF28: AuditionMgr's `remove_autoplay_users` handler calls
        it, and so does the finish state just before it deletes the builder. */
    void RemoveAutoplayUsers();
    /** retail 0x825EAFA0: AuditionMgr's `has_autoplayer` handler. */
    bool HasAutoplayer() const;
    /** retail 0x825EBE70: called first by AuditionMgr's `apply_settings`
        handler, before the slot settings are copied onto the users. Name ours. */
    void ApplySettings();
    /** One audition slot. AuditionSessionPanel reads the user at +0 and a
        has-part flag at +0x10. */
    struct Slot {
        LocalBandUser *mUser; // 0x0
        /** Autoplay flag: retail ctor `stb 0,4`, GetSlotState `lbz 4` (nonzero
            => state 2), SetSlotAutoplay `stb r5,4`. A byte, not an int. */
        bool mAutoplay; // 0x4
        int unk8; // 0x8
        int unkc; // 0xc
        bool mHasPart; // 0x10
    };

    /** retail 0x825EADC8: 0 = empty, 2 = autoplay. */
    int GetSlotState(int slot) const;
    /** retail 0x825EADF8: 0-7 (guitar, bass, keys, pro keys, drums, pro drums,
        vocals, harmony vocals; see AuditionSessionPanel::GetSlotTrackSym). */
    int GetSlotTrack(int slot) const;
    /** retail 0x825EAE10: 0-3 (easy..expert). */
    int GetSlotDifficulty(int slot) const;
    /** retail 0x825EACA8: no `this`; maps a GetSlotTrack value to the
        ControllerType GetFontCharFromControllerType takes. */
    static int SlotTrackToControllerType(int track);
    /** retail 0x825EBE10. */
    void SetSlotAutoplay(int slot, bool autoplay);
    /** retail 0x825EB1A0. */
    void NextSlotDifficulty(int slot);
    /** retail 0x825EB0D0. */
    void NextSlotTrack(int slot);
    /** retail 0x825EBB08, called from AuditionSessionPanel::Enter. */
    void Reset();

    int unk28; // 0x28
    int unk2c; // 0x2c
    std::vector<Slot *> mSlots; // 0x30
    int unk3c; // 0x3c
    int unk40; // 0x40
    int unk44; // 0x44
    /** The panel to notify; AuditionSessionPanel sets it on Enter and clears it
        on Exit. */
    Hmx::Object *mPanel; // 0x48
    /** retail ctor 0x825EC118: `stb 0,0x4c`, then an 8-iteration loop storing
        1 into each byte of 0x4d..0x54. Retail allocates this class with
        `li r3,0x58` (site 0x82560D34) -- these 9 bytes are what carry the object
        from our former 0x4c to retail's 0x58. */
    bool unk4c; // 0x4c
    bool unk4d[8]; // 0x4d
};

/** Retail global pointer @0x82C7292C. App's init names the object
    "audition_mgr" (SetName, Object vtable slot 16) and stores Symbols at
    +0x48/+0x4c.

    Layout from retail bytes (lane W16-OP):
      * RTTI: `.?AVAuditionMgr@@` has bases Hmx::Object (offset 0) and
        ThreadCallback (PMD m=40, i.e. offset 0x28) -- two vtables, the primary
        0x8209803C (22 slots = Object's 21 + one new virtual) and the
        ThreadCallback one 0x8209802C (3 slots: dtor thunk, ThreadStart,
        ThreadDone). The old `int unk28` at 0x28 was that second vfptr.
      * dtor 0x825618D8: stores both vtables, releases the DataArray at 0x44,
        restores ThreadCallback's vtable at 0x28, then ~Hmx::Object.
      * static-init 0x82C3FD60 constructs a retail object with the ctor fully
        inlined and initialises every member listed below (0x54 is the only
        word it leaves alone).
      * ThreadStart (0x82561168, this = the 0x28 subobject) reads the state at
        +4 (= 0x2c) and the Symbol at +0x20 (= 0x48); ThreadDone (0x825637A8)
        adjusts `this - 0x28` and calls back into AuditionMgr.
    Retail does not override ClassName/SetType (slots 4/5 are Object's) but
    does override Handle (slot 6, 0x825638B0). */
class AuditionMgr : public Hmx::Object, public ThreadCallback {
public:
    virtual ~AuditionMgr();
    virtual DataNode Handle(DataArray *, bool);
    /** Primary slot 21, 0x82563708: the one virtual AuditionMgr introduces.
        A void(void) state-machine step over mState/0x30/0x3c, so named Poll;
        the name is not attested by any string or symbol. */
    virtual void Poll();
    virtual int ThreadStart();
    virtual void ThreadDone(int);

    /** Inline: retail has no out-of-line ctor; the static initializer
        (0x82C3FD60) carries it whole and leaves 0x54 unwritten. */
    AuditionMgr()
        : mState(0), unk30(0), mBuilder(0), mUser(0), mDownloader(0), mValidationFailed(0),
          mSongData(0), mDrumFillsEnabled(0), mSavedBandNoFail(0), mLoadStep(0), mValidator(0),
          unk60(0) {}

    /** 0x825610B8, called once from App::App right after TheUI's Init.
        Names the object "audition_mgr" (Object slot 16) and fills the two
        Symbols at 0x48/0x4c. */
    void Init();
    /** 0x82563428: runs the exit action of the current state, then the entry
        action of the new one. Names of this and every helper below are ours. */
    void SetState(int);
    /** 0x82561EA0: sends `update` to the current screen when it is
        audition_main_screen and the UI is not in transition. */
    void UpdateScreen();
    /** 0x82561A70: sends a message to the current screen when it is
        audition_main_screen and the UI is not in transition. */
    void SendToScreen(Message &);
    /** 0x82561A08: the downloader's progress clamped to [0, 100] and scaled
        to [0, 1]; 0 with no downloader. `get_download_progress`. */
    float DownloadProgress();
    /** 0x825619A8: 0 = none, 1 = loading (states 5-7), 2 = downloaded and
        ready (state 4). `get_loaded_song_status`. */
    int LoadedSongStatus();
    /** 0x82561CB0: copies each builder slot's track and difficulty onto the
        slot's user. */
    void ApplySlotSettings(AuditionSessionBuilder *);
    /** 0x82561DE8: `apply_settings`. */
    void ApplySettings();
    /** 0x82563800: `start_net_poll`; enters state 4 unless already starting
        (states 1-3). */
    void StartNetPoll();
    /** 0x82563858: `start_validation`; enters state 5. */
    void StartValidation();
    /** 0x82561FD0: raises `sign_out` with reason 5 (kRBNMemberSignedOut) when
        the RBN member user signs out mid-audition. */
    DataNode OnMsg(const SigninChangedMsg &);
    /** 0x82562228: raises `lost_connection` (0) in states 4-7. */
    DataNode OnMsg(const ConnectionStatusChangedMsg &);
    /** 0x82562418: raises `lost_connection` (1) in states 4-7. */
    DataNode OnMsg(const ServerStatusChangedMsg &);
    void EnterStart(); // 0x82560CB0, state 1
    void EnterDownload(); // 0x825611F8, state 3
    void ExitLoad(); // 0x82561360, leaving state 5
    void EnterFinish(); // 0x825613C8, state 13
    /** 0x82562608, state 4: raises `lost_connection` (0 = no network, 1 = no
        server), else resumes the downloader. */
    void EnterNetPoll();
    /** 0x82562978, state 5: validates the downloaded songs.dta, registers the
        audition song with TheSongMgr, then caches the midi, milo and weights
        files for PollLoad to validate. */
    void EnterLoad();
    /** 0x82561B20: renames the downloaded song entry to mSong and points its
        `(song (name ...))` at the audition song directory. Name ours. */
    void FixupSongData();
    void EnterValidated(); // 0x82563158, state 6
    /** 0x825632F0, state 7: sets has_validation_failed, logs the
        `rbn/audition/fail` data point and sends on_validation_failed. */
    void EnterValidationFailed();
    void EnterEnd(); // 0x82560DE8, state 11
    void EnterUnmount(); // 0x82563208, state 12
    /** 0x82563578, polled in state 5: once the FileCache is done, validates
        the midi, the lipsync milo and the mogg (the last one 10 ms at a time),
        then enters state 6 (validated) or 7 (failed). */
    void PollLoad();

    int mState; // 0x2c -- switched on by slot 21, ThreadStart and ThreadDone
    int unk30; // 0x30
    AuditionSessionBuilder *mBuilder; // 0x34
    /** The local user whose pad is PlatformMgr's RBN member pad (state 3). */
    LocalBandUser *mUser; // 0x38
    /** Created in state 3, deleted in state 12; slot 21 polls it. */
    AuditionDownloader *mDownloader; // 0x3c
    /** `has_validation_failed`. */
    bool mValidationFailed; // 0x40
    /** The downloaded songs.dta array (EnterLoad); Release()d by the dtor and
        on entering state 0. */
    DataArray *mSongData; // 0x44
    /** The audition song (gUGCAuditionTempSongName); `apply_settings` passes it
        to MetaPerformer::SetSong. */
    Symbol mSong; // 0x48
    /** `set_venue_sym`/`get_venue_sym`; `apply_settings` passes it to
        MetaPerformer::SetVenue. */
    Symbol mVenue; // 0x4c
    /** `toggle_drum_fills`/`are_drum_fills_enabled`. */
    bool mDrumFillsEnabled; // 0x50
    /** MetaPerformer's band no-fail flag, saved in state 1, restored in 11. */
    bool mSavedBandNoFail; // 0x51
    /** Deleted on leaving state 5; the ctor never writes it. */
    FileCache *mFileCache; // 0x54
    /** PollLoad's step: 1 = caching, 2 = midi, 3 = lipsync, 5 = mogg,
        6 = failed, 7 = passed (EnterLoad sets 1). */
    int mLoadStep; // 0x58
    /** The UGC validator (0x104 bytes), allocated in state 5, deleted on
        leaving it. */
    Validator *mValidator; // 0x5c
    bool unk60; // 0x60
};

extern AuditionMgr *TheAuditionMgr;
