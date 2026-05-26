#include "math/Key.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "math/Rot.h"

void InterpTangent(
    const Vector3 &v1,
    const Vector3 &v2,
    const Vector3 &v3,
    const Vector3 &v4,
    float f,
    Vector3 &vout
) {
    float fsq = f * f;
    float f6 = f * 6.0f;
    float fsq3 = fsq * 3.0f;
    float f4 = f * 4.0f;

    float a = fsq * 6.0f - f6;
    float b = fsq3 - f4 + 1.0f;
    float c = f6 - fsq * 6.0f;
    float d = fsq3 - f * 2.0f;

    Scale(v1, a, vout);
    Vector3 vtmp;
    Scale(v2, b, vtmp);
    Add(vout, vtmp, vout);
    Scale(v3, c, vtmp);
    Add(vout, vtmp, vout);
    Scale(v4, d, vtmp);
    Add(vout, vtmp, vout);
}

void SplineTangent(const Keys<Vector3, Vector3> &keys, int i, Vector3 &vout) {
    int size = keys.size();
    MILO_ASSERT(size > 1, 0x17);
    if (size == 2) {
        Subtract(keys[1].value, keys[0].value, vout);
    } else if (i <= 0) {
        Subtract(keys[1].value, keys[0].value, vout);
        Scale(vout, 1.5f, vout);
        Vector3 vtmp;
        Subtract(keys[2].value, keys[0].value, vtmp);
        Scale(vtmp, 0.25f, vtmp);
        Subtract(vout, vtmp, vout);
    } else if (i >= size - 1) {
        Subtract(keys[size - 1].value, keys[size - 2].value, vout);
        Scale(vout, 1.5f, vout);
        Vector3 vtmp;
        Subtract(keys[size - 1].value, keys[size - 3].value, vtmp);
        Scale(vtmp, 0.25f, vtmp);
        Subtract(vout, vtmp, vout);
    } else {
        Subtract(keys[i + 1].value, keys[i - 1].value, vout);
        Scale(vout, 0.5f, vout);
    }
}

void InterpVector(
    const Keys<Vector3, Vector3> &keys,
    const Key<Vector3> *prev,
    const Key<Vector3> *next,
    float ref,
    bool spline,
    Vector3 &vref,
    Vector3 *vptr
) {
    if (keys.size() < 3) {
        spline = false;
        if (keys.size() < 2) {
            if (vptr)
                vptr->Set(0.0f, 1.0f, 0.0f);
            if (keys.size() != 0)
                vref = prev->value;
            else
                vref.Set(0, 0, 0);
            return;
        }
    }
    int idx = prev - keys.begin();
    if (spline) {
        float fsq = ref * ref;
        float fcubed = fsq * ref;
        float fsq3 = fsq * 3.0f;
        Scale(prev->value, (fcubed * 2.0f - fsq3) + 1.0f, vref);
        Vector3 v70;
        SplineTangent(keys, idx, v70);
        Vector3 v7c;
        Vector3 v88;
        Scale(v70, ref + fsq * -2.0f + fcubed, v88);
        Add(vref, v88, vref);
        Scale(next->value, fcubed * -2.0f + fsq3, v88);
        Add(vref, v88, vref);
        SplineTangent(keys, idx + 1, v7c);
        Scale(v7c, fcubed - fsq, v88);
        Add(vref, v88, vref);
        if (vptr) {
            InterpTangent(prev->value, v70, next->value, v7c, ref, *vptr);
        }
    } else {
        Interp(prev->value, next->value, ref, vref);
        if (vptr) {
            if (idx == keys.size() - 1) {
                idx--;
            }
            Subtract(keys[idx + 1].value, keys[idx].value, *vptr);
        }
    }
}

void InterpVector(
    const Keys<Vector3, Vector3> &keys,
    bool spline,
    float frame,
    Vector3 &vref,
    Vector3 *vptr
) {
    const Key<Vector3> *prev;
    const Key<Vector3> *next;
    float ref;
    keys.AtFrame(frame, prev, next, ref);
    InterpVector(keys, prev, next, ref, spline, vref, vptr);
}

void QuatSpline(
    const Keys<Hmx::Quat, Hmx::Quat> &keys,
    const Key<Hmx::Quat> *prev,
    const Key<Hmx::Quat> *next,
    float ref,
    Hmx::Quat &qout
) {
    MILO_ASSERT(keys.size(), 0x9B);
    if (prev == next) {
        qout = prev->value;
    } else {
        int idx = prev - &keys.front();
        Hmx::Quat prevQuat = prev->value;
        Hmx::Quat nextQuat = next->value;
        Hmx::Quat q88 = idx == 0 ? prevQuat : keys[idx - 1].value;
        Hmx::Quat q58 = idx + 1 == keys.size() - 1 ? nextQuat : keys[idx + 2].value;
        NormalizeTo(q88, prevQuat);
        NormalizeTo(nextQuat, prevQuat);
        NormalizeTo(prevQuat, q58);
        int i = 0;
        while (i < 4) {
            float prev_val = prevQuat[i];
            float next_val = nextQuat[i];
            float prev_prev_val = q88[i];
            float next_next_val = q58[i];

            float twice_prev = prev_val * 2.0f;
            float five_prev = prev_val * 5.0f;
            float three_next = next_val * 3.0f;
            float four_next = next_val * 4.0f;

            float difference = next_val - prev_prev_val;
            float c1 = difference * 0.5f;
            float c3 = (three_next - prev_prev_val) - difference;
            float c2 = (four_next - five_prev + twice_prev) - next_next_val;

            qout[i] = ((c3 * ref + c2) * ref + c1) * ref + twice_prev;
            i++;
        }
        Normalize(qout, qout);
    }
}
