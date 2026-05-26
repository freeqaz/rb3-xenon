#pragma once
#include "xdk/D3D9.h"
#include "xdk/XAPILIB.h"
#include "xdk/win_types.h"
#include "xdk/unknwn.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID3DXBuffer : public IUnknown { /* Size=0x4 */
    /* 0x0000: fields for IUnknown */

    virtual HRESULT QueryInterface(const _GUID &riid, void **ppvObject) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual LPVOID GetBufferPointer();
    virtual DWORD GetBufferSize();

    //   public: ID3DXBuffer(const ID3DXBuffer&);
    //   public: ID3DXBuffer();
    //   public: ID3DXBuffer& operator=(const ID3DXBuffer&);
};

HRESULT D3DXCreateBuffer(DWORD NumBytes, ID3DXBuffer **ppBuffer);

#ifdef __cplusplus
}
#endif
