// Ported from rb3-Wii src/system/bandobj/OutfitConfig.cpp (MWCC -> MSVC X360).
#include "bandobj/OutfitConfig.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandHeadShaper.h"
#include "char/CharClip.h"
#include "char/CharMeshCacheMgr.h"
#include "decomp.h"
#include "math/Color32.h"
#include "math/Rand.h"
#include "obj/ObjMacros.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "rndobj/Rnd.h"
#include "rndobj/Utl.h"
#include "utl/Loader.h"
#include "utl/Symbols.h"
#ifdef HX_NATIVE
#include <cstdlib> // getenv (RB3_SKIN_FIX_OFF opt-out)
#include <cstring> // strstr
#endif

INIT_REVS(OutfitConfig);

RndMat *OutfitConfig::sMat;
RndCam *OutfitConfig::sCam;
BandCharDesc *OutfitConfig::sBandCharDesc;
OutfitConfig *gOutfitConfigOwner;

OutfitConfig::MatSwap::MatSwap(Hmx::Object *o)
    : mMat(o, 0), mResourceMat(o, 0), mTwoColorDiffuse(o, 0), mTwoColorInterp(o, 0),
      mTwoColorMask(o, 0), mColor1Palette(o, 0), mColor1Option(0), mColor2Palette(o, 0),
      mColor2Option(0), mTextures(o), mTwoColor(0) {}

void OutfitConfig::MatSwap::SyncTwoColor() {
    mTwoColor = mTwoColorDiffuse || mTwoColorInterp || mTwoColorMask;
}

bool OutfitConfig::MatSwap::MatchesPatchCategory(int i, ObjVector<BandPatchMesh> &vec) {
    if (!mMat)
        return false;
    else if (i == -1)
        return true;
    else {
        for (ObjVector<BandPatchMesh>::iterator it = vec.begin(); it != vec.end(); ++it) {
            if (i & (*it).mCategory) {
                for (ObjVector<BandPatchMesh::MeshPair>::iterator mp =
                         (*it).mMeshes.begin();
                     mp != (*it).mMeshes.end();
                     ++mp) {
                    MILO_ASSERT(mp->mesh, 0x6B);
                    if (mp->mesh->Mat() == mMat)
                        return true;
                }
            }
        }
        return false;
    }
}

void OutfitConfig::MatSwap::SwapResource() {
    if (mResourceMat) {
        static Symbol mn("Mesh");
        MemDoTempAllocations m;
        const ObjRef &refs = mResourceMat->Refs();
        for (ObjRef::iterator rit = refs.begin(); rit != refs.end();) {
            ObjRef *cur = rit;
            ++rit;
            bool replace = false;
            if (RefPtrOf(cur)->RefOwner()) {
                if (RefPtrOf(cur)->RefOwner()->ClassName() == mn)
                    replace = true;
            }
            if (replace)
                // ObjRef::Replace(Hmx::Object*) is an elided stub off HX_NATIVE.
                RefPtrOf(cur)->Replace(
                    reinterpret_cast<ObjRef *>((RndMat *)mResourceMat), mMat
                );
        }
    }
}

void OutfitConfig::MatSwap::UnSwapResource() {
    if (mResourceMat && mMat) {
        static Symbol mn("Mesh");
        MemDoTempAllocations m;
        const ObjRef &refs = mMat->Refs();
        for (ObjRef::iterator rit = refs.begin(); rit != refs.end();) {
            ObjRef *cur = rit;
            ++rit;
            bool replace = false;
            if (RefPtrOf(cur)->RefOwner()) {
                if (RefPtrOf(cur)->RefOwner()->ClassName() == mn)
                    replace = true;
            }
            if (replace)
                // ObjRef::Replace(Hmx::Object*) is an elided stub off HX_NATIVE.
                RefPtrOf(cur)->Replace(
                    reinterpret_cast<ObjRef *>((RndMat *)mMat), mResourceMat
                );
        }
    }
}

