#include "os/ProfilePicture.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Tex.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/xapilibi/winerror.h"
#include "xdk/xapilibi/xbox.h"

// The gamer picture is read straight into a linear 64x64 ARGB texture, then
// copied into an RndTex once the overlapped read completes.

ProfilePicture::ProfilePicture(int padNum, Hmx::Object *callback)
    : mState(kIdle), mOverlapped(0), mTexture(0), mUserPicture(0), mPadNum(padNum),
      mCallback(callback) {}

void ProfilePicture::FetchUserPicture() {
    mOverlapped = new XOVERLAPPED;
    memset(mOverlapped, 0, sizeof(XOVERLAPPED));
    HRESULT hr = IDirect3DDevice9_CreateTexture(
        nullptr, 64, 64, 1, 0, D3DFMT_LIN_A8R8G8B8, 0, &mTexture, nullptr
    );
    if (SUCCEEDED(hr)) {
        D3DLOCKED_RECT rect = {};
        mTexture->LockRect(0, &rect, nullptr, 0);
        memset(rect.pBits, 0, rect.Pitch * 64);
        DWORD result = XUserReadGamerPicture(
            mPadNum, false, (BYTE *)rect.pBits, rect.Pitch, 64, mOverlapped
        );
        if (result != ERROR_IO_PENDING) {
            mTexture->UnlockRect(0);
            if (mTexture) {
                mTexture->Release();
                mTexture = nullptr;
            }
            delete mOverlapped;
            mOverlapped = nullptr;
            Fail();
        }
    }
}

bool ProfilePicture::ReceiveUserPicture() {
    bool received = false;
    if (mOverlapped->InternalLow != ERROR_IO_PENDING) {
        DWORD result = XGetOverlappedResult(mOverlapped, nullptr, false);
        if (result != ERROR_IO_INCOMPLETE) {
            if (result == 0) {
                received = true;
            } else {
                Fail();
            }
            mTexture->UnlockRect(0);
            D3DLOCKED_RECT rect = {};
            mTexture->LockRect(0, &rect, nullptr, D3DLOCK_READONLY);
            RndBitmap bitmap;
            bitmap.Create(64, 64, 0, 32, 0, nullptr, rect.pBits, nullptr);
            mTexture->UnlockRect(0);
            RELEASE(mUserPicture);
            mUserPicture = Hmx::Object::New<RndTex>();
            mUserPicture->SetBitmap(bitmap, nullptr, true);
            delete mOverlapped;
            mOverlapped = nullptr;
            if (mTexture) {
                mTexture->Release();
                mTexture = nullptr;
            }
        }
    }
    return received;
}

void ProfilePicture::FetchUserData() {}

bool ProfilePicture::ReceiveUserData() { return true; }
