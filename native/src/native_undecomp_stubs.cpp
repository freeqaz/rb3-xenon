// Native link stubs for engine virtuals that are declared + referenced by
// compiled vtables/typeinfo but whose bodies are NOT yet decompiled in-tree.
//
// The X360 matching build never fully links (objdiff compares objects), so an
// undefined vtable slot is harmless there. The native build DOES link, so every
// virtual referenced by a compiled vtable needs a body. These classes are off
// the DTA-parse path; empty/neutral bodies suffice.
//
// Added 2026-07-24 (native-adv): PlatformMgr's 07-18 `: public MsgSource`
// re-anchor and User's UnkTU5Virtual_beforeUserName addition left these
// undefined at native link time.

#include "obj/Msg.h"
#include "os/User.h"
#include "utl/SongInfoCopy.h"

// ---- SongInfoCopy::GetTracks (real one-line accessor, from char/CharBoneDir.cpp:17).
// Replaces the weak no-op stub in dta_link_stubs.s. Not on the DTA parse path
// today, but the real body is trivial and self-contained, so prefer it.
//
// WEAK because X2's rb3-milo compiles char/CharBoneDir.cpp, whose line 17 IS the
// real definition. `weak` lets the strong one win there while every other target
// (which does not compile char/) keeps this copy -- strictly better than an
// #ifdef on a per-target macro, since it needs no coordination when the next
// target widens its glob. Same reasoning for CacheWav / CacheResource below. ----
__attribute__((weak))
const std::vector<TrackChannels> &SongInfoCopy::GetTracks() const { return mTrackChannels; }

// ---- MsgSource (obj/Msg.h) — real bodies now live in src/system/obj/Msg.cpp
// (ported from the rb3-Wii oracle, 2026-07-24). obj/*.cpp is globbed into the
// native build, so Msg.cpp supplies MsgSource's ctor/dtor/Handle/SyncProperty/
// Export/AddSink/RemoveSink/etc. The former no-op shim here is removed: the
// broadcast now actually reaches sinks. ----

// ---- User (os/User.cpp compiled, but this TU5-discovered virtual has no body) ----
// Renamed UnkTU5Virtual_beforeUserName -> IsNullUser in cb926469 (os/User.h:51).
bool User::IsNullUser() const { return false; }

// ---- CacheWav (synth/Utl.cpp) ------------------------------------------------
// os/FileCache.cpp:384 calls CacheWav for .wav entries, but src/system/synth/ is
// not in the native build (its TUs pull tomcrypt's broken <angled> sibling
// includes and an `environ` clash), so the real definition never links.
//
// This is NOT a no-op stub: it is CacheWav's own kPlatformPC early-return
// (synth/Utl.cpp:27-29), which is the semantically correct branch for a native
// host. The other branches rewrite the path to `<dir>/gen/<base>.<ext>_<platform>`
// and then ask a Holmes ASSET SERVER to cook it — console-development
// machinery with no native counterpart. On PC the engine plays the loose .wav
// as-is, which is exactly what returning `file` unchanged does.
//
// ⚠ If synth/Utl.cpp is ever added to the native build, DELETE this — it would
// become a duplicate definition.
#include "synth/Utl.h"
__attribute__((weak))
const char *CacheWav(const char *file, CacheResourceResult &result) {
    result = (CacheResourceResult)0;
    return file;
}

// ---- CacheResource(const char*, const Hmx::Object*) (rndobj/Utl.cpp:1165) -----
// Same situation as CacheWav above, one directory over: os/FileCache.cpp:381
// calls it for .png/.bmp entries, but src/system/rndobj/ is not in the native
// build (only rndobj/Anim.cpp is — see RNDOBJ_SOURCES in native/CMakeLists.txt).
//
// The real body forwards to CacheResource(cc, CacheResourceResult&)
// (rndobj/Utl.cpp:1204), whose kCacheUnnecessary path returns the localized path
// unchanged, and then only diverges to ask a Holmes asset server to cook the
// image. Native hosts read loose assets, so `res == kCacheUnnecessary` is the
// live branch and returning `cc` reproduces it. The nullptr-on-empty guard is
// the real function's own (Utl.cpp:1166).
//
// ⚠ Delete this when rndobj/Utl.cpp joins the native build (X2 widens the fork
// glob to rndobj/) — it would become a duplicate definition.
namespace Hmx {
    class Object;
}
__attribute__((weak))
const char *CacheResource(const char *cc, const Hmx::Object *) {
    if (!cc || *cc == '\0')
        return nullptr;
    return cc;
}