void OutfitConfig::MatSwap::Compose(
    int *colors, ObjVector<BandPatchMesh> &patches, int category
) {
    if (!MatchesPatchCategory(category, patches))
        return;
    RndTex *diffTex = mMat->GetDiffuseTex();
#ifdef HX_NATIVE
    // Native two-color composite fix (c8-eyes, 2026-07-02): scope a flag while
    // this outfit composite paints its *_diffuse_output render target so the
    // native BandRnd::DrawRect can combine the modulate layers with a
    // DEST-MULTIPLY blend (product) instead of REPLACE. Without this the RT
    // collapses to the last (mask) layer -> untextured near-white eyeballs that
    // glow under warm venue light. Mirrors the Wii's WiiTex::
    // bComposingOutfitTexture scoping below. Wii match build is untouched.
    extern bool gRB3OutfitComposeActive;
    struct ComposeScope {
        ComposeScope() { gRB3OutfitComposeActive = true; }
        ~ComposeScope() { gRB3OutfitComposeActive = false; }
    } composeScope;
#endif
    if (!diffTex || (diffTex->GetType() & RndTex::kRenderedNoZ) != RndTex::kRenderedNoZ) {
        if (mTwoColor)
            return;
        if (!mTextures.empty()) {
            int idx = colors[mColor1Option] % mTextures.size();
            mMat->SetDiffuseTex(mTextures[idx]);
        } else if (mColor1Palette) {
            const Hmx::Color &c = mColor1Palette->GetColor(colors[mColor1Option]);
            mMat->SetColor(c.red, c.green, c.blue);
        }
    } else {
        RndCam *prevCam = RndCam::Current();
        RndTex *prevTarget = prevCam->TargetTex();
        if (prevTarget) {
            MILO_NOTIFY_ONCE(
                "%s: Cannot render to texture (%s) while already rendering to texture "
                "(%s).",
                PathName(prevTarget),
                PathName(diffTex),
                PathName(prevTarget)
            );
        }
        RndCam *defaultCam = TheRnd.GetDefaultCam();
        sCam->Copy(defaultCam, Hmx::Object::kCopyDeep);
        sCam->SetTargetTex(diffTex);
        sCam->SetTransParent(nullptr, false);
        {
            Transform &lx = sCam->DirtyLocalXfm();
            lx.v.Set(0.0f, 0.0f, 1.0f);
            lx.m.x.Set(1.0f, 0.0f, 0.0f);
            lx.m.y.Set(0.0f, 0.0f, -1.0f);
            lx.m.z.Set(0.0f, -1.0f, 0.0f);
        }
        sCam->SetFrustum(0.01f, 5.0f, 0.0f, 1.0f);
        sCam->Select();
        Hmx::Color baseColor(1.0f, 1.0f, 1.0f, 1.0f);
        sMat->SetColorModFlags(RndMat::kColorModNone);
        sMat->SetBlend(RndMat::kBlendSrc);
        sMat->SetTexWrap(kTexWrapClamp);
        sMat->SetDiffuseTex(nullptr);
        sMat->SetAlpha(1.0f);
        {
            const Hmx::Color *col = &baseColor;
            if (mColor1Palette) {
                col = &mColor1Palette->GetColor(colors[mColor1Option]);
            }
            sMat->SetColor(col->red, col->green, col->blue);
        }
        mMat->SetColor(baseColor.red, baseColor.green, baseColor.blue);
        Hmx::Rect rect(0.0f, 0.0f, (float)TheRnd.Width(), (float)TheRnd.Height());
        sMat->SetAlphaCut(false);
        TheRnd.DrawRect(rect, baseColor, sMat, nullptr, nullptr);
        if (mTwoColorDiffuse) {
            sMat->SetColorModFlags(RndMat::kColorModModulate);
            sMat->SetDiffuseTex(mTwoColorDiffuse);
            const Hmx::Color *col = &baseColor;
            if (mColor2Palette) {
                col = &mColor2Palette->GetColor(colors[mColor2Option]);
            }
            sMat->SetColor(col->red, col->green, col->blue);
            TheRnd.DrawRect(rect, baseColor, sMat, nullptr, nullptr);
        }
        if (mTwoColorInterp) {
            sMat->SetColorModFlags(RndMat::kColorModModulate);
            sMat->SetDiffuseTex(mTwoColorInterp);
            sMat->SetColor(baseColor.red, baseColor.green, baseColor.blue);
            TheRnd.DrawRect(rect, baseColor, sMat, nullptr, nullptr);
        }
        if (mTwoColorMask) {
            sMat->SetColorModFlags(RndMat::kColorModAlphaUnpackModulate);
            sMat->SetDiffuseTex(mTwoColorMask);
            sMat->SetColor(baseColor.red, baseColor.green, baseColor.blue);
            TheRnd.DrawRect(rect, baseColor, sMat, nullptr, nullptr);
        }
        sMat->SetAlphaCut(false);
        sMat->SetCull(kCullNone);
        for (int i = 0; i < patches.size(); i++) {
            patches[i].Render(diffTex, sMat);
        }
        sCam->SetTargetTex(nullptr);
        prevCam->Select();
        sMat->SetDiffuseTex(nullptr);
    }
}

bool OutfitConfig::MatSwap::Compress(BandCharDesc *desc) {
    RndMat *mat = mMat;
    if (!(int)mat)
        return false;
    RndTex *diffTex = mat->GetDiffuseTex();
    if (!diffTex || (diffTex->GetType() & RndTex::kRenderedNoZ) != RndTex::kRenderedNoZ)
        return false;
    ObjectDir *dir = mMat.RefOwner()->Dir();
    if (mTwoColorDiffuse && diffTex != mTwoColorDiffuse
        && mTwoColorDiffuse->Dir() == dir) {
        delete (RndTex *)mTwoColorDiffuse;
    }
    if (mTwoColorInterp && diffTex != mTwoColorInterp
        && mTwoColorInterp->Dir() == dir) {
        delete (RndTex *)mTwoColorInterp;
    }
    if (mTwoColorMask && diffTex != mTwoColorMask && mTwoColorMask->Dir() == dir) {
        delete (RndTex *)mTwoColorMask;
    }
    desc->Compress(diffTex, mMat->GetBlend() == RndMat::kBlendSrcAlpha);
    return true;
}

OutfitConfig::Piercing::Piercing(Hmx::Object *o)
    : mPiercing(o, 0), mReskin(0), mPieces(o) {}

RndMesh *OutfitConfig::Piercing::GetHeadMesh() {
    return mPiercing.Owner()->Dir()->Find<RndMesh>("head.mesh", false);
}

