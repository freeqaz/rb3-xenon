#include "synth/ByteGrinder.h"

#include <string.h>
#include <stdio.h>
#include <vector>
#include "types.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "utl/Str.h"
#include "os/Debug.h"
#include "os/System.h"
#include "synth/tomcrypt/mycrypt.h"

static unsigned char gHvKeyGreen[64] = {
    0x01, 0x22, 0x00, 0x38, 0xd2, 0x01, 0x78, 0x8b, 0xdd, 0xcd, 0xd0, 0xf0, 0xfe,
    0x3e, 0x24, 0x7f, 0x51, 0x73, 0xad, 0xe5, 0xb3, 0x99, 0xb8, 0x61, 0x58, 0x1a,
    0xf9, 0xb8, 0x1e, 0xa7, 0xbe, 0xbf, 0xc6, 0x22, 0x94, 0x30, 0xd8, 0x3c, 0x84,
    0x14, 0x08, 0x73, 0x7c, 0xf2, 0x23, 0xf6, 0xeb, 0x5a, 0x02, 0x1a, 0x83, 0xf3,
    0x97, 0xe9, 0xd4, 0xb8, 0x06, 0x74, 0x14, 0x6b, 0x30, 0x4c, 0x00, 0x91
};

namespace {
    int GetEncMethod(int ver) {
        int ret = 0;
        switch (ver) {
        case 0xc:
        case 0xd:
            ret = 0;
            break;
        case 0xe:
            ret = 1;
            break;
        case 0xf:
            ret = 2;
            break;
        case 0x10:
            ret = 3;
            break;
        default:
            MILO_NOTIFY(" Wrong encryption version passed to ByteGrinder: [%d] !\n", ver);
            break;
        }
        return ret;
    }
}

void ByteGrinder::HvDecrypt(unsigned char *inBlock, unsigned char *outBlock, int moggVer) {
    symmetric_key key;
    int enc_method = GetEncMethod(moggVer);
    void *placeholder = operator new(0x20C);
    rijndael_setup(&gHvKeyGreen[enc_method * 0x10], 0x10, 0, &key);
    rijndael_ecb_decrypt(inBlock, outBlock, &key);
    delete placeholder;
}

DataNode hashTo5Bits(DataArray *da) {
    static u32 hashMapping[0x100];
    u32 seed = da->Int(1) & 0xFF;
    u32 ret = hashMapping[seed];

    bool moreThanTwo = da->Size() > 2;
    if (moreThanTwo) {
        seed = da->Int(1);
        int max = DIM(hashMapping);
        for (int idx = 0; idx < max; idx++) {
            hashMapping[idx] = (seed >> 3) & 0x1F;
            seed = (seed * 0x19660D) + 0x3C6EF35F;
        }
        return DataNode(kDataInt, 0);
    }
    return DataNode(kDataInt, ret);
}

DataNode hashTo6Bits(DataArray *da) {
    static u32 hashMapping[0x100];
    u32 seed = da->Int(1) & 0xFF;
    u32 ret = hashMapping[seed];

    bool moreThanTwo = da->Size() > 2;
    if (moreThanTwo) {
        seed = da->Int(1);
        int max = DIM(hashMapping);
        for (int idx = 0; idx < max; idx++) {
            hashMapping[idx] = (seed >> 2) & 0x3F;
            seed = (seed * 0x19660D) + 0x3C6EF35F;
        }
        return DataNode(kDataInt, 0);
    }
    return DataNode(kDataInt, ret);
}

DataNode getRandomSequence32A(DataArray *da) {
    static u32 s_seed = 0x521;
    static bool usedUp[0x20];

    bool hasArgs = da->Size() > 1;
    if (hasArgs) {
        int dataint = da->Int(1);
        memset(usedUp, 0, 0x20);
        if ((unsigned int)dataint != 0) {
            s_seed = dataint;
        }
        return DataNode(kDataInt, 0x610A660F);
    } else {
        bool loop = true;
        int idx = 0;
        while (loop) {
            s_seed = s_seed * 0x19660D + 0x3C6EF35F;
            idx = (s_seed >> 2 & 0x1F);
            if (usedUp[idx] == false) {
                loop = false;
                usedUp[idx] = true;
            }
        }
        return DataNode(kDataInt, idx);
    }
}

