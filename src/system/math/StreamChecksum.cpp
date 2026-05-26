#include "math/StreamChecksum.h"
#include "math/SHA1.h"
#include "math/FileChecksum.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"

void StreamChecksum::Begin() {
    if (mState == 1)
        End();
    mSHA1.~CSHA1();
    mState = 1;
}

void StreamChecksum::End() {
    if (mState == 0 || mState == 2)
        return;
    mSHA1.Final();
    mState = 2;
}

void StreamChecksum::Update(const unsigned char *data, unsigned int ui) {
    if (ui != 0) {
        switch (mState) {
        case 0:
            Begin();
            break;
        case 2:
            MILO_FAIL(
                "Attempted to update a StreamChecksum that has already been finalized.  After calling End (or GetHash), you need to call Begin to restart the checksum."
            );
            Begin();
            break;
        default:
            break;
        }
        MILO_ASSERT(data, 0x3A);
        mSHA1.Update(data, ui);
    }
}

void StreamChecksum::GetHash(unsigned char *uc) {
    End();
    mSHA1.CopyDigest(uc);
}

bool StreamChecksumValidator::Begin(const char *file, bool b) {
    MILO_ASSERT(file, 0x6B);
    mFile = file;
    return SetFileChecksum(b);
}

void StreamChecksumValidator::Update(const unsigned char *uc, unsigned int ui) {
    mStreamChecksum.Update(uc, ui);
}

void StreamChecksumValidator::End() { mStreamChecksum.End(); }

bool StreamChecksumValidator::Validate() {
    unsigned char buf[20] = { 0 };
    mStreamChecksum.GetHash(buf);
    return ValidateChecksum(buf);
}

void StreamChecksumValidator::HandleError(const char *c) {
    MILO_LOG(c);
    ThePlatformMgr.SetDiskError(kFailedChecksum);
}

bool StreamChecksumValidator::SetFileChecksum(bool b) {
    if (*mFile == '.') {
        mSignature = GetFileChecksum(mFile, b);
    } else
        mSignature = GetFileChecksum(MakeString("./%s", mFile), b);
    if (!mSignature) {
        HandleError(MakeString("No checksum found for file %s\n", mFile));
    }
    return mSignature != 0;
}

bool StreamChecksumValidator::ValidateChecksum(const unsigned char *hash) {
    MILO_ASSERT(mSignature && hash, 0x9B);

    bool b2 = (mSignature && hash) && !memcmp(mSignature, hash, 0x14);
    if (!b2)
        HandleError(MakeString("Checksum failure for file %s\n", mFile));
    return b2;
}