// ===========================================================================
// X7 — the 13 functions BandCharacter.cpp CALLS that this tree never DEFINES
// ===========================================================================
//
// ⚠ READ THIS BEFORE TRUSTING ANY FRAME THAT CONTAINS A BAND MEMBER.
//
// Each of these is DECLARED in a header and has NO BODY ANYWHERE in
// rb3-xenon's src/ — verified per symbol with `grep -rn '::<name>' src/`, not
// assumed. They are ordinary decomp gaps that only surface here because the
// native build is the only one that links. Off X360 nothing changes: the
// matching build compiles objects and never resolves them.
//
// ★ WHAT IS AND IS NOT SUBSTITUTED, PRECISELY.
//
//   NOT substituted — PLACEMENT. Not one of these is on the band-placement
//   path. A band member's stage transform comes from the venue's
//   BandConfiguration (baked in the .milo, measured: 12 named slot-rows at 12
//   distinct positions in small_club_01) through
//   BandConfiguration::SyncPlayMode -> BandCharacter::Teleport ->
//   Character::Teleport (char/Character.cpp:486), and every one of those has a
//   real body. No stub below can move a band member one unit.
//
//   IS substituted — DEFORMATION AND SKIN REFINEMENT. CharCollide::Deform,
//   CharCuff::Deform, RndMeshDeform::Reskin, CharBoneOffset::ApplyToLocal and
//   CharMeshHide::HideAll are the second-order mesh passes that run AFTER the
//   pose is computed: collision squash, cuff/sleeve fitting, deform reskin,
//   per-bone offsets, and hiding the body parts an outfit covers. With them
//   inert a member is posed and animated by the real skeleton but is not
//   refined — expect interpenetration at joints and body geometry visible
//   through clothing. This is the same CLASS of disclosure as X6's crowd draw:
//   a mechanism substitution, never a placement one.
//
// This is the honest boundary of what a frame from this lane proves. It is
// recorded here rather than only in the plan doc so it cannot be read out of
// the code.
//
// Every body below is neutral (no-op / identity / "nothing found"), never a
// plausible-looking guess at the real behaviour — a wrong body that looked
// right would be strictly worse than an inert one, because it would be
// invisible in a screenshot.

#include "char/CharBoneOffset.h"
#include "char/CharClip.h"
#include "char/CharCollide.h"
#include "char/CharCuff.h"
#include "char/CharMeshHide.h"
#include "char/Character.h"
#include "math/Mtx.h"
#include "rndobj/MeshDeform.h"
#include "rndobj/Rnd.h"

// --- Character (char/Character.h:153-154). Declared, never defined.
// RepointSphereBase re-anchors the bounding sphere onto a merged dir; with it
// inert the sphere keeps whatever base it loaded with, which affects CULLING
// and LOD selection, not position.
void Character::RepointSphereBase(ObjectDir *) {}
// RemoveFromPoll drops a pollable from the character's poll list. Inert means
// a removed pollable keeps polling — a leak of work, not of correctness, for a
// single-frame render.
void Character::RemoveFromPoll(RndPollable *) {}

// --- CharClip (char/CharClip.h:186-187). Declared, never defined. Note the
// tree DOES define the differently-named CharClip::InGroups() (CharClip.cpp:1021);
// these two are not it.
// InGroup answers "is this clip in that group"; false = "no group claims it",
// which routes clip selection to its default arm rather than to a wrong group.
bool CharClip::InGroup(Hmx::Object *) { return false; }
// MakeMRU promotes a clip in the LRU cache. Inert = no reordering; eviction
// order changes, contents do not.
void CharClip::MakeMRU() {}

// --- Deformation passes. All declared, never defined. See the disclosure above.
void CharCollide::Deform() {}
void CharCuff::Deform(SyncMeshCB *, FileMerger *) {}
void CharBoneOffset::ApplyToLocal() {}
void RndMeshDeform::Reskin(SyncMeshCB *, bool) {}
void CharMeshHide::HideAll(const ObjPtrList<CharMeshHide, ObjectDir> &, int) {}

// --- MakeVertical(Hmx::Matrix3&) — declared in math/Mtx.h, never defined.
// Orthonormalizes a basis about world-up. Inert leaves the caller's matrix
// exactly as passed, which is the identity transformation ON THE MATRIX — the
// one case where "do nothing" is genuinely the neutral answer rather than an
// approximation of one.
void MakeVertical(Hmx::Matrix3 &) {}

