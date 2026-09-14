#include "meta_band/MainHubPanel.h"
#include "bandobj/BandLabel.h"
#include "beatmatch/TrackType.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "game/NetGameMsgs.h"
#include "math/Rand.h"
#include "meta_band/BandMachine.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandUI.h"
#include "meta_band/LockStepMgr.h"
#include "meta_band/MainHubMessageProvider.h"
#include "meta_band/Matchmaker.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/ProfileMessages.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/UIEventMgr.h"
#include "net/NetMessage.h"
#include "net/NetSession.h"
#include "net/Server.h"
#include "net_band/RockCentral.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "ui/UIPanel.h"
#include "utl/Locale.h"
#include "utl/Messages.h"
#include "utl/Messages3.h"
#include "utl/Messages4.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

namespace {
    class MainHubAdvanceMsg : public NetMessage {
    public:
        MainHubAdvanceMsg() {}
        MainHubAdvanceMsg(NetUIState state, const char *name) : unk4(state), unk8(name) {}
        virtual ~MainHubAdvanceMsg() {}
        virtual void Save(BinStream &bs) const {
            bs << (unsigned char)unk4;
            bs << unk8;
        }
        virtual void Load(BinStream &bs) {
            unsigned char state;
            bs >> state;
            unk4 = (NetUIState)state;
            bs >> unk8;
        }
        virtual void Dispatch() {
            MainHubPanel *panel =
                ObjectDir::Main()->Find<MainHubPanel>(unk8.c_str(), true);
            static Message advanceMsg("advance", 0);
            advanceMsg[0] = unk4;
            panel->HandleType(advanceMsg);
        }
        NETMSG_BYTECODE(MainHubAdvanceMsg);
        NETMSG_NAME(MainHubAdvanceMsg);

        NETMSG_NEWNETMSG(MainHubAdvanceMsg);

        NetUIState unk4;
        String unk8;
    };

    NetMessage *MainHubAdvanceMsg::NewNetMessage() { return new MainHubAdvanceMsg(); }
}

MainHubPanel::MainHubPanel()
    : mHubState(kMainHubState_Main), mHubOverride(kMainHubOverride_None),
      mMessageProvider(0), mCurrentMessage(0), mMessageRotationMs(3000.0f), unkb8(0),
      unkbc(0), unkc0(kScoreBand) {
    mMachineMgr = TheSessionMgr->mMachineMgr;
    mWaitingStateLock = new LockStepMgr("main_hub_waiting", this);
    MainHubAdvanceMsg::Register();
}

MainHubPanel::~MainHubPanel() {
    delete mMessageProvider;
    delete mWaitingStateLock;
}

void MainHubPanel::Enter() {
    TheProfileMgr.AddSink(this, PrimaryProfileChangedMsg::Type());
    TheSessionMgr->AddSink(this);
    TheSessionMgr->GetMatchmaker()->AddSink(this);
    mMachineMgr->AddSink(this);
    TheBandUI.GetOvershell()->AddSink(this, OvershellOverrideEndedMsg::Type());
    static Symbol message_rotation_ms("message_rotation_ms");
    DataArray *rotMsArr = TypeDef()->FindArray(message_rotation_ms, false);
    if (rotMsArr)
        mMessageRotationMs = rotMsArr->Float(1);
    mMessageProvider = new MainHubMessageProvider(this);
#ifndef HX_NATIVE
    TheServer.AddSink(this, UserLoginMsg::Type());
#endif
    UIPanel::Enter();
    RefreshData();
    SetMainHubOverride(kMainHubOverride_None);
    UpdateStateView(mHubState, kMainHubState_None, mHubOverride, kMainHubOverride_None);
}

