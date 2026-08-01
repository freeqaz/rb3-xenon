#pragma once
#include "os/Debug.h"
#include "os/File.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/ShaderProgram.h"
#include "utl/MemMgr.h"
#include "xdk/D3DX9.h"
#include "xdk/XAPILIB.h"
#include "xdk/d3dx9/d3dx9mesh.h"

class DxShaderBuffer : public RndShaderBuffer {
public:
    DxShaderBuffer() : mBuffer(0) {}
    DxShaderBuffer(unsigned int numBytes) {
        if (numBytes != 0) {
            HRESULT res = D3DXCreateBuffer(numBytes, &mBuffer);
            if (res != ERROR_SUCCESS) {
                MILO_FAIL("File: %s Line: %d Error: %X\n", __FILE__, 0x65, res);
            }
        } else {
            mBuffer = nullptr;
        }
    }
    virtual ~DxShaderBuffer() {
        if (mBuffer) {
            mBuffer->Release();
            mBuffer = nullptr;
        }
    }
    virtual void *Storage() {
        if (mBuffer) {
            return mBuffer->GetBufferPointer();
        } else
            return nullptr;
    }
    virtual unsigned int Size() const {
        if (mBuffer) {
            return mBuffer->GetBufferSize();
        } else
            return 0;
    }

private:
    ID3DXBuffer *mBuffer; // 0x4
};

class DxShaderInclude : public ID3DXInclude {
public:
    virtual HRESULT Open(
        D3DXINCLUDE_TYPE IncludeType,
        LPCSTR pFileName,
        LPCVOID pParentData,
        LPCVOID *ppData,
        UINT *pBytes,
        LPSTR pFullPath,
        DWORD cbFullPath
    ) {
        String str(ShaderSourcePath(pFileName));
        if (pFullPath && cbFullPath != 0) {
            strncpy(pFullPath, str.c_str(), cbFullPath - 1);
            pFullPath[cbFullPath - 1] = '\0';
        }
        File *file = NewFile(str.c_str(), FILE_OPEN_NOARK | FILE_OPEN_READ);
        if (!file) {
            // Retail materializes str as a BY-VALUE MiloStripEval param here
            // (copy-ctor + dtor pair) rather than MILO_NOTIFY's discarded
            // comma-form; see CharBoneDir.cpp:277 for the mechanism.
            // MILO_NOTIFY itself must stay comma-form globally.
            MiloStripEval("Could not find shader file '%s'.", str);
            return ERROR_FILE_NOT_FOUND;
        } else {
            *pBytes = file->Size();
            // Retail homes the size into a 4-byte stack local that it stores
            // ONCE and NEVER reloads (target: `stw r3,0x50(r31)`, sitting
            // between `stw r3,0(r28)` and the MemAlloc call).  That local also
            // pushes both String locals +8 and grows the frame 0xa0 -> 0xb0,
            // which is additionally what lets the String-dtor EH funclet
            // (fn_827362B0) match -- this local is worth +2, not +1.
            //
            // A compiler only emits a dead store to a non-escaping local when
            // the store is volatile-qualified.  Measured here at /O1: a plain
            // dead `int` (whether or not it also fed MemAlloc), a 1-element
            // array element, a one-int struct field, and an address escaped
            // into an inlined-away MiloStripEval were ALL dead-store-eliminated
            // and left the frame at 0xa0 -- byte-identical to omitting the
            // local entirely (98.348%).  `volatile` is the only spelling that
            // reproduces the artifact.
            //
            // It must come AFTER the *pBytes store: initialising it from
            // file->Size() instead forces a volatile RELOAD to feed *pBytes and
            // inserts an extra `lwz` (98.303%).  This ordering is byte-exact.
            volatile UINT size = *pBytes;
            *ppData = MemAlloc(*pBytes, __FILE__, 0x44, "shader compile buffer");
            file->Read((void *)*ppData, *pBytes);
            delete file;
            return ERROR_SUCCESS;
        }
    }

    virtual HRESULT Close(LPCVOID pData) {
        MemFree((void *)pData, __FILE__, 0x4D, "shader compile buffer");
        return ERROR_SUCCESS;
    }
    virtual HRESULT Open(
        D3DXINCLUDE_TYPE IncludeType,
        LPCSTR pFileName,
        LPCVOID pParentData,
        LPCVOID *ppData,
        UINT *pBytes
    ) {
        return Open(IncludeType, pFileName, pParentData, ppData, pBytes, nullptr, 0);
    }
};
