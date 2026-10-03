// Quazal NetZ - .\MatchMaking\Protocol\GameSessionDDL.cpp
//
// The DDL-generated half of GameSession, a Gathering with no members of its
// own: the static Add/Extract stream helpers and the six virtuals that sit in
// GameSession's vtable (0x8217E6FC, emitted by MatchMakingClient.cpp).
//
// The retail TU is 0x82AE5B50..0x82AE5D80. It starts at the static Add, the
// only caller of which is StreamIn below, and ends after StreamOut: the two
// functions at 0x82AE5D80 and 0x82AE5DC0 are called only from
// DynamicGatheringDDL's StreamIn/StreamOut and stream a member at +0x28, so
// they are that file's Add/Extract. GameSessionDDL's .rdata is the file string
// and "GameSession" (0x821848A8..0x821848E0).
//
// Built /Od /Oi- /EHs-c- /Ob1 /GR-, like the other Quazal TUs: no EH records
// and none of these classes' vtables carries an RTTI locator.
//
// GameSession derives from _DDL_GameSession, which derives from Gathering: the
// inline constructors store the _DDL_GameSession and GameSession vptrs, and
// the linker folded those two identical vtables, which is why Clone stores the
// same address twice. Gathering's constructor is out of line (0x82AE1210, in
// Gathering.cpp). The classes are declared here only as far as this file uses
// them.

#include "Platform/RootObject.h"
#include "Platform/String.h"

namespace Quazal {

    class Message;

    class _DDL_Gathering : public RootObject {
    public:
        _DDL_Gathering() {}
        virtual ~_DDL_Gathering() {}
        virtual class Gathering *Clone() const;
        virtual String GetGatheringType() const;
        virtual bool IsA(const String &) const;
        virtual bool IsAKindOf(const String &) const;
        virtual void StreamIn(Message *) const;
        virtual void StreamOut(Message *);

        static void Add(Message *, const _DDL_Gathering &);
        static void Extract(Message *, _DDL_Gathering *);

        unsigned int m_idMyself; // 0x4
        unsigned int m_pidOwner; // 0x8
        unsigned int m_pidHost; // 0xc
        unsigned short m_uiMinParticipants; // 0x10
        unsigned short m_uiMaxParticipants; // 0x12
        unsigned int m_uiParticipationPolicy; // 0x14
        unsigned int m_uiPolicyArgument; // 0x18
        unsigned int m_uiFlags; // 0x1c
        unsigned int m_uiState; // 0x20
        String m_strDescription; // 0x24
    };

    class Gathering : public _DDL_Gathering {
    public:
        Gathering();
        virtual ~Gathering();
        virtual void Trace(unsigned int) const;
    };

    class _DDL_GameSession : public Gathering {
    public:
        _DDL_GameSession() {}
        virtual ~_DDL_GameSession() {}
        virtual Gathering *Clone() const;
        virtual String GetGatheringType() const;
        virtual bool IsA(const String &) const;
        virtual bool IsAKindOf(const String &) const;
        virtual void StreamIn(Message *) const;
        virtual void StreamOut(Message *);

        static void Add(Message *, const _DDL_GameSession &);
        static void Extract(Message *, _DDL_GameSession *);
    };

    class GameSession : public _DDL_GameSession {
    public:
        GameSession() {}
        virtual ~GameSession() {}
    };

    void _DDL_GameSession::Add(Message *msg, const _DDL_GameSession &gs) { _DDL_Gathering::Add(msg, gs); }

    void _DDL_GameSession::Extract(Message *msg, _DDL_GameSession *gs) { _DDL_Gathering::Extract(msg, gs); }

    Gathering *_DDL_GameSession::Clone() const { return new (__FILE__, 27) GameSession; }

    String _DDL_GameSession::GetGatheringType() const { return String("GameSession"); }

    bool _DDL_GameSession::IsA(const String &s) const { return String::IsEqual(s.m_szContent, "GameSession"); }

    bool _DDL_GameSession::IsAKindOf(const String &s) const {
        return String::IsEqual(s.m_szContent, "GameSession") || _DDL_Gathering::IsAKindOf(s);
    }

    void _DDL_GameSession::StreamIn(Message *msg) const { Add(msg, *this); }

    void _DDL_GameSession::StreamOut(Message *msg) { Extract(msg, this); }

}