void MainHubPanel::Poll() {
    UIPanel::Poll();
    if (mMessageTimer.Running()) {
        // Retail (fn_82622550) makes ONE out-of-line call here --
        // `bl fn_82270188` == ?SplitMs@Timer@@QAAMXZ -- and compares its float
        // return directly against mMessageRotationMs (`lfs f0, 0x4c(r30)`).
        // Split() and Ms() are both header-inline, so spelling them separately
        // emits the __mftb sequence and the CyclesToMs float math inline in
        // place of that single `bl`, which is the whole of this row's gap.
        // NOTE the rb3-Wii oracle is WRONG for retail X360 here: it spells this
        // `Timer::CyclesToMs(mMessageTimer.mCycles)`.  Retail bytes outrank the
        // oracle.  (Lane W16-AN.)
        if (mMessageTimer.SplitMs() > mMessageRotationMs) {
            mMessageTimer.Restart();
            int num = mMessageProvider->NumData();
            if (num == 0) {
                mMessageTimer.Stop();
                mCurrentMessage = 0;
                PrepareProfilesAndMessages();
            } else {
                mCurrentMessage = (mCurrentMessage + 1) % num;
                UpdateHeader();
            }
        }
    }
}

void MainHubPanel::CycleNextMessage() {
    mMessageTimer.Restart();
    if (mMessageProvider->NumData() > 0) {
        mCurrentMessage++;
        mCurrentMessage %= mMessageProvider->NumData();
    }
    UpdateHeader();
}

void MainHubPanel::Exit() {
    Matchmaker *matchmaker = TheSessionMgr->GetMatchmaker();
    if (matchmaker->IsFinding()) {
        matchmaker->CancelFind();
    }
    if (mHubOverride == kMainHubOverride_Waiting && TheSessionMgr->IsLocal()) {
        SetMainHubOverride(kMainHubOverride_None);
    }
    UIPanel::Exit();
    mMachineMgr->RemoveSink(this);
    TheSessionMgr->GetMatchmaker()->RemoveSink(this);
    TheSessionMgr->RemoveSink(this);
    TheProfileMgr.RemoveSink(this, PrimaryProfileChangedMsg::Type());
#ifndef HX_NATIVE
    TheServer.RemoveSink(this, UserLoginMsg::Type());
#endif
    TheBandUI.GetOvershell()->RemoveSink(this, "override_ended");
    unkbc = 0;
    unkc0 = kScoreBand;
}

void MainHubPanel::Unload() {
    UIPanel::Unload();
    RELEASE(mMessageProvider);
}

void MainHubPanel::RefreshData() { PrepareProfilesAndMessages(); }

void MainHubPanel::ReloadMessages() {
    // Retail fn_82621AC0 opens with a guarded local-static Symbol init from
    // "messages_per_session" (lbl_820C6670 -> lbl_82E010C4, guard lbl_82E010C8)
    // BEFORE `stb r11, 0xb8`.  Nothing in the body reads it; a static with a
    // non-trivial ctor is emitted regardless, so the construction is what is
    // observable.  Shadows the extern of the same name in utl/Symbols4.h.
    static Symbol messages_per_session("messages_per_session");
    unkb8 = false;
    UpdateMessageProvider();
    LocalBandUser *user = nullptr;
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (profile) {
        user = TheBandUserMgr->GetUserFromPad(profile->GetPadNum());
    }
    if (profile) {
        if (user) {
            // Retail fn_82621AC0 has NO Server::GetPlayerID test here.  Two
            // independent proofs on retail bytes: (a) a unit-wide scan for the
            // GetPlayerID vtable slot (`lwz r11, 0x1c(...)` + bctrl) finds it
            // ONLY in CheckProfileForTicker (fn_8261FF10) and SetMainHubOverride
            // (fn_82622648), never in fn_82621AC0; (b) lbl_82C6EB50 (TheServer)
            // does not appear among fn_82621AC0's data references at all.
            // Retail goes straight from GetUserFromPad to user->GetTrackType().
            // The rb3-Wii DEV oracle gates the ticker request on a server login;
            // RB3 X360 retail does not.  (Lane W16-AN.)
            TrackType ty = user->GetTrackType();
            // Retail emits THREE explicit equality compares here -- `cmpwi 0xa`
            // / beq, `cmpwi 0xb` / beq, `cmpwi 0xc` / bne -- not the unsigned
            // range trick.  `ty - 10U <= 2` compiles to `subi 0xa` + `cmplwi 2`
            // + `bgt`, which is 3 charged instructions and 4 deletes.
            if (ty == kTrackNone || ty == kTrackPending
                || ty == kTrackPendingVocals) {
                bool randBool = RandomInt(0, 2) != 0;
                ty = ControllerTypeToTrackType(
                    user->ConnectedControllerType(), randBool
                );
            }
            ScoreType sty = TrackTypeToScoreType(
                ty,
                user->GetPreferredScoreType() == 4,
                user->GetPreferredScoreType() == 6
            );
            if (profile && (profile != unkbc || sty != unkc0)) {
                unkbc = profile;
                unkc0 = sty;
                TheRockCentral.GetTickerInfo(profile, sty, mLabelUpdateResults, this);
            }
        }
    }
}

