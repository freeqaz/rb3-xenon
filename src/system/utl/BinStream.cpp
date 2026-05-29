#include "utl/BinStream.h"
#include "math/Rand.h"
#include "math/Rand2.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/Endian.h"
#include "os/Timer.h"
#include <vector>

#define BUF_SIZE 512

// RB3's BinStream has no per-instance rev stack member (sizeof==0xc). The rev
// stack is process-wide here so PushRev/PopRev still compile while the derived
// stream classes keep the target member offsets. See BinStream.h note.
static std::vector<ObjVersion> *sRevStack = nullptr;

const char *BinStream::Name() const { return "<unnamed>"; }

BinStream::BinStream(bool b) : mLittleEndian(b), mCrypto(nullptr) {}

void SwapData(const void *in, void *out, int size) {
    switch (size) {
    case 2: {
        unsigned short *s1 = (unsigned short *)in;
        unsigned short *s2 = (unsigned short *)out;
        *s2 = EndianSwap(*s1);
        break;
    }
    case 4: {
        unsigned int *i1 = (unsigned int *)in;
        unsigned int *i2 = (unsigned int *)out;
        *i2 = EndianSwap(*i1);
        break;
    }
    case 8: {
        unsigned long long *l1 = (unsigned long long *)in;
        unsigned long long *l2 = (unsigned long long *)out;
        *l2 = EndianSwap(*l1);
        break;
    }
    default:
        MILO_ASSERT(0, 0xAC);
        break;
    }
}

void BinStream::DisableEncryption() {
    MILO_ASSERT(mCrypto, 0xDC);
    RELEASE(mCrypto);
}

void BinStream::Write(const void *void_data, int bytes) {
    if (Fail()) {
        MILO_PRINT_ONCE("Stream error: Can't write to %s\n", Name());
    } else {
        const unsigned char *data = (u8 *)void_data;
        if (!mCrypto) {
            WriteImpl(void_data, bytes);
        } else {
            char crypt[512];
            while (bytes > 0) {
                int x = Min(512, bytes);
                for (int i = 0; i < x; i++) {
                    u8 bastard = mCrypto->Int();
                    crypt[i] = data[i] ^ bastard;
                }
                WriteImpl(crypt, x);
                bytes -= 512;
                data += 512;
            }
        }
    }
}

void BinStream::Seek(int offset, SeekType type) {
    MILO_ASSERT(!Fail(), 0x11F);
    MILO_ASSERT(!mCrypto, 0x122);
    SeekImpl(offset, type);
}