void OutfitConfig::Piercing::Deform(SyncMeshCB *cb) {
    if (!mPiercing)
        return;
    RndMesh *reskinMesh = dynamic_cast<RndMesh *>((RndTransformable *)mPiercing);
    if (reskinMesh)
        cb->SyncMesh(reskinMesh, 0x1f);
    RndMesh *headMesh = GetHeadMesh();
    if (!headMesh)
        return;
    int _tmp0 = headMesh->Verts().size();
    if (_tmp0 == 0)
        return;
    const std::vector<SyncMeshCB::Vert> *beforeVerts = &cb->GetVerts(headMesh);
    if (!beforeVerts)
        return;
    if ((unsigned short)beforeVerts->size()
        != (unsigned int)headMesh->Verts().size()) {
        MILO_WARN(
            "%s can't apply piercing deformation, before verts different than head "
            "(0x%x) vert count (%d v %d)",
            PathName(mPiercing.Owner()),
            (int)headMesh,
            (unsigned long)beforeVerts->size(),
            headMesh->Verts().size()
        );
        return;
    }
    for (int i = 0.0f; i < mPieces.size(); i++) {
        const Piece &piece = mPieces[i];
        if (piece.mVert == -1)
            continue;
        if (reskinMesh && !piece.mAttachment) {
            if (piece.unk14.size()
                != (unsigned int)reskinMesh->Verts().size() * 2) {
                MILO_WARN(
                    "%s can't do piercing piece %d deform, head verts out of date, "
                    "need to re-ao",
                    PathName(mPiercing.Owner()),
                    i
                );
                continue;
            }
            for (int j = 0; j < reskinMesh->Verts().size(); j++) {
                unsigned short faceIdx = piece.unk14[j * 2];
                RndMesh::Vert &dst = reskinMesh->Verts(j);
                if (faceIdx >= (unsigned short)headMesh->Faces().size()) {
                    MILO_WARN(
                        "%s can't do piercing piece %d deform, head verts out of "
                        "date, need to re-ao",
                        PathName(mPiercing.Owner()),
                        i
                    );
                    break;
                }
                unsigned short packed = piece.unk14[j * 2 + 1];
                float weights[3];
                weights[0] = (float)(packed & 0xff) / 255.0f;
                weights[1] = (float)((packed >> 8) & 0xff) / 255.0f;
                weights[2] = 1.0f - weights[0] - weights[1];
                for (int k = 0; k < 3; k++) {
                    unsigned short *faceVerts = &headMesh->Faces(faceIdx).v1;
                    unsigned short srcIdx = faceVerts[k];
                    RndMesh::Vert &cur = headMesh->Verts(srcIdx);
                    const SyncMeshCB::Vert &before = (*beforeVerts)[srcIdx];
                    float w = weights[k];
                    dst.pos.x += (cur.pos.x - before.pos.x) * w;
                    dst.pos.y += (cur.pos.y - before.pos.y) * w;
                    dst.pos.z += (cur.pos.z - before.pos.z) * w;
                }
            }
        } else {
            if (piece.mVert >= headMesh->Verts().size()) {
                MILO_WARN(
                    "%s can't do piercing piece %d deform, head verts out of date, "
                    "need to re-ao",
                    PathName(mPiercing.Owner()),
                    i
                );
                continue;
            }
            const SyncMeshCB::Vert &before = (*beforeVerts)[piece.mVert];
            RndMesh::Vert &headVert = headMesh->Verts(piece.mVert);
            float dx = headVert.pos.x - before.pos.x;
            float dy = headVert.pos.y - before.pos.y;
            float dz = headVert.pos.z - before.pos.z;
            if (reskinMesh) {
                for (int j = 0; j < piece.unk14.size(); j++) {
                    unsigned short dstIdx = piece.unk14[j];
                    if (dstIdx >= reskinMesh->Verts().size()) {
                        MILO_WARN(
                            "%s mesh %s no longer matches piece %d, has fewer verts "
                            "(%d v %d), must re-AO file",
                            PathName(mPiercing.Owner()),
                            reskinMesh->Name(),
                            i,
                            (int)dstIdx,
                            reskinMesh->Verts().size()
                        );
                        break;
                    }
                    RndMesh::Vert &dst = reskinMesh->Verts(dstIdx);
                    dst.pos.x += dx;
                    dst.pos.y += dy;
                    dst.pos.z += dz;
                }
            } else {
                RndTransformable *attach = mPiercing;
                Transform &xfm = attach->DirtyLocalXfm();
                xfm.v.x = unkc.v.x + dx;
                xfm.v.y = unkc.v.y + dy;
                xfm.v.z = unkc.v.z + dz;
            }
        }
    }
}

void OutfitConfig::MeshAO::Apply(OutfitConfig *cfg, SyncMeshCB *mesh) {
    RndMesh *m =
        dynamic_cast<RndMesh *>(cfg->Dir()->FindObject(mMeshName.c_str(), false));
    if (m) {
        if (m->GetKeepMeshData()) {
            mesh->SyncMesh(m, 0x400);
            if ((unsigned int)m->Verts().size() == mCoeffs.size()) {
                m->SetHasAOCalc(true);
                for (unsigned int i = 0; i < mCoeffs.size(); i++) {
                    Hmx::Color32 ao(mCoeffs[i]);
                    Hmx::Color32 vc(m->Verts(i).color);
                    vc.a = Min(vc.a, ao.a);
                    vc.r = Min(vc.r, ao.r);
                    vc.g = Min(vc.g, ao.g);
                    vc.b = Min(vc.b, ao.b);
                    m->Verts(i).color.UnpackAlpha(vc.FullColor());
                }
            } else {
                MILO_WARN(
                    "%s MeshAO has different vert count %d v %d from %s, can't apply",
                    PathName(cfg),
                    (unsigned long)mCoeffs.size(),
                    m->Verts().size(),
                    m->Name()
                );
            }
        }
    } else {
        MILO_WARN(
            "%s MeshAO %s can't find matching mesh to apply", PathName(cfg), mMeshName
        );
    }
}

OutfitConfig::Overlay::Overlay(Hmx::Object *o) : mCategory(0), mTexture(o, 0) {}

void OutfitConfig::Init() {
    Register();
    sMat = Hmx::Object::New<RndMat>();
    sCam = Hmx::Object::New<RndCam>();
    sBandCharDesc = Hmx::Object::New<BandCharDesc>();
}

void OutfitConfig::Terminate() {
    RELEASE(sMat);
    RELEASE(sCam);
    RELEASE(sBandCharDesc);
}

OutfitConfig::OutfitConfig()
    : mMats(this), unk38(0), unk3c(0), mComputeAO(1), mPatches(this), mPermaProject(0),
      mPiercings(this), mTexBlender(this, 0), mWrinkleBlender(this, 0), mOverlays(this),
      mBandLogo(this, 0) {
    for (int i = 0; i < 3; i++)
        mColors[i] = i;
}

void OutfitConfig::SetColors(const int *cols) {
    bool recomp = false;
    for (int i = 0; i < 3; i++) {
        if (cols[i] != -1 && mColors[i] != cols[i]) {
            mColors[i] = cols[i];
            recomp = true;
        }
    }
    if (recomp)
        Recompose();
}

void OutfitConfig::Recompose() { unk38 = -1; }

void OutfitConfig::RecomposePatches(int flag) {
    for (int i = 0; i < mPatches.size(); i++) {
        if (flag & mPatches[i].mCategory) {
            unk38 |= flag;
        }
    }
}

// Retail fn_8229FF30 (0xD4 bytes, inside this TU's own .text span; called five
// times from the 3-arg SetSkinTextures). Points one head-feature texblend
// controller at the numbered head-normal map for the feature's current option and
// reports whether that actually changed anything. Reconstructed from the retail
// assembly -- rb3-Wii never decompiled it, which is exactly why its two format
// strings sat unreferenced in the DECOMP_FORCEACTIVE at the bottom of this file.
static bool
SetHeadNormMap(const char *part, int option, Symbol gender, ObjectDir *dir1, ObjectDir *dir2) {
    RndTexBlendController *ctrl =
        dir2->Find<RndTexBlendController>(MakeString("norm_%s.texblendctl", part), false);
    if (!ctrl) {
        MILO_WARN("%s could not find norm_%s.texblendctl", PathName(dir2), part);
        return false;
    }
    RndTex *tex =
        dir1->Find<RndTex>(MakeString("%s_head_norm%02d.tex", gender, option + 1), false);
    if (!tex) {
        MILO_WARN("%s could not find head norm %d", PathName(dir1), option + 1);
        return false;
    }
    if (tex == ctrl->Tex())
        return false;
    ctrl->SetTex(tex);
    return true;
}

