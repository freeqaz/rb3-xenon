// x7_band_stubs.cpp -- the undecompiled bodies the BAND-MEMBER surface needs.
//
// ⛔ WHY THIS IS ITS OWN TU AND NOT native_undecomp_stubs.cpp.
//
// It was in native_undecomp_stubs.cpp first, and that BROKE THE NATIVE GATE:
// 8 of 18 targets failed to link (rb3-dta / -song / -midi / -gem / -hit /
// -score / -save / -ark). native_undecomp_stubs.cpp is in NATIVE_SHIMS, which
// EVERY target links, and those eight compile neither src/system/char/ nor the
// bandobj TUs -- so defining Character::/CharClip::/CharKeyHandMidi:: bodies
// there drags in `typeinfo for Character`, `typeinfo for CharWeightable`,
// `typeinfo for RndTransformable`, the whole RndPollable/CharWeightable virtual
// set and BandCharDesc::NameToDrumVenue, none of which those targets have.
//
// ★ The lesson is the file-placement one: a stub's blast radius is the SOURCE
// LIST it sits in, not the target you were thinking about. This TU is listed
// only in MILO_TARGET_COMMON_SOURCES (rb3-milo + rb3-render), which is exactly
// the set that compiles the classes these bodies mention.
//
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