DataNode getRandomSequence32B(DataArray *da) {
    static u32 s_seed = 0x303F;
    static bool usedUp[0x20];

    bool hasArgs = da->Size() > 1;
    if (hasArgs) {
        int dataint = da->Int(1);
        memset(usedUp, 0, 0x20);
        if ((unsigned int)dataint != 0) {
            s_seed = dataint;
        }
        return DataNode(kDataInt, 0x610A660F);
    } else {
        bool loop = true;
        int idx = 0;
        while (loop) {
            s_seed = s_seed * 0x19660D + 0x3C6EF35F;
            idx = (s_seed >> 2 & 0x1F);
            if (usedUp[idx] == false) {
                loop = false;
                usedUp[idx] = true;
            }
        }
        return DataNode(kDataInt, idx);
    }
}
#define OP_ROT_L(byte, dist)                                                             \
    (unsigned char)((byte << (dist & 31) | byte >> (8 - dist & 31)) & 255)
#define OP_ROT_R(byte, dist)                                                             \
    (unsigned char)((byte >> (dist & 31) | byte << (8 - dist & 31)) & 255)

DataNode op0(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    return DataNode(kDataInt, u8(w ^ operand));
}

DataNode op1(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    auto _tmp3 = u8(w);
    auto _tmp2 = u8(_tmp3 + u8(operand));
    auto _tmp1 = DataNode(kDataInt, _tmp2);
    return _tmp1;
}

DataNode op2(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = bw | (bw << 8);
    ret >>= u8(operand & 7);
    return DataNode(kDataInt, u8(ret));
}

DataNode op3(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    bool b = (operand == 0);
    unsigned long bw = u8(w);
    unsigned long ret = bw | (bw << 8);
    ret >>= b;
    return DataNode(u8(ret));
}

DataNode op4(DataArray *msg) {
    u32 operand = msg->Int(1);
    u32 w = msg->Int(2);
    u32 b = (operand == 0);
    u32 a = (u8(w) == 0);
    u32 ret = (a << 8) | a;
    ret >>= b;
    return u8(ret);
}

DataNode op5(DataArray *msg) {
    u32 operand = msg->Int(1);
    u32 w = msg->Int(2);
    u32 ret;
    // r5 = u8(r3 NOR r3)
    // r3 = (r31 << 29) >> 29;
    // r5 |= r5 << 8
    // r5 >>= r3
    // r0 = u8(r5)
    ret = u8(~(w | w));
    u32 r3 = (operand << 29) >> 29, r4 = ret << 8;
    ret |= r4;
    ret >>= r3;
    return DataNode(kDataInt, u8(ret));
}

DataNode op6(DataArray *msg) {
    u32 operand = msg->Int(1);
    u32 w = msg->Int(2);
    return u8(!w ^ operand);
}

DataNode op7(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    return DataNode(kDataInt, (int)((!w + operand) & 0xFF));
}

DataNode op8(DataArray *msg) {
    u32 op = msg->Int(1);
    u8 bop = op;
    return u8(msg->Int(2) + bop) ^ bop;
}

DataNode op9(DataArray *msg) {
    unsigned long b = msg->Int(1);
    unsigned long a = u8(msg->Int(2));
    return DataNode(kDataInt, (int)(((a ^ b) + b) & 0xFF));
}

DataNode op10(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = bw | (bw << 8);
    ret >>= !operand;
    ret ^= operand;
    return DataNode(kDataInt, (int)(ret & 0xFF));
}

DataNode op11(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = bw | (bw << 8);
    ret >>= u8(operand & 7);
    ret ^= operand;
    return DataNode(kDataInt, (int)(ret & 0xFF));
}

DataNode op12(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = bw | (bw << 8);
    ret >>= u8(operand & 7);
    return DataNode(kDataInt, u8(ret + operand));
}

DataNode op13(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = bw | (bw << 8);
    ret >>= !operand;
    return DataNode(kDataInt, u8(ret + operand));
}

DataNode op14(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = (bw >> 1) | (bw << 7);
    return DataNode(kDataInt, (int)((ret + operand) & 0xFF));
}

DataNode op15(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = (bw >> 2) | (bw << 6);
    return DataNode(kDataInt, (int)((ret + operand) & 0xFF));
}

DataNode op16(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = (bw >> 3) | (bw << 5);
    return DataNode(kDataInt, u8(ret + operand));
}

DataNode op17(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = (bw >> 4) | (bw << 4);
    return DataNode(kDataInt, u8(ret + operand));
}

DataNode op18(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = (bw >> 5) | (bw << 3);
    return DataNode(kDataInt, u8(ret + operand));
}

DataNode op19(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = (bw >> 6) | (bw << 2);
    return DataNode(kDataInt, u8(ret + operand));
}

DataNode op20(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = msg->Int(2);
    unsigned long bw = u8(w);
    unsigned long ret = (bw >> 7) | (bw << 1);
    return DataNode(kDataInt, u8(ret + operand));
}

DataNode op21(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 1) | (br << 7);
    return DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
}

DataNode op22(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 2) | (br << 6);
    return DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
}