void OutfitConfig::SetSkinTextures(ObjectDir *dir1, ObjectDir *dir2, BandCharDesc *desc) {
    OutfitConfig *cfg = dir2->Find<OutfitConfig>("skin.cfg", false);
    static const char *skinMats[] = {
        "torso_naked.mat",     "torso", "legs_skin.mat",  "legs", "feet_skin.mat", "legs",
        "feet_socks_skin.mat", "legs",  "head_naked.mat", "head"
    };
    Symbol gender = desc->mGender;
    for (int i = 0; 5 > i; i++) {
        const char *partname = skinMats[i * 2 + 1];
        RndMat *curmat = dir1->Find<RndMat>(skinMats[i * 2], false);
        if (curmat) {
            RndTex *curtex = dir1->Find<RndTex>(
                MakeString("%s_%s_diff.tex", gender, partname), false
            );
            if (curtex) {
                if (cfg) {
                    if (i < 2 || i == 4) {
                        int idx = 2;
                        if (i != 4)
                            idx = i;
                        MatSwap &curswap = cfg->mMats[idx];
                        curswap.mTwoColorDiffuse = curtex;
                        RndTex *interptex = dir1->Find<RndTex>(
                            MakeString("%s_%s_interp_gw.tex", gender, partname), false
                        );
                        if (interptex)
                            curswap.mTwoColorInterp = interptex;
                    }
                    RndTex *difftex = dir2->Find<RndTex>(
                        MakeString("%s_skin_diffuse_output.tex", partname), false
                    );
                    if (difftex)
                        curmat->SetDiffuseTex(difftex);
                } else {
                    curmat->SetDiffuseTex(curtex);
                }
            }
            // Specular map. Retail binds it UNCONDITIONALLY (SetSpecularMap is
            // out-of-line and is called even with a null texture), then warns.
            RndTex *spectex =
                dir1->Find<RndTex>(MakeString("%s_%s_spec.tex", gender, partname), false);
            curmat->SetSpecularMap(spectex);
            if (!spectex)
                MILO_WARN("%s could not find %s spec map", PathName(dir1), partname);
            // Normal map. The head (i == 4) takes its normal from the wrinkle
            // blender's output RT instead of a numbered *_norm map.
            if (i < 4) {
                const char *variant = desc->HeadNormVariant();
                const char *normname;
                if (!*variant)
                    normname = MakeString("%s_%s_norm.tex", gender, partname);
                else
                    normname = MakeString("%s_%s_norm_%s.tex", gender, partname, variant);
                RndTex *normtex = dir1->Find<RndTex>(normname, false);
                curmat->SetNormalMap(normtex);
                if (!normtex)
                    MILO_WARN("%s could not find %s norm map", PathName(dir1), partname);
            } else {
                RndTex *wrinkletex = dir2->Find<RndTex>("head_wrinkle_output.tex", false);
                if (wrinkletex)
                    curmat->SetNormalMap(wrinkletex);
                else if (cfg)
                    MILO_WARN("%s could not find head_wrinkle_output.tex", PathName(dir2));
            }
        } else
            MILO_WARN("%s could not find %s", PathName(dir1), skinMats[i * 2]);
    }
    OutfitConfig *eyesCfg = dir2->Find<OutfitConfig>("eyes.cfg", false);
    if (eyesCfg) {
        // Five head-feature normal maps. The BandCharDesc offsets retail loads
        // (0x2c/0x4c/0x5c/0x40/0x28) are Head-relative 0x14/0x34/0x44/0x28/0x10
        // (Head sits at BandCharDesc+0x18), i.e. exactly mChin/mEye/mMouth/mNose/
        // mShape -- which independently corroborates the five string literals.
        BandCharDesc::Head &head = desc->GetHead();
        bool changed = SetHeadNormMap("chin", head.mChin, gender, dir1, dir2);
        changed = SetHeadNormMap("eye", head.mEye, gender, dir1, dir2) | changed;
        changed = SetHeadNormMap("mouth", head.mMouth, gender, dir1, dir2) | changed;
        changed = SetHeadNormMap("nose", head.mNose, gender, dir1, dir2) | changed;
        changed = SetHeadNormMap("shape", head.mShape, gender, dir1, dir2) | changed;
        if (changed) {
            // OutfitConfig+0x84 is mTexBlender's object pointer: mTexBlender is an
            // ObjPtr at 0x7c and ObjPtr is {vtable@0, mOwner@4, mObject@8}. (Both
            // offsets verified with cl /d1reportSingleClassLayout -- the `// 0xHEX`
            // comments in these headers are stale and say 0x68 / 0x7c.)
            eyesCfg->mTexBlender->SetUnkc0(true);
            RndTexBlender *wrinkle = dir2->Find<RndTexBlender>("wrinkle.texblend", false);
            if (wrinkle)
                wrinkle->SetUnkc0(true);
        }
    }
    if (cfg) {
        RndMesh *torsomesh =
            dir1->Find<RndMesh>(MakeString("%s_tattoo_torso.mesh", gender), false);
        if (torsomesh)
            torsomesh->SetMat(dir1->Find<RndMat>("torso_naked.mat", false));
        RndMesh *legsmesh =
            dir1->Find<RndMesh>(MakeString("%s_tattoo_legs.mesh", gender), false);
        if (legsmesh)
            legsmesh->SetMat(dir1->Find<RndMat>("legs_skin.mat", false));
        RndMesh *headmesh =
            dir1->Find<RndMesh>(MakeString("%s_tattoo_head.mesh", gender), false);
        if (headmesh)
            headmesh->SetMat(dir1->Find<RndMat>("head_naked.mat", false));
    }
}

DECOMP_FORCEACTIVE(OutfitConfig, "norm_%s.texblendctl", "%s_head_norm%02d.tex")
DECOMP_FORCEACTIVE(OutfitConfig, "ObjPtr_p.h", "f.Owner()", "")