void MainHubPanel::PrepareProfilesAndMessages() {
    ReloadMessages();
    if (mMessageProvider->NumData() != 0) {
        mCurrentMessage = 0;
        if (mMessageProvider->NumData() > 1) {
            mMessageTimer.Start();
        }
    } else {
        mCurrentMessage = 0;
        static Message refresh_message_provider("refresh_message_provider");
        HandleType(refresh_message_provider);
    }
    UpdateHeader();
}

bool MainHubPanel::CheckProfileForTicker() {
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (profile && TheServer.IsConnected()) {
        // Retail X360 (fn_8261FF10) closes this test with `cmplwi r3, 0x0` -- an
        // UNSIGNED zero test -- on the value returned by Server vtable slot 0x1c
        // (GetPlayerID).  The return type stays `int`: four other retail sites
        // test the same slot with signed `cmpwi` and are already 100% with
        // `int GetPlayerID(int)` (MetaPerformer x3, SongStatusMgr, per lane
        // W16-B), so widening the declaration would break them.  Only this call
        // site is in an unsigned context, and the cast reproduces exactly that.
        // Same shape as RockCentral.cpp:285 (lane W16-B).  (Lane W16-AN.)
        if ((unsigned int)TheServer.GetPlayerID(profile->GetPadNum()) != 0)
            return true;
    }
    return false;
}

void MainHubPanel::UpdateHeader() {
    BandLabel *label = mDir->Find<BandLabel>("message.lbl", true);
    if (mMessageProvider->NumData() != 0) {
        mMessageProvider->SetMessageLabel((AppLabel *)label, mCurrentMessage);
    } else
        label->SetTextToken(gNullStr);
    static Message msg("update_message_counter", 0, 0);
    msg[0] = mCurrentMessage + 1;
    msg[1] = mMessageProvider->NumData();
    HandleType(msg);
}

void MainHubPanel::UpdatePoolInfo() {
    static Message update_pool_info("update_pool_info", 0, 0, 0, 0, 0);
    MatchmakerPoolStats *stats = TheSessionMgr->GetMatchmaker()->mPoolStats;
    update_pool_info[0] = stats->HasCurrentStats();
    if (stats->HasCurrentStats()) {
        for (int i = 0; i < 4; i++) {
            update_pool_info[i + 1] = stats->GetSlotRating(i);
        }
    }
    HandleType(update_pool_info);
}

void MainHubPanel::SetMainHubState(MainHubState state) {
    MainHubState old = mHubState;
    mHubState = state;
    if (GetState() == kUp) {
        UpdateStateView(state, old, mHubOverride, mHubOverride);
    }
}

