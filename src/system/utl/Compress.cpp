#include "utl/Compress.h"
#include "utl/MemMgr.h"
#include "zlib/zconf.h"
#include "zlib/zlib.h"
#include "os/Debug.h"

void *ZAlloc(void *mem, unsigned int len, unsigned int ct) {
    return _MemAllocTemp(len * ct, __FILE__, 0x55, "zlib", 0);
}

void ZFree(void *a, void *b) { MemFree(b); }

void DecompressMem(
    const void *in, int in_len, void *out, int &out_len, const char *filename
) {
    z_stream s;

    s.next_in = (unsigned char *)in;
    s.avail_in = in_len;
    s.next_out = (unsigned char *)out;
    s.avail_out = out_len;
    s.zalloc = ZAlloc;
    s.zfree = ZFree;

    MILO_ASSERT(inflateInit2(&s, -MAX_WBITS) == Z_OK, 0x6C);

    int ret = inflate(&s, 4);
    if (ret != 1)
        MILO_FAIL("Inflate error: %d in %s", ret, filename);

    MILO_ASSERT(s.avail_in == 0, 0x70);
    MILO_ASSERT(inflateEnd(&s) == Z_OK, 0x71);

    out_len = s.total_out;
}

void CompressMem(
    const void *in, int in_len, void *out, int &out_len, const char *filename
) {
    z_stream s;
    s.next_in = (unsigned char *)in;
    s.avail_in = in_len;
    s.next_out = (unsigned char *)out;
    s.avail_out = out_len;
    s.zalloc = ZAlloc;
    s.zfree = ZFree;

    MILO_ASSERT(deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) == Z_OK, 0x91);

    MILO_ASSERT(deflate(&s, Z_FINISH) == Z_STREAM_END, 0x93);
    MILO_ASSERT(deflateEnd(&s) == Z_OK, 0x94);

    out_len = s.total_out;
}