DataNode op23(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 3) | (br << 5);
    return DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
}

DataNode op24(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 4) | (br << 4);
    return DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
}

DataNode op25(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 5) | (br << 3);
    return DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
}

DataNode op26(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 6) | (br << 2);
    return DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
}

DataNode op27(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 7) | (br << 1);
    return DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
}

DataNode op28(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 5) | (br << 3);
    return DataNode(kDataInt, (int)(((rot + l) ^ l) & 0xFF));
}

DataNode op29(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 3) | (br << 5);
    return DataNode(kDataInt, (int)(((rot + l) ^ l) & 0xFF));
}

DataNode op30(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 3) | (br << 5);
    return DataNode(kDataInt, (int)(((rot ^ l) + l) & 0xFF));
}

DataNode op31(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 5) | (br << 3);
    return DataNode(kDataInt, (int)(((rot ^ l) + l) & 0xFF));
}

DataNode op32(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 byteVal = w;
    u32 tmp = ((byteVal >> 3) ^ 0x1F) | ((byteVal & 7) << 5);
    return u8(tmp ^ operand);
}

DataNode op33(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 w_byte = w & 0xFF;
    u32 tmp = ((w_byte >> 5) ^ 7) | ((w_byte & 0x1F) << 3);
    return u8(tmp ^ operand);
}

DataNode op34(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 tmp = w;
    u32 val = ((tmp >> 2) ^ 0x3F) | ((tmp & 3) << 6);
    return u8(val ^ operand);
}

DataNode op35(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 tmp = w;
    u32 val = ((tmp >> 6) ^ 3) | ((tmp & 0x3F) << 2);
    return u8(val ^ operand);
}

DataNode op36(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long r = msg->Int(2);
    unsigned long br = u8(r);
    unsigned long rot = (br >> 2) | ((~br) << 6);
    return DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
}

DataNode op37(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long br = u8(msg->Int(2));
    unsigned long rot = (br >> 5) | ((~br) << 3);
    auto _tmp4 = DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
    return _tmp4;
}

DataNode op38(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long br = u8(msg->Int(2));
    unsigned long rot = (br >> 6) | ((~br) << 2);
    auto _tmp6 = DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
    return _tmp6;
}

DataNode op39(DataArray *msg) {
    unsigned long l = msg->Int(1);
    unsigned long br = u8(msg->Int(2));
    unsigned long rot = (br >> 3) | ((~br) << 5);
    auto _tmp5 = DataNode(kDataInt, (int)((rot ^ l) & 0xFF));
    return _tmp5;
}

DataNode op40(DataArray *msg) {
    u32 operand = msg->Int(1);
    u32 w = (u8)msg->Int(2);

    u32 tmp = (((w << 8) | (w ^ 0x5Cu)) >> 6);
    return u8(tmp ^ operand);
}

DataNode op41(DataArray *msg) {
    u32 operand = msg->Int(1);
    u32 w = (u8)msg->Int(2);

    u32 tmp = ((u8)(w >> 2) ^ 0x17) | ((w << 6) & 0xC0);
    return u8(tmp ^ operand);
}

DataNode op42(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = (u8)msg->Int(2);

    unsigned long tmp = ((w >> 3) ^ 0xB) | ((w << 5) & 0xE0);
    return DataNode(kDataInt, (int)((tmp ^ operand) & 0xFF));
}

DataNode op43(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = (u8)msg->Int(2);

    unsigned long tmp = ((w >> 5) ^ 2) | ((w & 0x1F) << 3);
    return DataNode(kDataInt, (int)((tmp ^ operand) & 0xFF));
}

DataNode op44(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = (u8)msg->Int(2);

    unsigned long tmp = ((w >> 2) ^ 0xD) | ((w << 6) & 0xC0);
    return DataNode(kDataInt, (int)((tmp ^ operand) & 0xFF));
}

DataNode op45(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);
    u32 byteVal = w;

    u8 highBits = u8((byteVal >> 3) ^ 6);
    u8 lowBits = u8(((byteVal & 7) << 5));
    u8 rotated = u8(highBits | lowBits);
    return u8(rotated ^ operand);
}

DataNode op46(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);
    u32 byteVal = w;

    u8 highBits = u8((byteVal >> 4) ^ 3);
    u8 lowBits = u8((byteVal << 4) & 0xF0);
    u8 rotated = u8(highBits | lowBits);
    return u8(rotated ^ operand);
}

DataNode op47(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);
    u32 byteVal = w;

    u8 highBits = u8((byteVal >> 1) ^ 0x1B);
    u8 lowBits = u8((byteVal & 1) << 7);
    u8 rotated = u8(highBits | lowBits);
    return u8(rotated ^ operand);
}

