#pragma once
#include "game/GameMic.h"
#include "obj/Msg.h"
#include "obj/Dir.h"
#include "synth/MicManagerInterface.h"

class GameMicManager : public MicManagerInterface, public MsgSource {
public:
    GameMicManager();
    virtual ~GameMicManager();
    virtual void HandleMicsChanged();
    virtual void SetPlayback(bool);
    virtual float GetEnergyForMic(const MicClientID &);

    void LoadMicFx();
    void SetSynapseProximity(float);
    void SetSynapseSlackyness(float);
    void SetSynapseFocus(float);
    void SetSynapseAmount(float);
    void DeleteMic(int);
    void CreateMic(int);
    bool HasMic(const MicClientID &) const;
    GameMic *GetMic(const MicClientID &);
#ifdef HX_NATIVE
    void InitFakeMics();
#endif
    void ApplyPlayback(bool, GameMic *) const;
    void HookUpFxForMicId(GameMic *);
    int GetMicCount() const;
    void SetOverdriveEffectEnable(bool);
    void Poll(float);
    void SetPitchCorrectionTarget(bool, bool, float, float, float, float, float);

    static void Init();
    static void Terminate();

    ObjDirPtr<ObjectDir> unk20; // 0x1c
    bool unk2c;
    bool unk2d;
    bool unk2e;
    bool unk2f;
    float mSynapseProximity; // 0x2c
    float mSynapseFocus; // 0x30
    int mMicCount; // 0x34
    std::vector<GameMic *> mMics; // 0x38
#ifdef HX_NATIVE
    // DC3-only fake-mic feature (frame_rate mode). Retail RB3 X360 has no
    // mFakeMics: the retail funclet places the Hmx::Object vbase at 0x4c, which
    // only holds if mPlayback follows mMics directly (no intervening vector).
    // rb3-Wii's GameMicManager also does not carry this in the retail build.
    std::vector<GameMic *> mFakeMics;
#endif
    bool mPlayback; // 0x44
};

extern GameMicManager *TheGameMicManager;