BinStream &operator>>(BinStream &bs, OutfitConfig::MatSwap &swap) {
    bs >> swap.mMat;
    bs >> swap.mResourceMat;
    if (OutfitConfig::gRev < 5) {
        bool b;
        bs >> b;
    } else {
        bs >> swap.mTwoColorDiffuse;
        bs >> swap.mTwoColorInterp;
    }
    bs >> swap.mTwoColorMask;
    if (OutfitConfig::gRev > 4) {
        bs >> swap.mColor1Palette;
        bs >> swap.mColor1Option;
        bs >> swap.mColor2Palette;
        bs >> swap.mColor2Option;
        bs >> swap.mTextures;
    }
    swap.SyncTwoColor();
    return bs;
}

BinStream &operator>>(BinStream &bs, OutfitConfig::Piercing::Piece &piece) {
    if (OutfitConfig::gRev > 0xF)
        bs >> piece.mVert;
    else {
        piece.mVert = -1;
        int i, j;
        bs >> i;
        bs >> j;
    }
    if (OutfitConfig::gRev < 0xF) {
        bool b;
        bs >> b;
    }
    if (OutfitConfig::gRev < 0xE) {
        Transform tf;
        bs >> tf;
        std::vector<unsigned short> vec;
        bs >> vec;
    } else
        bs >> piece.mAttachment;
    bs >> piece.unk14;
    if (OutfitConfig::gRev < 0x1A && !piece.mAttachment) {
        piece.mVert = -1;
        piece.unk14.clear();
    }
    return bs;
}

BinStream &operator>>(BinStream &bs, OutfitConfig::Piercing &piercing) {
    bs >> piercing.mPiercing;
    if (OutfitConfig::gRev < 0xD) {
        piercing.mPieces.resize(1);
        OutfitConfig::Piercing::Piece &curPiece = piercing.mPieces[0];
        int i, j;
        bs >> i;
        bs >> j;
        curPiece.mVert = -1;
        if (OutfitConfig::gRev < 0xE) {
            Transform tf;
            bs >> tf;
        }
    } else {
        bs >> piercing.unkc;
        if (OutfitConfig::gRev < 0xE) {
            bool b;
            bs >> b;
        }
        if (OutfitConfig::gRev == 0x10) {
            bool b;
            bs >> b;
        }
        bs >> piercing.mPieces;
        if (OutfitConfig::gRev > 0x1A)
            bs >> piercing.mReskin;
    }
    return bs;
}

BinStream &operator>>(BinStream &bs, OutfitConfig::Overlay &o) {
    bs >> o.mCategory;
    bs >> o.mTexture;
    return bs;
}

BinStream &operator>>(BinStream &bs, OutfitConfig::MeshAO::Seam &seam) {
    bs >> seam.mIndex;
    bs >> seam.mCoeff;
    return bs;
}

BinStream &operator>>(BinStream &bs, OutfitConfig::MeshAO &ao) {
    bs >> ao.mMeshName;
    if (OutfitConfig::gRev == 9 || OutfitConfig::gRev == 10 || OutfitConfig::gRev == 11
        || OutfitConfig::gRev == 12 || OutfitConfig::gRev == 13
        || OutfitConfig::gRev == 14 || OutfitConfig::gRev == 15
        || OutfitConfig::gRev == 16 || OutfitConfig::gRev == 17
        || OutfitConfig::gRev == 18 || OutfitConfig::gRev == 19
        || OutfitConfig::gRev == 20 || OutfitConfig::gRev == 21
        || OutfitConfig::gRev == 22) {
        CSHA1::Digest d;
        bs >> d;
    }
    bs >> ao.mCoeffs;
    bs >> ao.mSeams;
    if (OutfitConfig::gRev > 0x18)
        bs >> ao.unkc;
    return bs;
}

BinStream &operator>>(BinStream &bs, OldMatOption &o) {
    bs >> o.mMat;
    bs >> o.mPrimaryPalette;
    if (OutfitConfig::gRev != 0)
        bs >> o.mSecondaryPalette;
    bs >> o.mTexs;
    return bs;
}

BinStream &operator>>(BinStream &bs, OldColorOption &o) {
    bs >> o.mColorIndex;
    bs >> o.mMatOptions;
    return bs;
}

SAVE_OBJ(OutfitConfig, 0x5C7)

#pragma push
#pragma dont_inline on
BEGIN_LOADS(OutfitConfig)
    LOAD_REVS(bs)
    ASSERT_REVS(0x1B, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    if (gRev > 4) {
        bs >> mColors[0];
        bs >> mColors[1];
        if (gRev > 10)
            bs >> mColors[2];
    }
    if (gRev > 3)
        bs >> mMats;
    if (gRev < 5) {
        ObjVector<OldColorOption> colors(this);
        bs >> colors;
        for (int i = 0; i < colors.size(); i++) {
            OldColorOption &curColor = colors[i];
            mColors[i] = curColor.mColorIndex;
            for (ObjList<OldMatOption>::iterator it = curColor.mMatOptions.begin();
                 it != curColor.mMatOptions.end();
                 ++it) {
                int m;
                for (m = 0; m < mMats.size(); m++) {
                    if (mMats[m].mMat == it->mMat)
                        break;
                    if (mMats[m].mResourceMat == it->mMat)
                        break;
                }
                if (m == mMats.size()) {
                    mMats.push_back();
                    mMats.back().mResourceMat = it->mMat;
                    mMats.back().mMat = it->mMat;
                }
                MatSwap &curSwap = mMats[m];
                if (it->mTexs.size() != 0) {
                    curSwap.mTextures = it->mTexs;
                    curSwap.mColor1Option = i;
                }
                if (it->mPrimaryPalette) {
                    curSwap.mColor1Palette = it->mPrimaryPalette;
                    curSwap.mColor1Option = i;
                }
                if (it->mSecondaryPalette) {
                    curSwap.mColor2Palette = it->mSecondaryPalette;
                    curSwap.mColor2Option = i;
                }
            }
        }
        if (gRev != 0) {
            if (gRev < 2) {
                ObjVector<ObjPtr<RndDir> > dirs(this);
                bs >> dirs;
            } else if (gRev < 1) {
                ObjVector<ObjPtr<RndDir> > dirs(this);
                bs >> dirs;
            }
        }
    }
    if (gRev > 6)
        bs >> mMeshAO;
    if (gRev > 7)
        bs >> mComputeAO;
    if (gRev > 9)
        bs >> mPatches;
    if (gRev > 0xB)
        bs >> mPiercings;
    if (gRev > 0x12)
        bs >> mTexBlender;
    if (gRev > 0x13)
        bs >> mOverlays;
    if (gRev > 0x14)
        bs >> mBandLogo;
    if (gRev > 0x15)
        bs >> mDigest;
    if (gRev > 0x17)
        bs >> mWrinkleBlender;
END_LOADS

BEGIN_COPYS(OutfitConfig)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(OutfitConfig)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mColors[0])
        COPY_MEMBER(mColors[1])
        COPY_MEMBER(mColors[2])
        COPY_MEMBER(mMats)
        COPY_MEMBER(mMeshAO)
        COPY_MEMBER(mPiercings)
        COPY_MEMBER(mOverlays)
        COPY_MEMBER(mComputeAO)
        COPY_MEMBER(mTexBlender)
        COPY_MEMBER(mWrinkleBlender)
        COPY_MEMBER(mBandLogo)
        Recompose();
    END_COPYING_MEMBERS
