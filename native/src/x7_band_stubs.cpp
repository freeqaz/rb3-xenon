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
//   CharCuff::Deform, CharBoneOffset::ApplyToLocal and
//   CharMeshHide::HideAll are the second-order mesh passes that run AFTER the
//   pose is computed: collision squash, cuff/sleeve fitting,
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

// --- Character. (RepointSphereBase and RemoveFromPoll used to be inert stubs
// here; their real bodies are in char/Character.cpp, from retail 0x8236F1D0
// (lane W16-LA) and 0x823710B8 (lane W16-PC).)

// (CharClip::InGroup and CharClip::MakeMRU, and the four deformation passes
// CharCollide::Deform, CharCuff::Deform, CharBoneOffset::ApplyToLocal and
// CharMeshHide::HideAll, used to be inert stubs here; lane W16-LA wrote the real
// bodies from retail 0x8237E0B8, 0x8237E118, 0x8239AD88, 0x8239F1A0, 0x823A45E0
// and 0x823A0D60, so the disclosure above no longer applies to them.)

// (RndMeshDeform::Reskin used to be an inert stub here; lane W16-JA wrote the
// real body in rndobj/MeshDeform.cpp, so it is no longer substituted.)
// (MakeVertical(Hmx::Matrix3&) used to be an inert stub here; lane W16-HZ wrote the real
// body in src/system/math/Rot.cpp from retail 0x824EE5F0, so that links instead.)

// (Rnd::CompressTextureCancel used to be an inert stub here; lane W16-KB wrote the
// real body in rndobj/Rnd.cpp from retail 0x82412A58, so that links instead.)

// --- X7 continued: four more declared-never-defined symbols, same rules.

#include "bandobj/BandCharDesc.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandPatchMesh.h"
#include "meta/FixedSizeSaveable.h"
#include "meta/FixedSizeSaveableStream.h"

// (BandPatchMesh::ConstructQuad used to be an inert stub here; W17-BPM2 ported
// the patch-projection subsystem, so the real member in
// src/system/bandobj/BandPatchMesh.cpp links instead.)

// (FixedSizeSaveable::{Save,Load}FixedString are defined in
// src/system/meta/FixedSizeSaveable.cpp, retail 0x827A2C28/0x827A2A50, and link from there.)

// (CharKeyHandMidi: the real TU, src/system/bandobj/CharKeyHandMidi.cpp, retail
// 0x822CF888-0x822D2BA8, is compiled from native/CMakeLists.txt -- lane W16-IF.)
