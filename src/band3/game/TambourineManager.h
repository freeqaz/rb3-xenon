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

    class VocalPlayer &mPlayerRef; // 0x1c
    bool mIsLocal; // 0x20
    ObjDirPtr<ObjectDir> mBank; // 0x24
    Sequence *mTambourineSequence; // 0x30
    Fader *mTambourineFader; // 0x34
    MidiParser *mTambourineParser; // 0x38
    std::vector<int> unk3c; // 0x3c
    int mTambourineIdx; // 0x44
    bool unk48; // 0x48
    int unk4c; // 0x4c
    int mTambourineWindowTicks; // 0x50
    float mTambourineCrowdSuccess; // 0x54
    float mTambourineCrowdFailure; // 0x58
    bool mTambourineActive; // 0x5c
    int unk60; // 0x60
    float mTambourinePoints; // 0x64
    int unk68; // 0x68
    std::vector<int> unk6c; // 0x6c
    // unk74 is a DC3-only mic-mute state bool. Retail rb3-xenon TambourineManager
    // is 4 bytes smaller (mGemStates at standalone-offset 0x90, proven by
    // vector begin/end loads lwz 0x90/0x94 in fn_826DC7C0/fn_826DCB88/fn_826DD580;
    // ours is 0x94) and its ctor stores a full word (stw) at 0x88 — i.e. unk78
    // sits directly after the unk6c vector, with no bool between. Guarded out of
    // the retail build so unk78/unk7c pack at 0x88/0x8c and mGemStates lands 0x90.
#ifdef HX_NATIVE
    bool unk74; // native-only mic-mute state
#endif
    int unk78; // 0x78 (retail 0x88)
    int unk7c; // 0x7c (retail 0x8c)
    std::vector<int> mGemStates; // 0x80 (retail 0x90)
};