void MainHubPanel::SetMainHubOverride(MainHubOverride oride) {
    MainHubOverride old = mHubOverride;
    mHubOverride = oride;
    UpdateStateView(mHubState, mHubState, oride, old);
    CheckStartWaitingLock();
    if (mHubOverride == kMainHubOverride_Finding) {
        StartFinding();
    }
    Matchmaker *maker = TheSessionMgr->GetMatchmaker();
    if (mHubOverride != kMainHubOverride_Finding && maker->IsFinding()) {
        maker->CancelFind();
    }
    OvershellPanel *panel = TheBandUI.GetOvershell();
    if (old == kMainHubOverride_Finding && mHubOverride != kMainHubOverride_Finding
        && panel->mPanelOverrideFlow == kOverrideFlow_RegisterOnline) {
        TheSessionMgr->Disconnect();
        if (panel->mPanelOverrideFlow == kOverrideFlow_RegisterOnline)
            panel->EndOverrideFlow(kOverrideFlow_RegisterOnline, true);
    }
    TheSessionMgr->UpdateInvitesAllowed();
}

void MainHubPanel::StartFinding() {
    // Retail fn_82622748: guard lbl_82E010E0 bit 0x1 inits a local static Symbol
    // from "mod_auto_vocals" (lbl_8203FA74) BEFORE the IsModifierActive call;
    // bit 0x2 inits "error_find_players_with_auto_vocals" (lbl_820C6860) lazily
    // inside the else arm.  Both shadow externs of the same name.
    static Symbol mod_auto_vocals("mod_auto_vocals");
    if (!TheModifierMgr->IsModifierActive(mod_auto_vocals)) {
        Matchmaker *maker = TheSessionMgr->GetMatchmaker();
        OvershellPanel *panel = TheBandUI.GetOvershell();
        if (TheSessionMgr->IsOnlineEnabled() && !maker->IsFinding()) {
            MILO_ASSERT(mHubState == kMainHubState_Quickplay || mHubState == kMainHubState_Tour, 0x192);
            maker->FindPlayers((MatchmakerFindType)(mHubState != kMainHubState_Quickplay)
            );
        } else if (!TheSessionMgr->IsOnlineEnabled()
                   && panel->mPanelOverrideFlow != kOverrideFlow_RegisterOnline) {
            panel->BeginOverrideFlow(kOverrideFlow_RegisterOnline);
        }
    } else {
        SetMainHubOverride(kMainHubOverride_None);
        static Symbol error_find_players_with_auto_vocals(
            "error_find_players_with_auto_vocals"
        );
        TheUIEventMgr->TriggerEvent(error_find_players_with_auto_vocals, nullptr);
    }
}

DataNode MainHubPanel::OnMsg(const OvershellOverrideEndedMsg &msg) {
    if (msg.GetOverrideFlowType() == 2 && mHubOverride == kMainHubOverride_Finding) {
        if (msg.Cancelled()) {
            static Message cancel_find_override("cancel_find_override");
            HandleType(cancel_find_override);
        } else {
            MILO_ASSERT(mHubState == kMainHubState_Quickplay || mHubState == kMainHubState_Tour, 0x1B7);
            TheSessionMgr->GetMatchmaker()->FindPlayers(
                (MatchmakerFindType)(mHubState != kMainHubState_Quickplay)
            );
        }
    }
    return 1;
}

#pragma push
#pragma pool_data off
DataNode MainHubPanel::OnMsg(const SessionDisconnectedMsg &msg) {
    if (mHubOverride == kMainHubOverride_Finding) {
        if (!ThePlatformMgr.IsAnyUserSignedIntoLive()) {
            static Symbol error_message("error_message");
            static Symbol error_lost_connection("error_lost_connection");
            static Symbol error_ethernet_unplugged("error_ethernet_unplugged");
            static Message init("init", error_lost_connection);
            init[0] = ThePlatformMgr.IsEthernetCableConnected()
                ? error_lost_connection
                : error_ethernet_unplugged;
            TheUIEventMgr->TriggerEvent(error_message, init);
        }
        static Message cancel_find_override("cancel_find_override");
        HandleType(cancel_find_override);
    }
    if (mHubOverride == kMainHubOverride_Waiting) {
        MILO_ASSERT(!mWaitingStateLock->InLock(), 0x1F2);
        static Message cancel("cancel_waiting_override");
        HandleType(cancel);
    }
    return 1;
}
#pragma pop

