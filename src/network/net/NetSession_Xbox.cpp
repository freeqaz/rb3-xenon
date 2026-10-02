// XboxSession and XSessionData (retail RTTI .?AVXboxSession@@ vtables
// 0x82058434 / 0x8205848C, .?AVXSessionData@@ vtable 0x820582A4).
// Retail .text from 0x823EEAB8; the XboxSession code continues past 0x823EF5F8.
#include "net/NetSession.h"
#include "net/XSessionData.h"

// ---------------------------------------------------------------------------
// XSessionData. Retail sizeof 0x50 (SessionData::New). Its vtable slot 0 is
// the shared SessionData deleting dtor at 0x823EEAB8: XSessionData's own is
// identical once the dead derived-vptr store goes, and ICF folds it there.

// 0x823EF2C8
void XSessionData::CopyInto(SessionData *data) {
    XSessionData *other = dynamic_cast<XSessionData *>(data);
    memcpy(&other->mInfo, &mInfo, sizeof(XSESSION_INFO));
    other->mNonce = mNonce;
}

// 0x823EF4D8
void XSessionData::Save(BinStream &bs) const {
    bs << mNonce;
    bs.Write(&mInfo, sizeof(XSESSION_INFO));
}

// 0x823EF538
void XSessionData::Load(BinStream &bs) {
    bs >> mNonce;
    bs.Read(&mInfo, sizeof(XSESSION_INFO));
}

// 0x823EF338: the session info alone decides it; the nonce is not compared.
bool XSessionData::Equals(const SessionData *data) const {
    const XSessionData *other = dynamic_cast<const XSessionData *>(data);
    return memcmp(&mInfo, &other->mInfo, sizeof(XSESSION_INFO)) == 0;
}

// 0x823EF498
SessionData *SessionData::New() { return new XSessionData(); }