DataNode op48(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 a = w;
    u32 working2 = ((a >> 4) ^ 0x6u);
    u32 working3 = (((a << 4) & 0xF0u) ^ 0x5u);
    u32 tmp = (working2 | working3);
    return u8(tmp ^ operand);
}

DataNode op49(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 working3 = (w << 8) ^ 0x5Cu;
    u32 working2 = (w ^ 0x63u);
    u32 tmp = ((working2 | working3) >> 3);
    return u8(tmp ^ operand);
}

DataNode op50(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);
    u32 byteVal = w;

    u8 highBits = u8(((byteVal << 3) & 0xF8) ^ 2);
    u8 lowBits = u8((byteVal >> 5) ^ 3);
    u8 rotated = u8(highBits | lowBits);
    return u8(rotated ^ operand);
}

DataNode op51(DataArray *msg) {
    u32 operand = msg->Int(1);
    u32 w = (u8)msg->Int(2);

    u32 working2 = (w ^ 0x63);
    u32 working3 = (w << 8) ^ 0x5C;
    u32 tmp = ((working2 | working3) >> 6);
    return u8(tmp ^ operand);
}

DataNode op52(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);
    u32 byteVal = w;

    u8 highBits = u8((byteVal >> 1) ^ 0x2e);
    u8 lowBits = u8((byteVal << 7) ^ 0x1b);
    u8 rotated = u8(highBits | lowBits);
    return u8(rotated ^ operand);
}

DataNode op53(DataArray *msg) {
    unsigned long operand = msg->Int(1);
    unsigned long w = (u8)msg->Int(2);

    unsigned long working3 = (w << 8) ^ 0x36u;
    unsigned long working2 = (w ^ 0x5Cu);
    unsigned long tmp = ((working2 | working3) >> 7);
    return DataNode(kDataInt, (int)((tmp ^ operand) & 0xFF));
}

DataNode op54(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 w32 = (u32)w;
    u32 part2 = (w32 << 5) & 0xE0;
    part2 ^= 0x6;
    u32 part1 = (w32 >> 3) & 0xFF;
    part1 ^= 0xB;
    u32 tmp = part1 | part2;
    return u8(tmp ^ operand);
}

DataNode op55(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);
    u32 byteVal = w;

    u8 highBits = u8(((byteVal & 0x1f) << 3) ^ 1);
    u8 lowBits = u8((byteVal >> 5) ^ 2);
    u8 rotated = u8(highBits | lowBits);
    return u8(rotated ^ operand);
}

DataNode op56(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);
    u32 byteVal = w;

    u8 highBits = u8(((byteVal & 0xF) << 4) ^ 6);
    u8 lowBits = u8((byteVal >> 4) ^ 3);
    u8 rotated = u8(highBits | lowBits);
    return u8(rotated ^ operand);
}

DataNode op57(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 w_extended = w;
    u32 working2 = (w_extended ^ 0x3Cu);
    u32 working3 = (w_extended << 8) ^ 0x65u;
    u32 tmp = ((working2 | working3) >> 5);
    return DataNode(kDataInt, u8(tmp ^ operand));
}

DataNode op58(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 working2 = (w ^ 0x65u);
    u32 working3 = (w << 8) ^ 0x3Cu;
    u32 tmp = ((working2 | working3) >> 6);
    return u8(tmp ^ operand);
}

DataNode op59(DataArray *msg) {
    u32 operand = msg->Int(1);
    u32 w = (u8)msg->Int(2);

    u32 working2 = (w ^ 0x65u);
    u32 working3 = (w << 8) ^ 0x3Cu;
    u32 tmp = ((working2 | working3) >> 2);
    return u8(tmp ^ operand);
}

DataNode op60(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 byteVal = w;
    u32 xorLow = (byteVal ^ 0xFFu);
    u32 xorHigh = (byteVal << 8) ^ 0xAAu;
    u32 tmp = ((xorLow | xorHigh) >> 4);
    return u8(tmp ^ operand);
}

DataNode op61(DataArray *msg) {
    u32 operand = msg->Int(2);
    u8 w = msg->Int(1);

    u32 a = (w >> 3) ^ 0x15;
    u32 b = ((w & 7) << 5) ^ 0x1f;
    u32 tmp = a | b;
    return u8(tmp ^ operand);
}

DataNode op62(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);
    u32 w32 = w;

    u8 tmp1 = u8((w32 >> 5) ^ 5);
    u8 tmp2 = u8(((w32 << 3) & 0xF8) ^ 7);
    u8 combined = u8(tmp1 | tmp2);
    return u8(combined ^ operand);
}

