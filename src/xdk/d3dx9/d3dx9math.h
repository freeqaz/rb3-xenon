#pragma once
#include "xdk/D3D9.h"
#include "xdk/XAPILIB.h"
#include "xdk/win_types.h"
#include "xdk/unknwn.h"

#ifdef __cplusplus
extern "C" {
#endif

struct D3DXFLOAT16 { /* Size=0x2 */
    /* 0x0000 */ WORD value;
};

struct D3DXVECTOR4 { /* Size=0x10 */
    /* 0x0000 */ float x;
    /* 0x0004 */ float y;
    /* 0x0008 */ float z;
    /* 0x000c */ float w;

    D3DXVECTOR4(float, float, float, float);
    D3DXVECTOR4(const D3DXFLOAT16 *);
    D3DXVECTOR4(const float *);
    D3DXVECTOR4();
    operator float *();
    operator const float *() const;
    D3DXVECTOR4 &operator=(const D3DXVECTOR4 &);
    D3DXVECTOR4 &operator+=(const D3DXVECTOR4 &);
    D3DXVECTOR4 &operator-=(const D3DXVECTOR4 &);
    D3DXVECTOR4 &operator*=(float);
    D3DXVECTOR4 &operator/=(float);
    D3DXVECTOR4 operator+(const D3DXVECTOR4 &) const;
    D3DXVECTOR4 operator+() const;
    D3DXVECTOR4 operator-(const D3DXVECTOR4 &) const;
    D3DXVECTOR4 operator-() const;
    D3DXVECTOR4 operator*(float) const;
    D3DXVECTOR4 operator/(float) const;
    BOOL operator==(const D3DXVECTOR4 &) const;
    BOOL operator!=(const D3DXVECTOR4 &) const;
};

struct D3DXMATRIX : public _D3DMATRIX { /* Size=0x40 */
    /* 0x0000: fields for _D3DMATRIX */

    D3DXMATRIX(
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float,
        float
    );
    D3DXMATRIX(const D3DXFLOAT16 *);
    D3DXMATRIX(const _D3DMATRIX &);
    D3DXMATRIX(const float *);
    D3DXMATRIX();
    float operator()(UINT, UINT) const;
    float &operator()(UINT, UINT);
    operator float *();
    operator const float *() const;
    D3DXMATRIX &operator=(const D3DXMATRIX &);
    D3DXMATRIX &operator*=(float);
    D3DXMATRIX &operator*=(const D3DXMATRIX &);
    D3DXMATRIX &operator+=(const D3DXMATRIX &);
    D3DXMATRIX &operator-=(const D3DXMATRIX &);
    D3DXMATRIX &operator/=(float);
    D3DXMATRIX operator+(const D3DXMATRIX &) const;
    D3DXMATRIX operator+() const;
    D3DXMATRIX operator-(const D3DXMATRIX &) const;
    D3DXMATRIX operator-() const;
    D3DXMATRIX operator*(float) const;
    D3DXMATRIX operator*(const D3DXMATRIX &) const;
    D3DXMATRIX operator/(float) const;
    BOOL operator==(const D3DXMATRIX &) const;
    BOOL operator!=(const D3DXMATRIX &) const;
};

#ifdef __cplusplus
}
#endif