void MainHubPanel::UpdateStateView(
    MainHubState s1, MainHubState s2, MainHubOverride o1, MainHubOverride o2
) {
    static Message updateView("update_state_view", 0, 0, 0, 0);
    updateView[0] = s1;
    updateView[1] = s2;
    updateView[2] = o1;
    updateView[3] = o2;
    HandleType(updateView);
}

DataNode MainHubPanel::OnMsg(const PrimaryProfileChangedMsg &) {
    RefreshData();
    return DataNode(kDataUnhandled, 0);
}

DataNode MainHubPanel::OnMsg(const ProcessedJoinRequestMsg &) {
    if (mHubOverride == kMainHubOverride_Finding && TheSessionMgr->IsLeaderLocal()
        && !TheSessionMgr->NumOpenSlots()) {
        AdvanceFromFinding();
    }
    return 1;
}

DataNode MainHubPanel::OnMsg(const NewRemoteMachineMsg &) {
    CheckStartWaitingLock();
    static Message update_finding_help("update_finding_help");
    HandleType(update_finding_help);
    return 1;
}

DataNode MainHubPanel::OnMsg(const RemoteMachineLeftMsg &) {
    CheckStartWaitingLock();
    static Message update_finding_help("update_finding_help");
    HandleType(update_finding_help);
    return 1;
}

DataNode MainHubPanel::OnMsg(const SessionMgrUpdatedMsg &) {
    static Message update_finding_help("update_finding_help");
    HandleType(update_finding_help);
    return 1;
}

DataNode MainHubPanel::OnMsg(const RemoteMachineUpdatedMsg &msg) {
    if (msg.GetMask() & 1) {
        CheckStartWaitingLock();
    }
    return 1;
}

DataNode MainHubPanel::OnMsg(const MatchmakerChangedMsg &) {
    UpdatePoolInfo();
    return 1;
}

DataNode MainHubPanel::OnMsg(const LockStepStartMsg &msg) {
    bool ready = IsWaitingNetUIState(mMachineMgr->GetLocalMachine()->GetNetUIState());
    MILO_ASSERT(ready == (mHubOverride == kMainHubOverride_Waiting), 0x23F);
    mWaitingStateLock->RespondToLock(ready);
    return 1;
}

DataNode MainHubPanel::OnMsg(const LockStepCompleteMsg &) { return 1; }

DataNode MainHubPanel::OnMsg(const ReleasingLockStepMsg &msg) {
    if (msg->Int(2)) {
        AdvanceAll(mMachineMgr->GetLocalMachine()->GetNetUIState());
    }
    return 1;
}