DataNode op63(DataArray *msg) {
    u32 operand = msg->Int(1);
    u8 w = msg->Int(2);

    u32 byteVal = w;
    u32 xorLow = (byteVal ^ 0xFFu);
    u32 xorHigh = (byteVal << 8) ^ 0xAFu;
    u32 tmp = ((xorLow | xorHigh) >> 6);
    return u8(tmp ^ operand);
}

extern DataArray *DataReadString(const char *);

unsigned long ByteGrinder::pickOneOf32A(bool b, long l) {
    DataArray *a;
    char script[256];
    if (b) {
        sprintf(script, "{xa %d}", l);
        a = DataReadString(script);
    } else {
        a = DataReadString("{xa}");
    }
    unsigned long result = a->Evaluate(0).Int();
    a->Release();
    return result;
}

unsigned long ByteGrinder::pickOneOf32B(bool b, long l) {
    DataArray *a;
    char script[256];
    if (b) {
        sprintf(script, "{ya %d}", l);
        a = DataReadString(script);
    } else {
        a = DataReadString("{ya}");
    }
    unsigned long result = a->Evaluate(0).Int();
    a->Release();
    return result;
}

DataNode getRandomLong(DataArray *da) {
    static u32 s_seed = 0x521;
    bool hasOne = da->Size() > 1;
    if (hasOne) {
        s_seed = s_seed * 0x19660D + 0x3C6EF35F;
    }
    return (s32)s_seed;
}

DataNode magicNumberGenerator(DataArray *da) {
    int magic = 0x5c5c5c5c;
    if (da->Int(2) == 2) {
        magic = 0x36363636;
    }
    int idx = da->Int(1);
    int v = ((idx ^ magic) * 0x19660d + 0x3c6ef35f);
    if (da->Int(2) == 1) {
        v = (v * 0x19660d + 0x3c6ef35f);
    }
    return DataNode(kDataInt, v);
}

#ifdef HX_NATIVE
// ============================================================================
// Native (non-DTA) implementation of GrindArray
// Uses the Onyx Music Game Toolkit's proven algorithm (cbits/ByteGrinder.cpp).
// The DTA version fails on x86_64 due to integer width/evaluation differences.
// ============================================================================

namespace {

static const u32 kLcgMul = 0x19660D;
static const u32 kLcgInc = 0x3C6EF35F;

// --- Native op functions matching Onyx ops.c ---
// Signature: op(bar, foo) where bar = bar[ix+1], foo = running accumulator.
// All rotations are 8-bit right rotations via (val>>amt)|(val<<(8-amt)).
#define ROTR8(val, amt) (u8)(((u8)(val) >> (amt)) | ((u8)(val) << (8 - (amt))))

typedef u8 (*NativeOp)(u8, u8);

// RB2 ops (0-31)
static u8 nop0(u8 bar, u8 foo)  { return foo ^ bar; }
static u8 nop1(u8 bar, u8 foo)  { return u8(foo + bar); }
static u8 nop2(u8 bar, u8 foo)  { return ROTR8(foo, bar & 7); }
static u8 nop3(u8 bar, u8 foo)  { return ROTR8(foo, (bar == 0)); }
static u8 nop4(u8 bar, u8 foo)  { return ROTR8(foo == 0, bar == 0); }
static u8 nop5(u8 bar, u8 foo)  { return ROTR8(0xFF ^ foo, bar & 7); }
static u8 nop6(u8 bar, u8 foo)  { return u8(bar ^ (foo == 0)); }
static u8 nop7(u8 bar, u8 foo)  { return u8(bar + (foo == 0)); }
static u8 nop8(u8 bar, u8 foo)  { return u8(bar ^ u8(foo + bar)); }
static u8 nop9(u8 bar, u8 foo)  { return u8(bar + (foo ^ bar)); }
static u8 nop10(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, (bar == 0))); }
static u8 nop11(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, bar & 7)); }
static u8 nop12(u8 bar, u8 foo) { return u8(bar + ROTR8(foo, bar & 7)); }
static u8 nop13(u8 bar, u8 foo) { return u8(bar + ROTR8(foo, (bar == 0))); }
static u8 nop14(u8 bar, u8 foo) { return u8(bar + ROTR8(foo, 1)); }
static u8 nop15(u8 bar, u8 foo) { return u8(bar + ROTR8(foo, 2)); }
static u8 nop16(u8 bar, u8 foo) { return u8(bar + ROTR8(foo, 3)); }
static u8 nop17(u8 bar, u8 foo) { return u8(bar + ROTR8(foo, 4)); }
static u8 nop18(u8 bar, u8 foo) { return u8(bar + ROTR8(foo, 5)); }
static u8 nop19(u8 bar, u8 foo) { return u8(bar + ROTR8(foo, 6)); }
static u8 nop20(u8 bar, u8 foo) { return u8(bar + ROTR8(foo, 7)); }
static u8 nop21(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 1)); }
static u8 nop22(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 2)); }
static u8 nop23(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 3)); }
static u8 nop24(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 4)); }
static u8 nop25(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 5)); }
static u8 nop26(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 6)); }
static u8 nop27(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 7)); }
static u8 nop28(u8 bar, u8 foo) { return u8(bar ^ u8(bar + ROTR8(foo, 5))); }
static u8 nop29(u8 bar, u8 foo) { return u8(bar ^ u8(bar + ROTR8(foo, 3))); }
static u8 nop30(u8 bar, u8 foo) { return u8(bar + (bar ^ ROTR8(foo, 3))); }
static u8 nop31(u8 bar, u8 foo) { return u8(bar + (bar ^ ROTR8(foo, 5))); }