END_COPYS
#pragma pop

void OutfitConfig::PreSave(BinStream &bs) {
    for (ObjVector<MatSwap>::iterator it = mMats.begin(); it != mMats.end(); ++it) {
        it->UnSwapResource();
    }
    Recompose();
}

void OutfitConfig::PostSave(BinStream &) {}
void OutfitConfig::UpdatePreClearState() {
    TheRnd.PreClearDrawAddOrRemove(this, true, false);
}

BandCharDesc *OutfitConfig::FindBandCharDesc() {
    if (Dir()) {
        static Symbol sBandCharName("BandCharacter");
        if (Dir()->ClassName() == sBandCharName) {
            return static_cast<BandCharacter *>(Dir());
        }
        if (Dir()->Dir()) {
            if (Dir()->Dir()->ClassName() == sBandCharName) {
                return static_cast<BandCharacter *>(Dir()->Dir());
            }
        }
    }
    sBandCharDesc->SetGender(
        Symbol(strstr(Dir()->GetPathName(), "female") ? "female" : "male")
    );
    return sBandCharDesc;
}

void OutfitConfig::SetSkinTextures() {
    Hmx::Object *obj = Dir()->Find<Hmx::Object>("torso_naked.mat", false);
    if (obj) {
        SetSkinTextures(Dir(), Dir(), FindBandCharDesc());
        OutfitConfig *cfg = Dir()->Find<OutfitConfig>("skin.cfg", false);
        if (cfg)
            cfg->Recompose();
    }
}

void OutfitConfig::Randomize() {
    for (int i = 0; i < 3; i++) {
        int num = NumIndices(i);
        if (num != 0)
            mColors[i] = RandomInt(0, num);
    }
    Recompose();
}

int OutfitConfig::NumColorOptions() const {
    int maxOption = -1;
    for (int i = 0; i < mMats.size(); i++) {
        const MatSwap &m = mMats[i];
        if (m.mColor1Palette || !m.mTextures.empty()) {
            if (maxOption < mMats[i].mColor1Option)
                maxOption = mMats[i].mColor1Option;
        }
        if (m.mColor2Palette) {
            if (maxOption < mMats[i].mColor2Option)
                maxOption = mMats[i].mColor2Option;
        }
    }
    return maxOption + 1;
}

unsigned int OutfitConfig::OverlayFlags() const {
    unsigned int flags = 0;
    for (int i = 0; i < mOverlays.size(); i++) {
        flags |= mOverlays[i].mCategory;
    }
    return flags;
}

bool OutfitConfig::InMilo() {
    if (ObjectDir::Main()->FindObject("milo", false)) {
        static DataNode &n = DataVariable("milo.dir");
        bool result = false;
        bool isSameDir = false;
        bool isMiloObj = false;
        DataNode *nPtr = &n;
        if (nPtr->Type() == kDataObject && Dir()) {
            isMiloObj = true;
        }
        if (isMiloObj) {
            ObjectDir *castedDir = dynamic_cast<ObjectDir *>(nPtr->GetObj(NULL));
            if (Dir() == castedDir) {
                isSameDir = true;
            }
        }
        if (isSameDir) {
            if (!streq(Dir()->Name(), "main")) {
                result = true;
            }
        }
        return result;
    }
    return false;
}

int OutfitConfig::NumIndices(int idx) const {
    for (int i = 0; i < mMats.size(); i++) {
        const MatSwap &m = mMats[i];
        if (m.mColor1Option == idx) {
            if (m.mColor1Palette)
                return m.mColor1Palette->NumColors();
            if (!m.mTextures.empty())
                return m.mTextures.size();
        }
        if (m.mColor2Option == idx) {
            if (m.mColor2Palette)
                return m.mColor2Palette->NumColors();
        }
    }
    return 0;
}

void OutfitConfig::CompressTextures() {
    if (unk3c != 2)
        unk3c = 1;
}

void OutfitConfig::ApplyAO(SyncMeshCB *mesh) {
    for (int i = 0; i < mMeshAO.size(); i++) {
        mMeshAO[i].Apply(this, mesh);
    }
    for (int i = 0; i < mPiercings.size(); i++) {
        mPiercings[i].Deform(mesh);
    }
}

