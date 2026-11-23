#pragma once
#include "midi/MidiParser.h"
#include "obj/Object.h"
#include "os/Joypad.h"
#include "synth/Faders.h"
#include "obj/Dir.h"
#include "synth/Sequence.h"

class TambourineManager : public Hmx::Object {
public:
    TambourineManager(class VocalPlayer &);
    virtual ~TambourineManager();
    virtual DataNode Handle(DataArray *, bool);

    void PostLoad();
    void ComputeTambourinePoints();
    void PostDynamicAdd();
    void Start();
    void Restart();
    void Poll(float);
    int TambourineSwing(int);
    void Jump(float);
    void SetPaused(bool);
    void SetTambourine(bool);
    void GameOver();
    void Rollback(float, float);
    void SetBank(ObjectDir *bank) { mBank = bank; }
    const std::vector<int> &TambourineGems() const;
    bool IsTambourineButton(JoypadButton) const;
    void HandleButtonDown();
    bool GemHit(int) const;
    bool GemProcessed(int) const;
    void TambourineSucceed(int);
    void TambourineFail(int, bool);
    void LocalTambourineSoloEnd(int, int);
    DataNode OnPlayTambourine(DataArray *);
    void OnRemoteTambourineSucceeding(DataArray *);

    class VocalPlayer &mPlayerRef; // 0x28
    bool mIsLocal; // 0x2c
    ObjDirPtr<ObjectDir> mBank; // 0x30
    Sequence *mTambourineSequence; // 0x3c
    Fader *mTambourineFader; // 0x40
    MidiParser *mTambourineParser; // 0x44
    std::vector<int> unk3c; // 0x48
    int mTambourineIdx; // 0x54
    bool unk48; // 0x58
    int unk4c; // 0x5c
    int mTambourineWindowTicks; // 0x60
    float mTambourineCrowdSuccess; // 0x64
    float mTambourineCrowdFailure; // 0x68
    bool mTambourineActive; // 0x6c
    int unk60; // 0x70
    float mTambourinePoints; // 0x74
    int unk68; // 0x78
    std::vector<int> unk6c; // 0x7c
    // No bool between unk6c and unk78: DC3's mic-mute state bool (unk74) is not
    // in retail (mGemStates at standalone-offset 0x90, proven by vector
    // begin/end loads lwz 0x90/0x94 in fn_826DC7C0/fn_826DCB88/fn_826DD580; the
    // ctor stores a full word (stw) at 0x88), and the native build no longer
    // carries it either since retail's Poll never mutes the mics.
    int unk78; // 0x88 (retail 0x88)
    int unk7c; // 0x8c (retail 0x8c)
    std::vector<int> mGemStates; // 0x90 (retail 0x90)
};