void BinStream::WriteEndian(const void *in, int size) {
#ifdef HX_NATIVE
    // On LE host: swap when file is BE (mLittleEndian=false)
    if (!mLittleEndian) {
#else
    // On BE host: swap when file is LE (mLittleEndian=true)
    if (mLittleEndian) {
#endif
        u64 output[2]; // 128 bits of buffer to swap
        SwapData(in, output, size);
        Write(output, size);
    } else
        Write(in, size);
}

bool BinStream::AddSharedInlined(const class FilePath &) {
    MILO_FAIL("BinStream::AddSharedInlined is a PC dev tool only !!");
    return false;
}

BinStream &BinStream::operator<<(const char *str) {
    MILO_ASSERT(str, 0x60);
    int size = strlen(str);
    *this << size;
    Write(str, size);
    return *this;
}

BinStream &BinStream::operator<<(const Symbol &sym) {
    const char *str = sym.Str();
    unsigned int len = strlen(str);
    MILO_ASSERT(len < BUF_SIZE, 0x6C);
    *this << len;
    Write(str, len);
    return *this;
}

BinStream &BinStream::operator<<(const class String &str) {
    int size = str.length();
    *this << size;
    Write(str.c_str(), size);
    return *this;
}

void BinStream::EnableWriteEncryption() {
    MILO_ASSERT(!mCrypto, 0xC8);
    int i = RandomInt();
    *this << i;
    mCrypto = new Rand2(i);
}

int BinStream::PopRev(Hmx::Object *o) {
    MILO_ASSERT(sRevStack, 0x34);
#ifdef HX_NATIVE
    if (sRevStack->empty()) {
        fprintf(stderr, "PopRev ABORT: empty stack for %s '%s' (stream=%p)\n", o->ClassName(), o->Name(), (void*)this);
        abort();
    }
#endif
    ObjVersion *back = &sRevStack->back();
    while (back->obj == nullptr) {
        MILO_NOTIFY("hey object got deleted!");
        sRevStack->pop_back();
        back = &sRevStack->back();
    }
    int revs = back->revs;
    if (o != back->obj) {
        MILO_LOG("rev stack $this mismatch (%08x != %08x\n", o, back->obj);
        MILO_LOG("curr obj: %s %s\n", o->ClassName(), PathName(o));
        MILO_LOG("stack obj: %s %s\n", back->obj->ClassName(), PathName(back->obj));
        MILO_FAIL(
            "rev stack (%08x %s %s != %08x %s %s)\n",
            o,
            o->ClassName(),
            PathName(o),
            back->obj,
            back->obj->ClassName(),
            PathName(back->obj)
        );
    }
    sRevStack->pop_back();
    return revs;
}

void BinStream::Read(void *data, int bytes) {
    if (Fail()) {
        MILO_NOTIFY_ONCE("Stream error: Can't read from %s", Name());
        memset(data, 0, bytes);
    } else {
        AutoGlitchReport report(50.0f, __FUNCTION__);
        ReadImpl(data, bytes);
        if (mCrypto) {
            for (unsigned char *ptr = (unsigned char *)data;
                 ptr < (unsigned char *)data + bytes;
                 ptr++) {
                unsigned char cryptoInt = mCrypto->Int();
                *ptr ^= cryptoInt;
            }
        }
    }
}

void BinStream::ReadEndian(void *out, int size) {
    Read(out, size);
#ifdef HX_NATIVE
    // On x86_64 (LE host), mLittleEndian=true means file is LE — no swap needed.
    // On Xbox 360 (BE host), mLittleEndian=true means swap LE file data to BE host.
    if (!mLittleEndian) {
        SwapData(out, out, size);
    }
#else
    if (mLittleEndian) {
        SwapData(out, out, size);
    }
#endif
}

void BinStream::ReadString(char *c, int i) {
    unsigned int a;
    *this >> a;
    if (a >= i)
        MILO_FAIL("String chars %d > %d", a + 1, i);
    Read(c, a);
    c[a] = 0;
}

BinStream &BinStream::operator>>(Symbol &sym) {
    char buf[BUF_SIZE];
    ReadString(buf, BUF_SIZE);
    sym = buf;
    return *this;
}
BinStream &BinStream::operator>>(String &str) {
    int siz;
    *this >> siz;
#ifdef HX_NATIVE
    if (siz > 10000 || siz < 0) {
        fprintf(stderr, "BinStream::operator>>(String) ABORT: bad size=%d at stream pos=%d\n", siz, Tell());
        abort();
    }
#endif
    str.resize(siz);
    Read((void *)str.c_str(), siz);
    return *this;
}

void BinStream::EnableReadEncryption() {
    MILO_ASSERT(!mCrypto, 0xC0);
    int i;
    *this >> i;
    mCrypto = new Rand2(i);
}

BinStream::~BinStream() {
    delete mCrypto;
}

#ifdef HX_NATIVE
bool BinStream::WaitUntilReady(int sleepMs) {
    for (int polls = 0; ; polls++) {
        EofType eof = Eof();
        if (eof == NotEof)
            return true;
        if (eof == RealEof) {
            MILO_WARN("BinStream::WaitUntilReady: unexpected end of file "
                       "(stream: %s)", Name());
            return false;
        }
#ifdef __EMSCRIPTEN__
        // Safe to bail: WebAssetsFetchSync() guarantees all file data is in
        // MEMFS before AsyncFile returns, so Eof() returns NotEof on first
        // check (line 242). This early exit prevents deadlock — can't spin-wait
        // on single-threaded browser event loop. All 11 call sites are also
        // #ifdef HX_NATIVE, so this path is never reached on web builds.
        MILO_WARN("BinStream::WaitUntilReady: data not ready, cannot block "
                   "on web (stream: %s)", Name());
        return false;
#else
        if (polls > 100000) {
            MILO_WARN("BinStream::WaitUntilReady: timed out after 100k polls "
                       "(stream: %s)", Name());
            return false;
        }
        Timer::Sleep(sleepMs);
#endif
    }
}
#endif

void BinStream::PushRev(int revs, Hmx::Object *obj) {
    if (!sRevStack) {
        sRevStack = new std::vector<ObjVersion>();
    }
    sRevStack->push_back(ObjVersion(revs, obj));
}