DataNode MainHubPanel::OnMsg(const RockCentralOpCompleteMsg &msg) {
    if (msg.Success() && GetState() == kUp) {
        if (!TheProfileMgr.GetPrimaryProfile())
            return 1;
        unkb8 = true;
        mLabelUpdateResults.Update(nullptr);
        if (mLabelUpdateResults.NumDataResults() > 0) {
            DataResult *res = mLabelUpdateResults.GetDataResult(0);
            DataNode node8b8, node8c0, node8c8, node8d0;
            DataNode node8d8;
            mMessageProvider->ClearData();
            res->GetDataResultValue("motd", node8d8);
            // RETAIL COMPARES THE POINTER, NOT THE STRING -- and unlike the many
            // `Symbol != ""` sites nearby (Symbol::operator==(const char*) does a
            // real strcmp, utl/Symbol.h) this one is a raw `const char*` compare.
            // Retail fn_82621EA0
            // (?OnMsg@MainHubPanel@@QAA?AVDataNode@@ABVRockCentralOpCompleteMsg@@@Z),
            // at 0x825C41AC in build/45410914/asm/MainHubPanel.s:
            //     bl   fn_8274B000              ; DataNode::Str(NULL)
            //     lis  r11, lbl_82000C55@ha
            //     addi r11, r11, lbl_82000C55@l
            //     cmplw cr6, r3, r11            ; POINTER compare
            //     beq  cr6, .L_825C41D4         ; skip AddUnlinkedMotd
            // lbl_82000C55 is `.rdata:0x82000C55 size:0x1` (symbols.txt:337) -- the
            // /GF-pooled "" -- and it is the SAME object gNullStr points at:
            // gNullStr lives at 0x82C71838 (retail Symbol::Symbol(const char*)'s
            // null path loads `lwz r11, lbl_82C71838@l(r11)`), and the initialized
            // word there in orig/45410914/band.exe reads 0x82000C55.
            //
            // So the test can only ever succeed for an interned EMPTY SYMBOL. This
            // node is a kDataString: DataResults' JSON 's' column builds it through
            // JsonString::GetValue() -> DataNode(const char *), which does
            // `mValue.array = new DataArray(c, strlen(c)+1)`, and
            // DataArray(const void *, int) heap-allocates (NodesAlloc + memcpy);
            // DataNode::Str() then returns that heap pointer. A heap pointer is
            // never 0x82000C55, so IN RETAIL THIS CONDITION IS ALWAYS TRUE and
            // AddUnlinkedMotd runs even for an empty MOTD.
            //
            // That is OBSERVABLE, not merely redundant. The consumer does re-check
            // (`IsUnlinkedMotdAvailable() { return !mUnlinkedMotd.empty(); }`), but
            // MainHubMessageProvider::ClearData() resets the three standings and
            // deliberately does NOT reset mUnlinkedMotd -- so a refresh whose motd
            // came back empty OVERWRITES the previously displayed message with ""
            // where the source plainly means to leave it alone. Genuine bug.
            //
            // Match build: keep retail's comparison verbatim, codegen must not move.
            // Native: do what the source means. Define RB3_BUGCOMPAT_MOTD_PTRCMP to
            // get retail's always-true behaviour back for an A/B against the game.
#if defined(HX_NATIVE) && !defined(RB3_BUGCOMPAT_MOTD_PTRCMP)
            const char *motd = node8d8.Str();
            if (motd != nullptr && *motd != '\0') {
#elif defined(HX_NATIVE)
            // Retail's exact test, spelled as the pointer compare it actually is
            // (gNullStr IS the "" the match build's literal resolves to), so the
            // opt-out still builds under -Werror=string-compare.
            if (node8d8.Str() != gNullStr) {
#else
            if (node8d8.Str() != "") {
#endif
                mMessageProvider->AddUnlinkedMotd(node8d8.Str());
            }
            res->GetDataResultValue("role_id", node8b8);
            res->GetDataResultValue("role_rank", node8c0);
            res->GetDataResultValue("role_is_global", node8c8);
            res->GetDataResultValue("role_is_percentile", node8d0);
            if (node8c0.Int() || node8c8.Int()) {
                mMessageProvider->AddTickerData(
                    (TickerDataType)0,
                    node8b8.Int(),
                    node8c0.Int(),
                    node8c8.Int(),
                    node8d0.Int()
                );
            }
            res->GetDataResultValue("band_id", node8b8);
            res->GetDataResultValue("band_rank", node8c0);
            res->GetDataResultValue("band_is_global", node8c8);
            res->GetDataResultValue("band_is_percentile", node8d0);
            if (node8c0.Int() || node8c8.Int()) {
                mMessageProvider->AddTickerData(
                    (TickerDataType)1,
                    node8b8.Int(),
                    node8c0.Int(),
                    node8c8.Int(),
                    node8d0.Int()
                );
            }
            res->GetDataResultValue("battle_count", node8c0);
            if (node8c0.Int()) {
                mMessageProvider->AddTickerData(
                    (TickerDataType)2, 0, node8c0.Int(), false, false
                );
            }
            // NOTE: rb3-Wii dev source has `mCurrentMessage = 0;` here, but the
            // RB3 retail X360 target emits no store to 0x48(this) at this point
            // (objdiff: one extra `stw r30, 0x48(r29)` on our side, everything
            // else equal). It cannot have been optimised away -- an opaque call
            // follows immediately -- so retail's source does not have it.
            UpdateMessageProvider();
            UpdateHeader();
        } else
            MILO_WARN("RockCentralOpCompleteMsg to MainHubPanel has empty results!");
    }
    return 1;
}

void MainHubPanel::UpdateMessageProvider() {
    LocalBandUser *user = nullptr;
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (profile) {
        user = TheBandUserMgr->GetUserFromPad(profile->GetPadNum());
    }
    static Message update_messages("update_messages", user, unkb8);
    update_messages[0] = user;
    update_messages[1] = unkb8;
    DataNode handled = HandleType(update_messages);
    mMessageProvider->SetData(handled);
    static Message refresh_message_provider("refresh_message_provider");
    HandleType(refresh_message_provider);
}

DataNode MainHubPanel::OnMsg(const UserLoginMsg &msg) {
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (profile) {
        int mypadnum = msg.GetPadNum();
        if (mypadnum == profile->GetPadNum()) {
            ReloadMessages();
        }
    }
    return 1;
}

void MainHubPanel::CheckStartWaitingLock() {
    if (TheSessionMgr->IsLeaderLocal() && !mWaitingStateLock->InLock()) {
        bool b1 = true;
        std::vector<BandMachine *> machines;
        mMachineMgr->GetMachines(machines);
        for (int i = 0; i < machines.size(); i++) {
            if (!IsWaitingNetUIState(machines[i]->GetNetUIState())) {
                b1 = false;
                break;
            }
        }
        if (b1) {
            mWaitingStateLock->StartLock();
        }
    }
}

void MainHubPanel::AdvanceAll(NetUIState state) {
    MainHubAdvanceMsg msg(state, Name());
    TheSessionMgr->SendMsgToAll(msg, kReliable);
    static Message advanceMsg("advance", 0);
    advanceMsg[0] = state;
    HandleType(advanceMsg);
}

void MainHubPanel::AdvanceFromFinding() {
    if (mHubState == kMainHubState_Quickplay) {
        AdvanceAll(kNetUI_WaitingChooseSong);
    } else if (mHubState == kMainHubState_Tour) {
        AdvanceAll(kNetUI_WaitingTour);
    } else
        MILO_FAIL("Trying to advance from finding in invalid state %i\n", mHubState);
}

void MainHubPanel::SetMotd(const char *motd) {
    mMotd = motd;
    if (GetState() == kUp) {
        PrepareProfilesAndMessages();
    }
}

const char *MainHubPanel::GetMotd() {
    // Retail fn_82620540 inits three local static Symbols at the top under one
    // shared guard word (lbl_82E01058), in this declaration order: bit 0x1
    // "message_motd", bit 0x2 "message_motd_signin", bit 0x4
    // "message_motd_noconnection".  All three precede `lwz r3, 0x90` (mMotd).
    static Symbol message_motd("message_motd");
    static Symbol message_motd_signin("message_motd_signin");
    static Symbol message_motd_noconnection("message_motd_noconnection");
    const char *motd = mMotd.c_str();
    if (strlen(motd) == 0) {
        // Retail makes exactly ONE call here (bl fn_8251BE08 ==
        // ?IsEthernetCableConnected@PlatformMgr@@) and then reads `lbz r11,
        // 0x26(r30)` inline -- that is IsConnected(), which is
        // `{ return mConnected; }` in the header.  There is NO
        // IsOnlineRestricted() call: it is declared out-of-line
        // (os/PlatformMgr.h:252) so it could not have been inlined away.  Our
        // `|| ThePlatformMgr.IsOnlineRestricted()` was a genuine extra test.
        if (!ThePlatformMgr.IsEthernetCableConnected()) {
            return Localize(message_motd_noconnection, nullptr);
        } else if (!ThePlatformMgr.IsConnected()) {
            return Localize(message_motd_signin, nullptr);
        } else {
            return Localize(message_motd, nullptr);
        }
    }
    return motd;
}

void MainHubPanel::SetDLCMotd(const char *motd) {
    unk94 = motd;
    if (GetState() == kUp) {
        PrepareProfilesAndMessages();
    }
}

// Retail has NO `message_latest_dlc` string anywhere in .rdata (whereas
// GetMotd's `message_motd_signin` fallback IS present), and the get_dlcmotd
// handler inlines to a bare `lwz r4, unk94.mStr` -- so retail's GetDLCMotd is a
// trivial accessor with no empty-string Localize fallback. (The rb3-Wii DEV
// build added that fallback.)
const char *MainHubPanel::GetDLCMotd() { return unk94.c_str(); }

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(MainHubPanel)
    HANDLE_EXPR(get_message_provider, GetMessageProvider())
    HANDLE_ACTION(cycle_next_message, CycleNextMessage())
    HANDLE_EXPR(get_state, GetMainHubState())
    HANDLE_ACTION(set_state, SetMainHubState((MainHubState)_msg->Int(2)))
    HANDLE_EXPR(get_override, GetMainHubOverride())
    HANDLE_ACTION(set_override, SetMainHubOverride((MainHubOverride)_msg->Int(2)))
    HANDLE_EXPR(in_waiting_lock, mWaitingStateLock->InLock())
    HANDLE_ACTION(set_motd, SetMotd(_msg->Str(2)))
    HANDLE_EXPR(get_motd, GetMotd())
    HANDLE_ACTION(set_dlcmotd, SetDLCMotd(_msg->Str(2)))
    HANDLE_EXPR(get_dlcmotd, GetDLCMotd())
    HANDLE_ACTION(advance_from_finding, AdvanceFromFinding())
    HANDLE_EXPR(check_profile_for_message_ticker, CheckProfileForTicker())
    HANDLE_EXPR(has_role_info, mMessageProvider->IsTickerDataValid((TickerDataType)0))
    HANDLE_EXPR(has_band_info, mMessageProvider->IsTickerDataValid((TickerDataType)1))
    HANDLE_EXPR(has_battles_info, mMessageProvider->IsTickerDataValid((TickerDataType)2))
    HANDLE_EXPR(has_unlinked_motd, mMessageProvider->IsUnlinkedMotdAvailable())
    HANDLE_MESSAGE(ProcessedJoinRequestMsg)
    HANDLE_MESSAGE(SessionDisconnectedMsg)
    HANDLE_MESSAGE(PrimaryProfileChangedMsg)
    HANDLE_MESSAGE(SessionMgrUpdatedMsg)
    HANDLE_MESSAGE(NewRemoteMachineMsg)
    HANDLE_MESSAGE(RemoteMachineLeftMsg)
    HANDLE_MESSAGE(RemoteMachineUpdatedMsg)
    HANDLE_MESSAGE(LockStepStartMsg)
    HANDLE_MESSAGE(LockStepCompleteMsg)
    HANDLE_MESSAGE(ReleasingLockStepMsg)
    HANDLE_MESSAGE(OvershellOverrideEndedMsg)
    HANDLE_MESSAGE(MatchmakerChangedMsg)
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_MESSAGE(UserLoginMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x35D)
END_HANDLERS
#pragma pop