void OutfitConfig::DrawPreClear() {
    if (mTexBlender && mTexBlender->Unkc0()) {
        mTexBlender->DrawShowing();
    }
    if (mPermaProject && TheLoadMgr.EditMode()) {
        for (int i = 0; i < mPatches.size(); i++) {
            if (mPatches[i].ReProject())
                Recompose();
        }
    }
    BandCharDesc *desc = NULL;
    if (unk3c == 1 || unk38 != 0) {
        desc = FindBandCharDesc();
    }
    if (unk38 != 0) {
        if (InMilo()) {
            PoseBones();
            SetSkinTextures();
        }
        if (unk3c != 2) {
            int dirty = unk38;
            if (dirty != -1) {
                for (ObjVector<BandPatchMesh>::iterator pit = mPatches.begin();
                     pit != mPatches.end();
                     ++pit) {
                    if (unk38 & pit->mCategory) {
                        for (ObjVector<BandPatchMesh::MeshPair>::iterator mp =
                                 pit->mMeshes.begin();
                             mp != pit->mMeshes.end();
                             ++mp) {
                            MILO_ASSERT(mp->mesh, 0x725);
                            RndMat *mpMat = mp->mesh->Mat();
                            for (ObjVector<BandPatchMesh>::iterator other =
                                     mPatches.begin();
                                 other != mPatches.end();
                                 ++other) {
                                if (other != pit) {
                                    for (ObjVector<BandPatchMesh::MeshPair>::iterator omp =
                                             other->mMeshes.begin();
                                         omp != other->mMeshes.end();
                                         ++omp) {
                                        if (mpMat == omp->mesh->Mat()) {
                                            dirty |= other->mCategory;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            for (ObjVector<MatSwap>::iterator it = mMats.begin(); it != mMats.end();
                 ++it) {
                it->SwapResource();
            }
            for (int i = 0; i < mPatches.size(); i++) {
                mPatches[i].PreRender(desc, dirty);
            }
            for (ObjVector<MatSwap>::iterator it = mMats.begin(); it != mMats.end();
                 ++it) {
                if (!it->mTwoColor) {
                    it->Compose(mColors, mPatches, unk38);
                }
            }
            for (ObjVector<MatSwap>::iterator it = mMats.begin(); it != mMats.end();
                 ++it) {
                if (it->mTwoColor) {
                    it->Compose(mColors, mPatches, unk38);
                }
            }
            for (int i = 0; i < mPatches.size(); i++) {
                mPatches[i].PostRender();
            }
        }
        if (mBandLogo) {
            RndTex *logoTex = desc->GetBandLogo();
            if (!logoTex)
                logoTex = TheRnd.GetDefaultTex(Rnd::kDefaultTex_White);
            mBandLogo->SetDiffuseTex(logoTex);
        }
        unk38 = 0;
    }
    if (unk3c == 1) {
        for (ObjVector<MatSwap>::iterator it = mMats.begin(); it != mMats.end();
             ++it) {
            it->Compress(desc);
        }
        BandCharDesc *desc2 = FindBandCharDesc();
        for (int i = 0; i < mPatches.size(); i++) {
            mPatches[i].Compress(desc2);
        }
        if (mTexBlender) {
            RndTex *blendDest = mTexBlender->OutputTexture();
            if (blendDest
                && (blendDest->GetType() & RndTex::kRenderedNoZ) == RndTex::kRenderedNoZ) {
                desc2->Compress(blendDest, false);
            }
        }
        unk3c = 2;
    }
    if (mWrinkleBlender) {
        mWrinkleBlender->DrawShowing();
    }
}

void OutfitConfig::Mats(std::list<RndMat *> &list, bool allocTempMats) {
    for (ObjVector<MatSwap>::iterator it = mMats.begin(); it != mMats.end(); ++it) {
        if (!it->mResourceMat)
            continue;
        // Do NOT cache Refs() in a local -- retail re-derives the end sentinel
        // from mResourceMat every iteration (`lwz r11,0x14(rIT); addi r11,r11,0x20`).
        // Caching it burns an extra callee-saved reg (__savegprlr_22 vs _23).
        for (ObjRef::iterator rit = it->mResourceMat->Refs().begin();
             rit != it->mResourceMat->Refs().end();) {
            // X360: capture the ring-ref (ObjRefNode::refPtr@8) BEFORE advancing.
            // Retail loads refPtr then next: `lwz r29,0x8(r28); lwz r28,0x0(r28)`.
            ObjRefOwner *cur = RefPtrOf(rit);
            ++rit;
            RndMesh *mesh = dynamic_cast<RndMesh *>(cur->RefOwner());
            if (mesh) {
                if (allocTempMats) {
                    // ObjRef::Replace(Hmx::Object*) is a NON-virtual stub off
                    // HX_NATIVE (OBJREF_VIRTUAL is empty) -- it inlines to
                    // nothing, so the whole `if (allocTempMats)` gets deleted.
                    // Retail dispatches ObjRefOwner::Replace (vtable slot +8)
                    // with the outgoing object as `from`:
                    //   lwz r11,0(r29); mr r3,r29; lwz r5,0x8(r31)
                    //   lwz r4,0x14(r31); lwz r11,0x8(r11); mtctr; bctrl
                    cur->Replace(
                        reinterpret_cast<ObjRef *>((RndMat *)it->mResourceMat),
                        it->mMat
                    );
                }
                // NOTE: retail copies the 8-byte MatShaderOptions with a single
                // ld/std pair; we emit 2x lwz/stw because our MatShaderOptions
                // (u32 union + bool) has alignment 4, not 8. Fixing that is a
                // struct-definition change affecting every material user -- out
                // of scope here, and it is the residual ~14 mismatches.
                it->mMat->SetShaderOpts(GetDefaultMatShaderOpts(mesh, it->mMat));
                list.push_back(it->mMat);
            }
        }
    }
    if (allocTempMats) {
        SetSkinTextures();
        if (mBandLogo) {
            mBandLogo->SetDiffuseTex(TheRnd.GetDefaultTex(Rnd::kDefaultTex_White));
        }
    }
}

void OutfitConfig::PoseBones() {
    CharClip *clip = BandCharDesc::GetDeformClip(
        Symbol(strstr(Dir()->GetPathName(), "female") ? "female" : "male")
    );
    if (clip) {
        clip->PoseMeshes(Dir(), clip->StartBeat());
    }
    RndMesh *headMesh =
        dynamic_cast<RndMesh *>(Dir()->FindObject("head.mesh", false));
    if (headMesh) {
        BandHeadShaper shaper;
        CharMeshCacheMgr cacheMgr;
        if (shaper.Start(
                Dir(),
                Symbol(strstr(Dir()->GetPathName(), "female") ? "female" : "male"),
                headMesh,
                &cacheMgr,
                true
            )) {
            BandCharDesc::Head head;
            head.SetShape(shaper);
        }
    }
}

void OutfitConfig::ListDrawChildren(std::list<RndDrawable *> &list) {
    for (int i = 0; i < mPatches.size(); i++) {
        mPatches[i].ListDrawChildren(list);
    }
    list.push_back(mTexBlender);
    list.push_back(mWrinkleBlender);
}

BEGIN_HANDLERS(OutfitConfig)
    HANDLE_ACTION(recompose, Recompose())
    HANDLE_ACTION(randomize_colors, Randomize())
    HANDLE_EXPR(num_color_options, NumColorOptions())
    HANDLE_ACTION(compress_textures, CompressTextures())
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xB3C)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(OutfitConfig::Piercing::Piece)
    SYNC_PROP(attachment, o.mAttachment)
    SYNC_PROP(highlight, o.mHighlight)
    SYNC_PROP(vert, o.mVert)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(OutfitConfig::Piercing)
    SYNC_PROP(piercing, o.mPiercing)
    SYNC_PROP(reskin, o.mReskin)
    SYNC_PROP(pieces, o.mPieces)
END_CUSTOM_PROPSYNC

void PropSyncTwoColor(OutfitConfig::MatSwap &swap) {
    swap.SyncTwoColor();
    gOutfitConfigOwner->Recompose();
}

BEGIN_CUSTOM_PROPSYNC(OutfitConfig::MatSwap)
    SYNC_PROP_MODIFY_ALT(mat, o.mMat, gOutfitConfigOwner->Recompose())
    SYNC_PROP_MODIFY_ALT(resource_mat, o.mResourceMat, gOutfitConfigOwner->Recompose())
    SYNC_PROP_MODIFY_ALT(two_color_diffuse, o.mTwoColorDiffuse, PropSyncTwoColor(o))
    SYNC_PROP_MODIFY_ALT(two_color_interp, o.mTwoColorInterp, PropSyncTwoColor(o))
    SYNC_PROP_MODIFY_ALT(two_color_mask, o.mTwoColorMask, PropSyncTwoColor(o))
    SYNC_PROP_MODIFY_ALT(color1_palette, o.mColor1Palette, gOutfitConfigOwner->Recompose())
    SYNC_PROP_MODIFY_ALT(color1_option, o.mColor1Option, gOutfitConfigOwner->Recompose())
    SYNC_PROP_MODIFY_ALT(color2_palette, o.mColor2Palette, gOutfitConfigOwner->Recompose())
    SYNC_PROP_MODIFY_ALT(color2_option, o.mColor2Option, gOutfitConfigOwner->Recompose())
    SYNC_PROP_MODIFY_ALT(textures, o.mTextures, gOutfitConfigOwner->Recompose())
    SYNC_PROP(two_color, o.mTwoColor)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(OutfitConfig::MeshAO::Seam)
    SYNC_PROP(index, o.mIndex)
    SYNC_PROP(coeff, o.mCoeff)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(OutfitConfig::MeshAO)
    SYNC_PROP(meshname, o.mMeshName)
    SYNC_PROP(coeffs, o.mCoeffs)
    SYNC_PROP(seams, o.mSeams)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(OutfitConfig::Overlay)
    SYNC_PROP(category, o.mCategory)
    SYNC_PROP(texture, o.mTexture)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(OutfitConfig)
    gOutfitConfigOwner = this;
    SYNC_PROP_MODIFY_ALT(primary_color, mColors[0], Recompose())
    SYNC_PROP_MODIFY_ALT(secondary_color, mColors[1], Recompose())
    SYNC_PROP_MODIFY_ALT(tertiary_color, mColors[2], Recompose())
    SYNC_PROP(mats, mMats)
    SYNC_PROP(meshao, mMeshAO)
    SYNC_PROP(compute_ao, mComputeAO)
    SYNC_PROP(patches, mPatches)
    SYNC_PROP(perma_project, mPermaProject)
    SYNC_PROP(piercings, mPiercings)
    SYNC_PROP(overlays, mOverlays)
    SYNC_PROP(tex_blender, mTexBlender)
    SYNC_PROP(wrinkle_blender, mWrinkleBlender)
    SYNC_PROP(band_logo, mBandLogo)
    SYNC_SUPERCLASS(RndDrawable)
END_PROPSYNCS

// sw3 COMDAT-scatter owner-TU includes (OutfitConfig owns these foreign spans).
// Outer `#ifndef gRev` guard: OutfitConfig.cpp is itself scatter-included as an
// owner by Gem.cpp and ExternalMic.cpp (`#define gRev gRev_OutfitConfig`), so this
// whole region must stay inert when transitively included (gRev is a static member
// VARIABLE, never a macro, in OutfitConfig's own primary TU). One shared
// SW_SCATTER_OWNER_INCLUDE region inside: (a) keeps BandCamShot's own nested
// scatter-includes inert, (b) prevents the per-shim guard from blocking siblings.
#ifndef gRev
#define SW_SCATTER_OWNER_INCLUDE

// plain ObjMacros owner
#include "bandobj/BandCamShot.cpp"

// CROSS ObjMacros<-Object dialect shims
#define gRev gRev_FontBase
#define gAltRev gAltRev_FontBase
#include "obj/dialect_object_push.h"
#include "rndobj/FontBase.cpp"
#include "obj/dialect_object_pop.h"
#undef gRev
#undef gAltRev

// NOTE: band3/game/Stats.cpp owner-include OMITTED — its own unguarded nested
// scatter-includes (hamobj/RhythmDetector.cpp) drag in hamobj/Difficulty.h, whose
// enum Difficulty is INCOMPATIBLE with game/Defines.h's (pulled via ContextChecker
// below) → C2011 redefinition. Matches the harvest's "1 plain + 3 dialect-shim".
#define gRev gRev_ContextChecker
#define gAltRev gAltRev_ContextChecker
#include "obj/dialect_object_push.h"
#include "band3/meta_band/ContextChecker.cpp"
#include "obj/dialect_object_pop.h"
#undef gRev
#undef gAltRev

#define gRev gRev_CharSignalApplier
#define gAltRev gAltRev_CharSignalApplier
#include "obj/dialect_object_push.h"
#include "char/CharSignalApplier.cpp"
#include "obj/dialect_object_pop.h"
#undef gRev
#undef gAltRev

#undef SW_SCATTER_OWNER_INCLUDE
#endif // ifndef gRev (OutfitConfig-as-owner inert guard)

// ZS-MISSING-INSTANTIATION: retail out-of-lined this by-value MakeString COMDAT
// in this TU; force emission (BandWardrobe idiom).
#include "utl/MakeString.h"
template const char *
MakeString<const char *, const char *>(const char *, const char *, const char *);