// RB3+ ops (32-63)
static u8 nop32(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 3) ^ 0x1F); }
static u8 nop33(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 5) ^ 0x07); }
static u8 nop34(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 2) ^ 0x3F); }
static u8 nop35(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 6) ^ 0x03); }
static u8 nop36(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 2) ^ 0xC0); }
static u8 nop37(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 5) ^ 0xF8); }
static u8 nop38(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 6) ^ 0xFC); }
static u8 nop39(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 3) ^ 0xE0); }
static u8 nop40(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 6) ^ 0x01); }
static u8 nop41(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 2) ^ 0x17); }
static u8 nop42(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 3) ^ 0x0B); }
static u8 nop43(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 5) ^ 0x02); }
static u8 nop44(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 2) ^ 0x0D); }
static u8 nop45(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 3) ^ 0x06); }
static u8 nop46(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 4) ^ 0x03); }
static u8 nop47(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 1) ^ 0x1B); }
static u8 nop48(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 4) | 0x05) ^ 0x02); }
static u8 nop49(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 3) | 0x0B) ^ 0x04); }
static u8 nop50(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 5) | 0x02) ^ 0x01); }
static u8 nop51(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 6) | 0x01)); }
static u8 nop52(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 1) | 0x1B) ^ 0x24); }
static u8 nop53(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 7)); }
static u8 nop54(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 3) | 0x06) ^ 0x09); }
static u8 nop55(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 5) | 0x01) ^ 0x02); }
static u8 nop56(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 4) | 0x06) ^ 0x01); }
static u8 nop57(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 5) | 0x03)); }
static u8 nop58(u8 bar, u8 foo) { return u8(bar ^ ROTR8(foo, 6) ^ 0x01); }
static u8 nop59(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 2) | 0x19) ^ 0x06); }
static u8 nop60(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 4) | 0x0A) ^ 0x05); }
static u8 nop61(u8 bar, u8 foo) { return u8(foo ^ ((bar << 5) | 0x1F)); } // args swapped vs others
static u8 nop62(u8 bar, u8 foo) { return u8(bar ^ u8((foo << 3) + 0x07)); }
static u8 nop63(u8 bar, u8 foo) { return u8(bar ^ (ROTR8(foo, 6) | 0x02) ^ 0x01); }

static const NativeOp kOpsRB2[32] = {
    nop0,  nop1,  nop2,  nop3,  nop4,  nop5,  nop6,  nop7,
    nop8,  nop9,  nop10, nop11, nop12, nop13, nop14, nop15,
    nop16, nop17, nop18, nop19, nop20, nop21, nop22, nop23,
    nop24, nop25, nop26, nop27, nop28, nop29, nop30, nop31,
};

static const NativeOp kOpsRB3[32] = {
    nop32, nop33, nop34, nop35, nop36, nop37, nop38, nop39,
    nop40, nop41, nop42, nop43, nop44, nop45, nop46, nop47,
    nop48, nop49, nop50, nop51, nop52, nop53, nop54, nop55,
    nop56, nop57, nop58, nop59, nop60, nop61, nop62, nop63,
};

// The O function table: O[slot] = op function, populated by constructor permutation.
static NativeOp sOTable[64];
static bool sOTableInit = false;

static void GeneratePermutation32(u32 seed, u32 outPerm[32]) {
    bool usedUp[32];
    memset(usedUp, 0, sizeof(usedUp));
    for (int i = 0; i < 32; i++) {
        u32 idx;
        for (;;) {
            seed = seed * kLcgMul + kLcgInc;
            idx = (seed >> 2) & 0x1F;
            if (!usedUp[idx]) { usedUp[idx] = true; break; }
        }
        outPerm[i] = idx;
    }
}

