#include "rndobj/BoxMap.h"
#include "math/Utl.h"
#include "os/Timer.h"
#include "rndobj/Lit.h"

static int gLightIndex = 0;
static Hmx::Color gLightBuffer1[150];
static Hmx::Color gLightBuffer2[150];

BoxMapLighting::BoxMapLighting() { Clear(); }

void BoxMapLighting::Clear() {
    mQueued_Directional.Clear();
    mQueued_Point.Clear();
    mQueued_Spot.Clear();
}

bool BoxMapLighting::QueueLight(RndLight *light, float colorScale) {
    if (light->Showing()) {
        Hmx::Color lightColor(light->GetColor());
        lightColor.red *= colorScale;
        lightColor.green *= colorScale;
        lightColor.blue *= colorScale;
        switch (light->GetType()) {
        case RndLight::kDirectional:
        case RndLight::kFakeSpot:
            LightParams_Directional *paramsDirectional;
            if (ParamsAt(paramsDirectional)) {
                paramsDirectional->mColor = lightColor;
                Negate(light->WorldXfm().m.y, paramsDirectional->mDirection);
                return true;
            }
            break;
        case RndLight::kPoint:
            LightParams_Point *paramsPoint;
            if (ParamsAt(paramsPoint)) {
                paramsPoint->mPosition = light->WorldXfm().v;
                paramsPoint->mColor = lightColor;
                paramsPoint->mRange = light->Range();
                paramsPoint->mFalloffStart = light->FalloffStart();
                return true;
            }
            break;
        default:
            break;
        }
    }
    return false;
}

void BoxMapLighting::ApplyQueuedLights(Hmx::Color * __restrict color, const Vector3 *v3) const {
    START_AUTO_TIMER("draw_light_approx");
    gLightIndex = 0;
    if (v3) {
        ApplyLight(mQueued_Spot, *v3);
        ApplyLight(mQueued_Point, *v3);
    }
    unsigned int idx = gLightIndex;
    ApplyLight(mQueued_Directional);

    if (idx != 0) {
        float c0r = color[0].red;
        float c0g = color[0].green;
        float c0b = color[0].blue;
        float c4r = color[1].red;
        float c4g = color[1].green;
        float c4b = color[1].blue;
        float c8r = color[2].red;
        float c8g = color[2].green;
        float c8b = color[2].blue;
        float c12r = color[3].red;
        float c12g = color[3].green;
        float c12b = color[3].blue;
        float c16r = color[4].red;
        float c16g = color[4].green;
        float c16b = color[4].blue;
        float c20r = color[5].red;
        float c20g = color[5].green;
        float c20b = color[5].blue;

        float *lightBuf1 = (float *)&gLightIndex;
        float *lightBuf2 = (float *)gLightBuffer2 - 2;
        for (unsigned int counter = idx; counter != 0; counter--) {
            float x1 = lightBuf1[2];
            float y1 = lightBuf1[3];
            lightBuf1 += 4;
            float z1 = *lightBuf1;

            float x2 = lightBuf2[2];
            float y2 = lightBuf2[3];
            lightBuf2 += 4;
            float z2 = *lightBuf2;

            float abs_nx1 = (-z1 >= 0.0f) ? -z1 : 0.0f;
            float abs_ny1 = (-x1 >= 0.0f) ? -x1 : 0.0f;
            float abs_nz1 = (-y1 >= 0.0f) ? -y1 : 0.0f;
            float abs_nw1 = (z1 >= 0.0f) ? z1 : 0.0f;
            float abs_nx2 = (x1 >= 0.0f) ? x1 : 0.0f;
            float abs_ny2 = (y1 >= 0.0f) ? y1 : 0.0f;

            float sq1 = abs_nx1 * abs_nx1;
            float sq2 = abs_ny1 * abs_ny1;
            float sq3 = abs_nz1 * abs_nz1;
            float sq4 = abs_nw1 * abs_nw1;
            float sq5 = abs_nx2 * abs_nx2;
            float sq6 = abs_ny2 * abs_ny2;

            c16b += sq1 * z2;
            c0b += sq2 * z2;
            c8b += sq3 * z2;
            c0r += sq2 * x2;
            c0g += sq2 * y2;
            c8r += sq3 * x2;
            c8g += sq3 * y2;
            c4b += sq4 * z2;
            c12b += sq5 * z2;
            c20g = sq6 * y2 + c20g;
            c20b = sq6 * z2 + c20b;
            c16r += sq1 * x2;
            c4r += sq4 * x2;
            c4g += sq4 * y2;
            c12r += sq5 * x2;
            c12g += sq5 * y2;
            c20r = sq6 * x2 + c20r;
            c16g = sq1 * y2 + c16g;
        }

        color[0].red = c0r;
        color[0].green = c0g;
        color[0].blue = c0b;
        color[1].red = c4r;
        color[1].green = c4g;
        color[1].blue = c4b;
        color[2].red = c8r;
        color[2].green = c8g;
        color[2].blue = c8b;
        color[3].red = c12r;
        color[3].green = c12g;
        color[3].blue = c12b;
        color[4].red = c16r;
        color[4].green = c16g;
        color[4].blue = c16b;
        color[5].red = c20r;
        color[5].green = c20g;
        color[5].blue = c20b;
    }
}

