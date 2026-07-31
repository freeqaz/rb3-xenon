#pragma once
#include "math/SHA1.h"
#include "utl/MemMgr.h"

// RB3 retail's StreamChecksum does NOT embed the portable CSHA1 from math/SHA1.h.
// That class is real (196 bytes, lives in SHA1.cpp, its Update/Final match 100%),
// but StreamChecksum uses the Xbox 360 kernel's XeCrypt SHA-1 instead and keeps
// the finished digest as its own trailing member.  Three independent instruments
// in the retail image agree on this layout:
//   1. StreamChecksum::End()     -> call(r3=this+0x04, r4=this+0x5c), callee is
//      `li r5,0x14; b XeCryptShaFinal`  (thunk fn_82BBE610)
//      StreamChecksum::GetHash() -> memcpy(dst, this+0x5c, 0x14)
//   2. SetFileChecksum/ValidateChecksum reach mSignature/mFile at 0x70/0x74.
//   3. The inlined ctor at `new StreamChecksumValidator()` in FileStream.cpp and
//      BufStream.cpp is `li r3,0x78` + exactly three zero stores (0x0/0x70/0x74)
//      -- so sizeof == 0x78 and the 88-byte SHA state is NOT zero-initialized.
// => sizeof(XeCryptSha)=0x58, mDigest@0x5c, sizeof(StreamChecksum)=0x70.
// The XeCryptShaInit/Update/Final forwarders live in another (unpinned) TU at
// 0x82BBE600, so these are declared-but-not-defined on purpose: that guarantees
// the out-of-line calls retail emits.
#if !HX_NATIVE
class XeCryptSha {
public:
    void Reset();
    void Update(const unsigned char *, unsigned int);
    void Final(unsigned char *);

private:
    unsigned int mCount; // 0x0
    unsigned int mState[5]; // 0x4
    unsigned char mBuffer[64]; // 0x18
}; // 0x58
#endif

class StreamChecksum {
private:
    int mState; // this is an enum - what the state enums are? that's anybody's guess
#if HX_NATIVE
    // Native host has no XeCrypt; keep the portable implementation.  Layout does
    // not need to match retail here.
    CSHA1 mSHA1;
#else
    XeCryptSha mSHA1; // 0x4
#endif
    CSHA1::Digest mDigest; // 0x5c

public:
    StreamChecksum() : mState(0) {}
    ~StreamChecksum() {}
    void Begin();
    void Update(const unsigned char *, unsigned int);
    void End();
    __declspec(noinline) void GetHash(unsigned char *);
};

class StreamChecksumValidator {
private:
    void HandleError(const char *);
    bool SetFileChecksum(bool);
    bool ValidateChecksum(const unsigned char *);

    StreamChecksum mStreamChecksum;
    const unsigned char *mSignature;
    const char *mFile;

public:
    StreamChecksumValidator() : mStreamChecksum(), mSignature(0), mFile(0) {}
    ~StreamChecksumValidator() {}

    MEM_OVERLOAD(StreamChecksumValidator, 0x3D);
    bool Begin(const char *, bool);
    void Update(const unsigned char *, unsigned int);
    void End();
    bool Validate();
};
