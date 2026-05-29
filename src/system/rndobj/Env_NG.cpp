#include "rndobj/Env_NG.h"
#include "rndobj/Lit_NG.h"
#include "rndobj/Mat_NG.h"
#include "rndobj/Rnd.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Stats_NG.h"
#include "rnddx9/RenderState.h"

namespace {
    Transform sIdentityXfm(
        Hmx::Matrix3(
            Vector3(1.0f, 0.0f, 0.0f),
            Vector3(0.0f, 1.0f, 0.0f),
            Vector3(0.0f, 0.0f, 1.0f)
        ),
        Vector3(0.0f, 0.0f, 0.0f)
    );

    void ClearLightTransforms() {
        TheShaderMgr.SetVConstant4x3(
            (VShaderConstant)0xdd, *(const Hmx::Matrix4 *)&sIdentityXfm
        );
        TheShaderMgr.SetPConstant4x3(
            (PShaderConstant)0xdd, *(const Hmx::Matrix4 *)&sIdentityXfm
        );
    }

    void ClearPointCubeTex() {
        TheShaderMgr.SetPConstant((PShaderConstant)0xd, TheRnd.DefaultCubeTexWhite());
        TheRenderState.SetTextureClamp(0xd, (RndRenderState::ClampMode)1);
        TheRenderState.SetTextureFilter(0xd, (RndRenderState::FilterMode)1, false);
    }

    bool CheckPointLight(NgLight &light) {
        if (!light.Showing())
            return false;
        Hmx::Color color = light.GetColor();
        return color.Pack() != 0;
    }

    bool CheckProjLight(NgLight &light) {
        if (!light.Showing())
            return false;
        if (light.GetProjectedBlend() == 0 && light.GetTexture() == nullptr)
            return false;
        if (light.GetProjectedBlend() == 1) {
            bool hasShadowOverride = light.GetShadowOverride()
                && light.GetShadowOverride()->size() != 0;
            if (!hasShadowOverride && light.ShadowObjectsSize() == 0)
                return false;
        }
        Hmx::Color color = light.GetColor();
        if (color.Pack() == 0)
            return false;
        light.CheckShadowMap();
        return true;
    }

    bool SetPointLightRegisters(int lightIdx, RndLight &light, bool &hasPointCubeTex) {
        hasPointCubeTex = false;
        if (!light.Showing())
            return false;
        Hmx::Color color = light.GetColor();
        if (color.Pack() == 0)
            return false;
        {
            const Transform &xfm = light.WorldXfm();
            float posX = xfm.v.x;
            float posY = xfm.v.y;
            float posZ = xfm.v.z;
            float range = light.Range();
            float falloff = light.FalloffStart();
            float invRange, rangeScale;
            if (range <= falloff) {
                rangeScale = 1.0f;
                invRange = 0.0f;
            } else {
                invRange = 1.0f / (falloff - range);
                rangeScale = -(range * invRange);
            }
            Vector4 pos(posX, posY, posZ, invRange);
            TheShaderMgr.SetVConstant((VShaderConstant)(lightIdx + 0x40), pos);
            TheShaderMgr.SetPConstant((PShaderConstant)(lightIdx + 0x40), pos);
            Vector4 colorVec(color.red, color.green, color.blue, rangeScale);
            TheShaderMgr.SetVConstant((VShaderConstant)(lightIdx + 0x43), colorVec);
            Vector4 colorVec2(color.red, color.green, color.blue, rangeScale);
            TheShaderMgr.SetPConstant((PShaderConstant)(lightIdx + 0x43), colorVec2);
            if (light.GetCubeTexture() != nullptr && lightIdx == 0) {
                TheShaderMgr.SetPConstant((PShaderConstant)0xd, light.GetCubeTexture());
                TheRenderState.SetTextureClamp(0xd, (RndRenderState::ClampMode)1);
                TheRenderState.SetTextureFilter(0xd, (RndRenderState::FilterMode)1, false);
                Transform localXfm = light.WorldXfm();
                TheShaderMgr.SetVConstant4x3(
                    (VShaderConstant)0xdd, Hmx::Matrix4(localXfm)
                );
                TheShaderMgr.SetPConstant4x3(
                    (PShaderConstant)0xdd, Hmx::Matrix4(localXfm)
                );
                hasPointCubeTex = true;
            }
            return true;
        }
    }