// --- Rnd::CompressTextureCancel — declared, never defined. It withdraws a
// pending async texture-compression request. This backend performs no async
// compression, so there is never a request to withdraw and inert is exact.
void Rnd::CompressTextureCancel(Rnd::CompressTextureCallback *) {}

// --- X7 continued: four more declared-never-defined symbols, same rules.

#include "bandobj/BandCharDesc.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandPatchMesh.h"
#include "meta/FixedSizeSaveable.h"
#include "meta/FixedSizeSaveableStream.h"

// BandCharacter.h:206 re-declares `static Symbol NameToDrumVenue(const char*)`
// which BandCharDesc ALREADY declares and DEFINES (BandCharDesc.cpp:575). The
// redeclaration hides the inherited one, so the call binds to a symbol with no
// body. Forwarding to the base is not a guess: identical signature, identical
// semantics, and BandCharacter IS-A BandCharDesc. Deleting the redundant
// declaration from the header would also work but would change name lookup in
// the X360 arm, which this lane does not touch.
Symbol BandCharacter::NameToDrumVenue(const char *name) {
    return BandCharDesc::NameToDrumVenue(name);
}

// BandPatchMesh::ConstructQuad builds the unit quad the patch mesh renders
// into. Declared, never defined. Inert leaves the mesh with no geometry, so a
// patch quad DRAWS NOTHING rather than drawing something wrong — which is the
// correct failure direction for an evidence frame.
void BandPatchMesh::ConstructQuad(RndTex *) {}

// FixedSizeSaveable::{Save,Load}FixedString (meta/FixedSizeSaveable.h:40-41).
// Declared, never defined. These are the SAVEGAME serializer, reached from
// BandCharDesc::OutfitPiece's fixed-size save/load (BandCharDesc.cpp:335, :353)
// — NOT from BandCharDesc::Load(BinStream&), which is the .milo path this lane
// exercises. Load clears the string rather than leaving it undefined, so a
// caller sees "empty" and not stale memory.
void FixedSizeSaveable::SaveFixedString(FixedSizeSaveableStream &, const String &) {}
void FixedSizeSaveable::LoadFixedString(FixedSizeSaveableStream &, String &s) { s = ""; }

// --- CharKeyHandMidi: an ENTIRELY UNDECOMPILED CLASS.
//
// bandobj/CharKeyHandMidi.h exists; there is NO CharKeyHandMidi.cpp anywhere in
// rb3-xenon, so not one of its 11 virtuals has a body and the compiler
// therefore emits no vtable and no typeinfo. BandCharacter.cpp:1466
// `dynamic_cast<CharKeyHandMidi *>(o)` needs the typeinfo, which is the single
// remaining undefined symbol in the band-member link.
//
// ⚠ This class is the KEYBOARD PLAYER'S HAND-POSITION MIDI DRIVER. Stubbing it
// means a keyboard band member's hands are not driven to the notes being
// played. Off the placement path entirely (it moves fingers, not people).
//
// Defining the virtuals is what emits the vtable and hence the typeinfo. The
// dynamic_cast can never actually succeed here in any case: nothing registers a
// CharKeyHandMidi factory and nothing news one, so no object of this type can
// exist and the cast always yields null -- which is exactly what the caller's
// null-check arm expects.
//
// ★ rb3-Wii HAS the real body at
//   /home/free/code/milohax/rb3/src/system/bandobj/CharKeyHandMidi.cpp.
// Porting it is a self-contained follow-up, listed in the X7 handoff.
#include "bandobj/CharKeyHandMidi.h"
CharKeyHandMidi::~CharKeyHandMidi() {}
void CharKeyHandMidi::Highlight() {}
DataNode CharKeyHandMidi::Handle(DataArray *, bool) { return DataNode(kDataUnhandled, 0); }
bool CharKeyHandMidi::SyncProperty(DataNode &, DataArray *, int, PropOp) { return false; }
void CharKeyHandMidi::Save(BinStream &) {}
void CharKeyHandMidi::Copy(const Hmx::Object *, Hmx::Object::CopyType) {}
void CharKeyHandMidi::Load(BinStream &) {}
void CharKeyHandMidi::Poll() {}
void CharKeyHandMidi::PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &) {}
void CharKeyHandMidi::Enter() {}
void CharKeyHandMidi::SetName(const char *, ObjectDir *) {}