static void EnsureOTableInit() {
    if (sOTableInit) return;
    // Matches ByteGrinder::ByteGrinder() constructor permutation:
    // PickOneOf32A(true, 0xD5) then O[pick()] = opsRB2[i]
    // PickOneOf32A(true, 0x23E) then O[pick()+32] = opsRB3[i]
    u32 perm[32];
    GeneratePermutation32(0xD5, perm);
    for (int i = 0; i < 32; i++) sOTable[perm[i]] = kOpsRB2[i];
    GeneratePermutation32(0x23E, perm);
    for (int i = 0; i < 32; i++) sOTable[perm[i] + 32] = kOpsRB3[i];
    sOTableInit = true;
}

} // anonymous namespace

void ByteGrinder::GrindArray(
    long seedA, long seedB, unsigned char *arrayToGrind, int arrayLen, int moggVersion
) {
    EnsureOTableInit();

    // Build hash map: for v > 0xD, 6-bit hash seeded with seedB; else 5-bit seeded with seedA.
    u8 hashMap[256];
    if (moggVersion > 0x0D) {
        u32 s = (u32)seedB;
        for (int i = 0; i < 256; i++) {
            hashMap[i] = (s >> 2) & 0x3F;
            s = s * kLcgMul + kLcgInc;
        }
    } else {
        u32 s = (u32)seedA;
        for (int i = 0; i < 256; i++) {
            hashMap[i] = (s >> 3) & 0x1F;
            s = s * kLcgMul + kLcgInc;
        }
    }

    // Build switchCases: per-call permutation mapping case index -> O-table slot.
    // First 32 slots from seedB, then (for v > 0xD) 32 more from seedA offset by 32.
    u8 switchCases[64];
    u32 perm[32];
    GeneratePermutation32((u32)seedB, perm);
    for (int i = 0; i < 32; i++) switchCases[i] = (u8)perm[i];

    if (moggVersion > 0xD) {
        GeneratePermutation32((u32)seedA, perm);
        for (int i = 0; i < 32; i++) switchCases[32 + i] = (u8)(perm[i] + 32);
    }

    // Grind: for each key byte, apply ops over pairs of array bytes.
    // This matches Onyx's ByteGrinder::GrindArray — simple stride-2 loop.
    for (int i = 0; i < arrayLen; i++) {
        u8 foo = arrayToGrind[i];
        for (int ix = 0; ix < arrayLen; ix += 2) {
            foo = sOTable[switchCases[hashMap[arrayToGrind[ix]]]](arrayToGrind[ix + 1], foo);
        }
        arrayToGrind[i] = foo;
    }
}

int magicNumberGeneratorNative(int idx, int mode) {
    int magic = (mode == 2) ? 0x36363636 : 0x5c5c5c5c;
    int v = (idx ^ magic) * 0x19660d + 0x3c6ef35f;
    if (mode == 1) v = v * 0x19660d + 0x3c6ef35f;
    return v;
}

#else // !HX_NATIVE
void ByteGrinder::GrindArray(
    long seedA, long seedB, unsigned char *arrayToGrind, int arrayLen, int moggVersion
) {
    char script[256];
    DataArray *mainScriptArray;

    sprintf(script, "{ma %d 2}", seedA);
    mainScriptArray = DataReadString(script);
    mainScriptArray->Evaluate(0).Int();
    mainScriptArray->Release();

    sprintf(script, "{za %d 2}", seedB);
    mainScriptArray = DataReadString(script);
    mainScriptArray->Evaluate(0).Int();
    mainScriptArray->Release();

    String mainScript;
    int encMethod = GetEncMethod(moggVersion);
    mainScript = "($foo $bar){O68($ix 0){O64{>{O65 $bar}$ix}{O66{ma{O67 $bar $ix}}";
    if (encMethod != 0) {
        mainScript = "($foo $bar){O68($ix 0){O64{>{O65 $bar}$ix}{O66{za{O67 $bar $ix}}";
    }

    pickOneOf32B(true, seedB);
    for (int i = 0; i < 0x20; i++) {
        char block[256];
        char callName[16];
        sprintf(callName, "O%d", pickOneOf32B(false, 0));
        sprintf(block, "(%d{O70 $ix}{O69 $foo{%s{O67 $bar $ix}$foo}})", i, callName);
        mainScript += block;
    }

    if (encMethod != 0) {
        pickOneOf32B(true, seedA);
        for (int i = 0x20; i < 0x40; i++) {
            char block[256];
            char callName[16];
            sprintf(callName, "O%d", pickOneOf32B(false, 0) + 0x20);
            sprintf(block, "(%d{O70 $ix}{O69 $foo{%s{O67 $bar $ix}$foo}})", i, callName);
            mainScript += block;
        }
    }

    mainScript += "}{O70 $ix}}}$foo";
    mainScriptArray = DataReadString(mainScript.c_str());
    for (int i = 0; i < arrayLen; i++) {
        char itoaBuffer[32];
        unsigned char w = arrayToGrind[i];
        String stringArgs("");
        Hx_snprintf(itoaBuffer, sizeof(itoaBuffer), "%d", w);
        stringArgs += itoaBuffer;
        stringArgs += " (";
        for (int j = 0; j < 0x10; j++) {
            Hx_snprintf(itoaBuffer, sizeof(itoaBuffer), "%d", arrayToGrind[j]);
            stringArgs += itoaBuffer;
            stringArgs += " ";
        }
        stringArgs += ")";
        DataArray *args = DataReadString(stringArgs.c_str());
        arrayToGrind[i] = mainScriptArray->ExecuteScript(0, nullptr, args, 0).Int();
        args->Release();
    }
    mainScriptArray->Release();
}
#endif // HX_NATIVE

