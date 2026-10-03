// Quazal NetZ - .\Competition\Protocol\TournamentDDL.cpp
//
// The DDL-generated half of Tournament, a Competition with no members of its
// own: the static Add/Extract stream helpers and the six virtuals that sit in
// Tournament's vtable (0x8217E5C4, emitted by CompetitionClient.cpp).
//
// The retail TU is 0x82AE10A0..0x82AE1210: Clone, GetGatheringType, IsA and
// IsAKindOf. It sits between two out-of-line constructors that belong to their
// own TUs: Competition::Competition at 0x82AE0820 and Gathering::Gathering at
// 0x82AE1210. Both are called from CompetitionClient/MatchMakingClient code at
// 0x82A8xxxx, so the linker would have placed them there had they been
// COMDATs. TournamentDDL's .rdata is the file string and "Tournament"
// (0x821847C0..0x821847F8).
//
// Add, Extract, StreamIn and StreamOut are byte-identical to RankingDDL's (both
// classes are member-less Competitions), so the linker folded them and
// Tournament's vtable points at RankingDDL's copies (0x82AE15A8, 0x82AE15D8,
// which call 0x82AE13D8/0x82AE1408). They are still defined here, in source
// order.
//
// Built /Od /Oi- /EHs-c- /Ob1 /GR-, like the other Quazal TUs: no EH records
// and none of these classes' vtables carries an RTTI locator.
//
// Tournament derives from _DDL_Tournament, which derives from Competition: the
// inline constructors store the _DDL_Tournament and Tournament vptrs, and the
// linker folded those two identical vtables, which is why Clone stores the
// same address twice. The classes are declared here only as far as this file
// uses them.

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

    class _DDL_Competition : public Gathering {
    public:
        _DDL_Competition();
        virtual ~_DDL_Competition();
        virtual Gathering *Clone() const;
        virtual String GetGatheringType() const;
        virtual bool IsA(const String &) const;
        virtual bool IsAKindOf(const String &) const;
        virtual void StreamIn(Message *) const;
        virtual void StreamOut(Message *);

        static void Add(Message *, const _DDL_Competition &);
        static void Extract(Message *, _DDL_Competition *);

        String m_str28; // 0x28
        unsigned int m_list[2]; // 0x2c, a qList
    };

    class Competition : public _DDL_Competition {
    public:
        Competition();
        virtual ~Competition();
    };

    class _DDL_Tournament : public Competition {
    public:
        _DDL_Tournament() {}
        virtual ~_DDL_Tournament() {}
        virtual Gathering *Clone() const;
        virtual String GetGatheringType() const;
        virtual bool IsA(const String &) const;
        virtual bool IsAKindOf(const String &) const;
        virtual void StreamIn(Message *) const;
        virtual void StreamOut(Message *);

        static void Add(Message *, const _DDL_Tournament &);
        static void Extract(Message *, _DDL_Tournament *);
    };

    class Tournament : public _DDL_Tournament {
    public:
        Tournament() {}
        virtual ~Tournament() {}
    };

    void _DDL_Tournament::Add(Message *msg, const _DDL_Tournament &t) { _DDL_Competition::Add(msg, t); }

    void _DDL_Tournament::Extract(Message *msg, _DDL_Tournament *t) { _DDL_Competition::Extract(msg, t); }

    Gathering *_DDL_Tournament::Clone() const { return new (__FILE__, 27) Tournament; }

    String _DDL_Tournament::GetGatheringType() const { return String("Tournament"); }

    bool _DDL_Tournament::IsA(const String &s) const { return String::IsEqual(s.m_szContent, "Tournament"); }

    bool _DDL_Tournament::IsAKindOf(const String &s) const {
        return String::IsEqual(s.m_szContent, "Tournament") || _DDL_Competition::IsAKindOf(s);
    }

    void _DDL_Tournament::StreamIn(Message *msg) const { Add(msg, *this); }

    void _DDL_Tournament::StreamOut(Message *msg) { Extract(msg, this); }

}