bool BoxMapLighting::CacheData(LightParams_Spot &spot) {
    if (spot.mBeamLength > 0) {
        if (spot.mBottomRadius >= spot.mTopRadius
            && (spot.mColor.red > 0.003921569f || spot.mColor.green > 0.003921569f
                || spot.mColor.blue > 0.003921569f)) {
            float f3 = (spot.mTopRadius * spot.mBeamLength)
                / (spot.mBottomRadius - spot.mTopRadius);
            Vector3 v58;
            Scale(spot.mDirection, f3, v58);
            Vector3 v4c;
            Subtract(spot.mPosition, v58, v4c);
            float f1 = spot.mBottomRadius / (spot.mBeamLength + f3);
            f1 *= f1;
            float f2 = 1.0f / (spot.mBeamLength * 2.0f);
            f1 = (1.0f - f1) / (f1 + 1.0f);
            spot.mApex = v4c;
            spot.mConeAngleFactor = f1;
            spot.mConeAngleInverse = 1.0f / (1.0f - f1);
            spot.mHalfLengthRecip = f2;
            spot.mOffsetFactor = f3 * f2;
            return true;
        }
    }
    mQueued_Spot.RemoveEntry();
    return false;
}

void BoxMapLighting::ApplyLight(
    const BoxLightArray<LightParams_Directional, 50> &arr
) const {
    int idx = gLightIndex;
    for (unsigned int i = 0; i < arr.NumElements(); i++) {
        const Hmx::Color *src = (const Hmx::Color *)&arr[i];
        gLightBuffer1[idx] = src[0];
        gLightBuffer2[idx] = src[1];
        idx++;
    }
    gLightIndex = idx;
}

void BoxMapLighting::ApplyLight(
    const BoxLightArray<LightParams_Point, 50> &arr, const Vector3 &viewPos
) const {
    int idx = gLightIndex;
    for (unsigned int i = 0; i < arr.NumElements(); i++) {
        const LightParams_Point &light = arr[i];
        if (light.mFalloffStart < light.mRange) {
            float dx = light.mPosition.x - viewPos.x;
            float dy = light.mPosition.y - viewPos.y;
            float dz = light.mPosition.z - viewPos.z;
            float *buf1 = (float *)&gLightBuffer1[idx];
            buf1[0] = dx;
            buf1[1] = dy;
            buf1[2] = dz;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (0.0f < distSq) {
                float invDist = 1.0f / sqrtf(distSq);
                float dist = Max(0.0f, invDist * distSq - light.mFalloffStart);
                float atten = Max(0.0f, 1.0f - dist / (light.mRange - light.mFalloffStart));
                gLightBuffer2[idx].red = light.mColor.red * atten;
                gLightBuffer2[idx].green = light.mColor.green * atten;
                gLightIndex = idx + 1;
                buf1[2] = dz * invDist;
                gLightBuffer2[idx].blue = light.mColor.blue * atten;
                buf1[0] = dx * invDist;
                buf1[1] = dy * invDist;
                idx++;
            }
        }
    }
}

void BoxMapLighting::ApplyLight(
    const BoxLightArray<LightParams_Spot, 50> &arr, const Vector3 &viewPos
) const {
    int idx = gLightIndex;
    for (unsigned int i = 0; i < arr.NumElements(); i++) {
        const LightParams_Spot &light = arr[i];
        float dz = viewPos.z - light.mApex.z;
        float dx = viewPos.x - light.mApex.x;
        float dy = viewPos.y - light.mApex.y;
        float distSq = dy * dy + dx * dx + dz * dz;
        float invDist = 1.0f / sqrtf(distSq);
        float dist = invDist * distSq * light.mHalfLengthRecip - light.mOffsetFactor;
        float cone = light.mDirection.y * dy * invDist
            + light.mDirection.x * dx * invDist + light.mDirection.z * dz * invDist;
        dist = Min(1.0f, dist);
        float coneClamped = Min(1.0f, cone) - light.mConeAngleFactor;
        float distAtten = Max(0.0f, 1.0f - dist);
        float coneAtten = Max(0.0f, coneClamped);
        float atten = distAtten * coneAtten * light.mConeAngleInverse;
        gLightBuffer2[idx].red = atten * light.mColor.red;
        gLightBuffer2[idx].green = atten * light.mColor.green;
        gLightBuffer2[idx].blue = atten * light.mColor.blue;
        gLightBuffer1[idx].red = -(dx * invDist);
        gLightBuffer1[idx].green = -(dy * invDist);
        gLightBuffer1[idx].blue = -(dz * invDist);
        gLightIndex = idx + 1;
        idx++;
    }
}
