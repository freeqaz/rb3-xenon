#include "xdk/LIBCMT/vectorintrinsics.h"

#ifdef __cplusplus
extern "C" {
#endif

void XMScalarSinCos(float *pSin, float *pCos, float Value);

XMMATRIX XMMatrixRotationX(float Angle) {
    float fSin, fCos;
    XMMATRIX M;

    XMScalarSinCos(&fSin, &fCos, Angle);

    M.m[0][0] = 1.0f;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = fCos;
    M.m[1][2] = fSin;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = -fSin;
    M.m[2][2] = fCos;
    M.m[2][3] = 0.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = 0.0f;
    M.m[3][3] = 1.0f;

    return M;
}

#ifdef __cplusplus
}
#endif
