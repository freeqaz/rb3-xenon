#include "mycrypt.h"

#ifdef CTR

int ctr_start(
    int cipher,
    const unsigned char *count,
    const unsigned char *key,
    int keylen,
    int num_rounds,
    symmetric_CTR *ctr
) {
    int x, errno_var;

    // _ARGCHK(count != NULL);
    // _ARGCHK(key != NULL);
    // _ARGCHK(ctr != NULL);

    /* bad param? */
    if ((errno_var = cipher_is_valid(cipher)) != CRYPT_OK) {
        errno_var = 1;
        return errno_var;
    }

    /* setup cipher */
    if ((errno_var = cipher_descriptor[cipher].setup(key, keylen, num_rounds, &ctr->key))
        != CRYPT_OK) {
        errno_var = 1;
        return errno_var;
    }

    /* copy ctr */
    ctr->blocklen = cipher_descriptor[cipher].block_length;
    ctr->cipher = cipher;
    ctr->padlen = 0;
    for (x = 0; x < ctr->blocklen; x++) {
        ctr->ctr[x] = count[x];
    }
    cipher_descriptor[ctr->cipher].ecb_encrypt(ctr->ctr, ctr->pad, &ctr->key);
    return CRYPT_OK;
}

int ctr_reinit(int cipher, unsigned char *r4, symmetric_CTR *ctr) {
    if (cipher_is_valid(cipher))
        return 1;
    else {
        ctr->padlen = 0;
        memcpy(ctr->ctr, r4, ctr->blocklen);
        cipher_descriptor[ctr->cipher].ecb_encrypt(ctr->ctr, ctr->pad, &ctr->key);
        return 0;
    }
}

int ctr_encrypt_fast(
    const unsigned char *src, unsigned char *dst, unsigned long len, symmetric_CTR *ctr
) {
    int x0, x1, x2, x3;
    unsigned long n;
    for (n = 0; n < len; n += 0x10) {
        int i;
        for (i = 0; i < ctr->blocklen; i++) {
            if (++ctr->ctr[i] != '\0')
                break;
        }
        cipher_descriptor[ctr->cipher].ecb_encrypt(ctr->ctr, ctr->pad, &ctr->key);

        /* The `int *d` local is CODEGEN-LOAD-BEARING, not a tidy-up (lane DK-2d).
         * Retail makes `src` the update-form induction variable (`lwzu 0x10(r29)`
         * on r29 = src-0x10) and advances `dst` with a plain `addi`. Writing the
         * stores as `((int *)(dst + n))[i]` makes MSVC pick the MIRROR IMAGE --
         * dst becomes the `stwu` pointer and src the plain one -- which costs 10
         * instructions. Hoisting only the dst address into a local flips the IV
         * selection to src and the whole body falls into retail's schedule.
         * The word order is equally load-bearing: retail emits loads and stores
         * in word order 1,2,3,0, and MSVC rotates the source order by +1, so the
         * source must be written 0,1,2,3 to emit 1,2,3,0. Writing 1,2,3,0 here
         * emits 2,3,0,1 (measured: 19 mismatches vs 4). */
        x0 = ((const int *)(src + n))[0];
        x1 = ((const int *)(src + n))[1];
        x2 = ((const int *)(src + n))[2];
        x3 = ((const int *)(src + n))[3];

        x0 ^= ((int *)ctr->pad)[0];
        x1 ^= ((int *)ctr->pad)[1];
        x2 ^= ((int *)ctr->pad)[2];
        x3 ^= ((int *)ctr->pad)[3];

        {
            /* RESIDUAL (1 instruction, 99.8% mpn): retail's word-1 xor is
             * `xor r8,r8,r11` (pad as rS) where ours is `xor r8,r11,r8`. Same
             * registers, same dest, only the rS/rB encoding differs. NOT
             * source-steerable -- MSVC canonicalizes commutative operands:
             * `pad[1] ^ x1`, making the pad the accumulator variable, and
             * reordering the xor statements all produced byte-identical output.
             * Register-allocation class; permuter is off by standing directive. */
            int *d = (int *)(dst + n);
            d[0] = x0;
            d[1] = x1;
            d[2] = x2;
            d[3] = x3;
        }
    }
    return 0;
}

int ctr_encrypt(
    const unsigned char *pt, unsigned char *ct, unsigned long len, symmetric_CTR *ctr
) {
    int x;

    // _ARGCHK(pt != NULL);
    // _ARGCHK(ct != NULL);
    // _ARGCHK(ctr != NULL);

    if (cipher_is_valid(ctr->cipher) != CRYPT_OK) {
        return CRYPT_ERROR;
    }

    if ((((int)pt & 3) == 0) && (((int)ct & 3) == 0) && ((len & ctr->blocklen - 1U) == 0)
        && (ctr->blocklen == 0x10) && (ctr->padlen == ctr->blocklen)) {
        return ctr_encrypt_fast(pt, ct, len, ctr);
    }

    while (len--) {
        /* is the pad empty? */
        if (ctr->padlen == ctr->blocklen) {
            /* increment counter */
            for (x = 0; x < ctr->blocklen; x++) {
                ctr->ctr[x] = (ctr->ctr[x] + 1) & 255;
                if (ctr->ctr[x] != 0) {
                    break;
                }
            }

            /* encrypt it */
            cipher_descriptor[ctr->cipher].ecb_encrypt(ctr->ctr, ctr->pad, &ctr->key);
            ctr->padlen = 0;
        }
        *ct++ = *pt++ ^ ctr->pad[ctr->padlen++];
    }
    return CRYPT_OK;
}

int ctr_decrypt(
    const unsigned char *ct, unsigned char *pt, unsigned long len, symmetric_CTR *ctr
) {
    // _ARGCHK(pt != NULL);
    // _ARGCHK(ct != NULL);
    // _ARGCHK(ctr != NULL);

    return ctr_encrypt(ct, pt, len, ctr);
}

#endif