    bool SetProjLightRegisters(int lightIdx, int projIdx, NgLight &light) {
        if (!light.Showing())
            return false;
        if (light.GetTexture() == nullptr && light.GetProjectedBlend() == 0)
            return false;
        Hmx::Color color = light.GetColor();
        if (color.Pack() == 0)
            return false;
        const Transform &xfm = light.WorldXfm();
        float dirX = -xfm.m.y.x;
        float dirY = -xfm.m.y.y;
        float dirZ = -xfm.m.y.z;
        Vector4 dir(dirX, dirY, dirZ, 0.0f);
        TheShaderMgr.SetVConstant((VShaderConstant)(lightIdx + 0x40), dir);
        Vector4 dir2(dirX, dirY, dirZ, 0.0f);
        TheShaderMgr.SetPConstant((PShaderConstant)(lightIdx + 0x40), dir2);
        Vector4 colorVec(color.red, color.green, color.blue, 1.0f);
        TheShaderMgr.SetVConstant((VShaderConstant)(lightIdx + 0x43), colorVec);
        Vector4 colorVec2(color.red, color.green, color.blue, 1.0f);
        TheShaderMgr.SetPConstant((PShaderConstant)(lightIdx + 0x43), colorVec2);
        Transform proj = light.Projection();
        TheShaderMgr.SetPConstant4x3(
            (PShaderConstant)(projIdx * 3 + 0x5f), Hmx::Matrix4(proj)
        );
        TheShaderMgr.SetPConstant((PShaderConstant)(projIdx + 5), light.GetShadowMapTex());
        TheRenderState.SetTextureFilter(projIdx + 5, (RndRenderState::FilterMode)1, false);
        int blend = light.GetProjectedBlend();
        if (blend == 0) {
            TheShaderMgr.SetPConstant((PShaderConstant)(projIdx + 10), light.GetTexture());
            TheRenderState.SetTextureClamp(projIdx + 10, (RndRenderState::ClampMode)6);
            TheRenderState.SetBorderColor(projIdx + 10, false);
            TheRenderState.SetTextureFilter(projIdx + 10, (RndRenderState::FilterMode)1, false);
            TheRenderState.SetTextureClamp(projIdx + 5, (RndRenderState::ClampMode)6);
            TheRenderState.SetBorderColor(projIdx + 5, true);
        } else if (blend == 1) {
            TheShaderMgr.SetPConstant(
                (PShaderConstant)(projIdx + 10),
                TheRnd.GetDefaultTex(Rnd::kDefaultTex_WhiteTransparent)
            );
            TheRenderState.SetTextureClamp(projIdx + 5, (RndRenderState::ClampMode)6);
            TheRenderState.SetBorderColor(projIdx + 5, false);
        }
        return true;
    }

    void ClearLightRegisters(int lightIdx) {
        static Vector4 sDefaultLight(0, 0, 0, 1);
        static Vector4 sZeroVec(0, 0, 0, 0);
        TheShaderMgr.SetVConstant((VShaderConstant)(lightIdx + 0x40), sZeroVec);
        TheShaderMgr.SetPConstant((PShaderConstant)(lightIdx + 0x40), sZeroVec);
        Vector4 v(sDefaultLight.x, sDefaultLight.y, sDefaultLight.z, sDefaultLight.w);
        TheShaderMgr.SetVConstant((VShaderConstant)(lightIdx + 0x43), v);
        Vector4 v2(sDefaultLight.x, sDefaultLight.y, sDefaultLight.z, sDefaultLight.w);
        TheShaderMgr.SetPConstant((PShaderConstant)(lightIdx + 0x43), v2);
    }
}