void ByteGrinder::Init() {
    char functionName[0x100];
    // This *must* be written out in reverse to match
    functionName[1] = 'a';
    functionName[0] = 'N';
    functionName[2] = '\0';
    DataRegisterFunc(functionName, getRandomLong);
    functionName[0] = 'h';
    DataRegisterFunc(functionName, magicNumberGenerator);
    functionName[0] = 'm';
    DataRegisterFunc(functionName, hashTo5Bits);
    functionName[0] = 'z';
    DataRegisterFunc(functionName, hashTo6Bits);
    functionName[0] = 'x';
    DataRegisterFunc(functionName, getRandomSequence32A);
    functionName[0] = 'y';
    DataRegisterFunc(functionName, getRandomSequence32B);
    std::vector<DataFunc *> funPtrs;
    funPtrs.push_back(op0);
    funPtrs.push_back(op1);
    funPtrs.push_back(op2);
    funPtrs.push_back(op3);
    funPtrs.push_back(op4);
    funPtrs.push_back(op5);
    funPtrs.push_back(op6);
    funPtrs.push_back(op7);
    funPtrs.push_back(op8);
    funPtrs.push_back(op9);
    funPtrs.push_back(op10);
    funPtrs.push_back(op11);
    funPtrs.push_back(op12);
    funPtrs.push_back(op13);
    funPtrs.push_back(op14);
    funPtrs.push_back(op15);
    funPtrs.push_back(op16);
    funPtrs.push_back(op17);
    funPtrs.push_back(op18);
    funPtrs.push_back(op19);
    funPtrs.push_back(op20);
    funPtrs.push_back(op21);
    funPtrs.push_back(op22);
    funPtrs.push_back(op23);
    funPtrs.push_back(op24);
    funPtrs.push_back(op25);
    funPtrs.push_back(op26);
    funPtrs.push_back(op27);
    funPtrs.push_back(op28);
    funPtrs.push_back(op29);
    funPtrs.push_back(op30);
    funPtrs.push_back(op31);
    pickOneOf32A(true, 0xD5);
    for (int i = 0; i < funPtrs.size(); i++) {
        int oNum = pickOneOf32A(false, 0);
        sprintf(functionName, "O%d", oNum);
        DataRegisterFunc(functionName, funPtrs[i]);
    }
    funPtrs.clear();
    funPtrs.push_back(op32);
    funPtrs.push_back(op33);
    funPtrs.push_back(op34);
    funPtrs.push_back(op35);
    funPtrs.push_back(op36);
    funPtrs.push_back(op37);
    funPtrs.push_back(op38);
    funPtrs.push_back(op39);
    funPtrs.push_back(op40);
    funPtrs.push_back(op41);
    funPtrs.push_back(op42);
    funPtrs.push_back(op43);
    funPtrs.push_back(op44);
    funPtrs.push_back(op45);
    funPtrs.push_back(op46);
    funPtrs.push_back(op47);
    funPtrs.push_back(op48);
    funPtrs.push_back(op49);
    funPtrs.push_back(op50);
    funPtrs.push_back(op51);
    funPtrs.push_back(op52);
    funPtrs.push_back(op53);
    funPtrs.push_back(op54);
    funPtrs.push_back(op55);
    funPtrs.push_back(op56);
    funPtrs.push_back(op57);
    funPtrs.push_back(op58);
    funPtrs.push_back(op59);
    funPtrs.push_back(op60);
    funPtrs.push_back(op61);
    funPtrs.push_back(op62);
    funPtrs.push_back(op63);
    pickOneOf32A(true, 0x23E);
    for (int i = 0; i < funPtrs.size(); i++) {
        int oNum = pickOneOf32A(false, 0) + 32;
        sprintf(functionName, "O%d", oNum);
        DataRegisterFunc(functionName, funPtrs[i]);
    }
}
