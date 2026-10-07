#include "rndobj/BoxMap.h"
#include "math/Utl.h"
#include "os/Timer.h"
#include "rndobj/Lit.h"

#ifdef HX_NATIVE
// The X360 reciprocal-square-root estimate; the host has no such instruction,
// so give it the exact value the estimate approximates.
static inline double __frsqrte(double x) { return 1.0 / std::sqrt(x); }
#else
double __frsqrte(double);
#endif

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
    ApplyLight(mQueued_Directional);
    // Read after the directional pass: the image reloads gLightIndex here.
    unsigned int idx = gLightIndex;

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

        float *lightBuf1 = (float *)gLightBuffer1 - 2;
        float *lightBuf2 = (float *)gLightBuffer2 - 2;
        for (unsigned int i = 0; i < idx; i++) {
            float x1 = lightBuf1[2];
            float y1 = lightBuf1[3];
            lightBuf1 += 4;
            float z1 = *lightBuf1;

            float x2 = lightBuf2[2];
            float y2 = lightBuf2[3];
            lightBuf2 += 4;
            float z2 = *lightBuf2;

            // One weight per cube face: +x, -x, +y, -y, +z, -z, each the
            // squared positive part of the light direction along that face.
            float wPosX = Max(0.0f, x1);
            float wNegX = Max(0.0f, -x1);
            float wPosY = Max(0.0f, y1);
            float wNegY = Max(0.0f, -y1);
            float wPosZ = Max(0.0f, z1);
            float wNegZ = Max(0.0f, -z1);
            wPosX *= wPosX;
            wNegX *= wNegX;
            wPosY *= wPosY;
            wNegY *= wNegY;
            wPosZ *= wPosZ;
            wNegZ *= wNegZ;

            c0r += wPosX * x2;
            c0g += wPosX * y2;
            c0b += wPosX * z2;
            c4r += wNegX * x2;
            c4g += wNegX * y2;
            c4b += wNegX * z2;
            c8r += wPosY * x2;
            c8g += wPosY * y2;
            c8b += wPosY * z2;
            c12r += wNegY * x2;
            c12g += wNegY * y2;
            c12b += wNegY * z2;
            c16r += wPosZ * x2;
            c16g += wPosZ * y2;
            c16b += wPosZ * z2;
            c20r += wNegZ * x2;
            c20g += wNegZ * y2;
            c20b += wNegZ * z2;
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
    for (unsigned int i = 0; i < arr.NumElements(); i++) {
        const Hmx::Color *src = (const Hmx::Color *)&arr[i];
        gLightBuffer1[gLightIndex] = src[0];
        gLightBuffer2[gLightIndex] = src[1];
        gLightIndex++;
    }
}

void BoxMapLighting::ApplyLight(
    const BoxLightArray<LightParams_Point, 50> &arr, const Vector3 &viewPos
) const {
    for (unsigned int i = 0; i < arr.NumElements(); i++) {
        const LightParams_Point &light = arr[i];
        if (light.mRange > light.mFalloffStart) {
            // Retail (BoxMap ApplyLight<Point>) stores dir.red, re-reads it for
            // distSq, and scales the three components only after the colour
            // stores; it computes dy, then dx, then dz.
            Hmx::Color &dir = gLightBuffer1[gLightIndex];
            float dy = light.mPosition.y - viewPos.y;
            dir.red = light.mPosition.x - viewPos.x;
            float dz = light.mPosition.z - viewPos.z;
            dir.green = dy;
            dir.blue = dz;
            float distSq = dy * dy + dir.red * dir.red + dz * dz;
            if (distSq > 0.0f) {
                // The raw estimate, no Newton step: frsqrte then frsp.
                float invDist = __frsqrte(distSq);
                float dist = Max(0.0f, invDist * distSq - light.mFalloffStart);
                float atten = Max(
                    0.0f, 1.0f - dist / (light.mRange - light.mFalloffStart)
                );
                Hmx::Color &col = gLightBuffer2[gLightIndex];
                col.red = light.mColor.red * atten;
                col.green = light.mColor.green * atten;
                col.blue = light.mColor.blue * atten;
                dir.red = dir.red * invDist;
                dir.green = dy * invDist;
                dir.blue = dz * invDist;
                gLightIndex++;
            }
        }
    }
}

void BoxMapLighting::ApplyLight(
    const BoxLightArray<LightParams_Spot, 50> &arr, const Vector3 &viewPos
) const {
    for (unsigned int i = 0; i < arr.NumElements(); i++) {
        const LightParams_Spot &light = arr[i];
        float dy = viewPos.y - light.mApex.y;
        float dz = viewPos.z - light.mApex.z;
        float dx = viewPos.x - light.mApex.x;
        float distSq = dz * dz + dx * dx + dy * dy;
        // The raw estimate, no Newton step: frsqrte then frsp.
        float invDist = __frsqrte(distSq);
        dz *= invDist;
        dx *= invDist;
        dy *= invDist;
        float dist = invDist * distSq * light.mHalfLengthRecip - light.mOffsetFactor;
        float cone = light.mDirection.z * dz + light.mDirection.x * dx
            + light.mDirection.y * dy;
        dist = Min(1.0f, dist);
        float coneClamped = Min(cone, 1.0f) - light.mConeAngleFactor;
        float distAtten = Max(0.0f, 1.0f - dist);
        float coneAtten = Max(0.0f, coneClamped);
        float atten = distAtten * (light.mConeAngleInverse * coneAtten);
        gLightBuffer2[gLightIndex].red = atten * light.mColor.red;
        gLightBuffer2[gLightIndex].green = atten * light.mColor.green;
        gLightBuffer2[gLightIndex].blue = atten * light.mColor.blue;
        gLightBuffer1[gLightIndex].red = -dx;
        gLightBuffer1[gLightIndex].green = -dy;
        gLightBuffer1[gLightIndex].blue = -dz;
        gLightIndex++;
    }
}