NgEnviron::NgEnviron()
    : mProjectedBlend(), mNumLightsReal(0), mNumLightsApprox(0), mNumLightsPoint(0),
      mNumLightsProj(0), mHasPointCubeTex(0) {}

void NgEnviron::UpdateApproxLighting(const Vector3 *pos) {
    mNumLightsApprox = 0;
    bool hasLights = !mLightsReal.empty() || !mLightsApprox.empty();
    bool useApprox = UsesApproxLocal() || UsesApproxGlobal();
    if (useApprox && hasLights) {
        static BoxMapLighting sBoxLight;
        static Hmx::Color sBoxResults[6];
        for (int i = 0; i < 6; i++) {
            sBoxResults[i].Set(0, 0, 0);
        }
        if (UsesApproxLocal()) {
            sBoxLight.Clear();
            for (ObjPtrList<RndLight>::iterator it = mLightsApprox.begin();
                 it != mLightsApprox.end(); ++it) {
                if (sBoxLight.QueueLight(*it, 1.0f))
                    mNumLightsApprox++;
            }
            sBoxLight.ApplyQueuedLights(sBoxResults, pos);
        }
        if (UsesApproxGlobal()) {
            unsigned int num = sGlobalLighting.NumQueuedLights();
            if (num > 0) {
                mNumLightsApprox += num;
                sGlobalLighting.ApplyQueuedLights(sBoxResults, pos);
            }
        }
        for (int i = 0; i < 6; i++) {
            Vector4 v(sBoxResults[i].red, sBoxResults[i].green, sBoxResults[i].blue, sBoxResults[i].alpha);
            TheShaderMgr.SetVConstant((VShaderConstant)(kVS_BoxMapLight0 + i), v);
            Vector4 v2(sBoxResults[i].red, sBoxResults[i].green, sBoxResults[i].blue, sBoxResults[i].alpha);
            TheShaderMgr.SetPConstant((PShaderConstant)(kPS_BoxMapLight0 + i), v2);
        }
    } else {
        static Hmx::Color sDefaultColor(0, 0, 0, 0);
        for (int i = 0; i < 6; i++) {
            Vector4 v(sDefaultColor.red, sDefaultColor.green, sDefaultColor.blue, sDefaultColor.alpha);
            TheShaderMgr.SetVConstant((VShaderConstant)(kVS_BoxMapLight0 + i), v);
            Vector4 v2(sDefaultColor.red, sDefaultColor.green, sDefaultColor.blue, sDefaultColor.alpha);
            TheShaderMgr.SetPConstant((PShaderConstant)(kPS_BoxMapLight0 + i), v2);
        }
    }
    if (mNumLightsReal > 0) {
        if (mNumLightsApprox <= 1)
            mNumLightsApprox = 1;
    }
}

void NgEnviron::Select(const Vector3 *pos) {
    mNumLightsReal = 0;
    mNumLightsApprox = 0;
    mNumLightsPoint = 0;
    mNumLightsProj = 0;
    mHasPointCubeTex = false;
    mProjectedBlend = (RndLight::ProjectedBlend)0;

    Rnd::DrawMode mode = TheRnd.GetDrawMode();
    if (mode == 4 || mode == 2 || mode == 6 || mode == 3) {
        RndEnviron::Select(pos);
        NgMat::SetCurrent(0);
#ifdef HX_NATIVE
        TheNgStats->mLightsReal += mNumLightsReal;
        TheNgStats->mLightsApprox += mNumLightsApprox;
#endif
        return;
    }

    ReclassifyLights();
    RndEnviron::Select(pos);
    NgMat::SetCurrent(0);
#ifdef HX_NATIVE
    TheNgStats->mLightsApprox += mNumLightsApprox;
    TheNgStats->mLightsReal += mNumLightsReal;
#endif
}
