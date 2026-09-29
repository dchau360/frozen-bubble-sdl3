/* Test helper for the server's account sign-in (server/account.h): the
 * Python server tests cannot produce Monocypher's EdDSA (BLAKE2b) signatures
 * with the standard library, so they shell out to this.
 *
 *   account-sign-tool <seed: 64 hex>                 -> prints the public key
 *   account-sign-tool <seed: 64 hex> <nonce: 64 hex> -> prints the AUTHSIG signature
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "account.h"
#include "monocypher.h"

static int unhex(const char* s, uint8_t* out, size_t n)
{
        size_t i;
        unsigned v;
        if (strlen(s) != n * 2) return 0;
        for (i = 0; i < n; i++) {
                if (sscanf(s + i * 2, "%2x", &v) != 1) return 0;
                out[i] = (uint8_t)v;
        }
        return 1;
}

static void puthex(const uint8_t* in, size_t n)
{
        size_t i;
        for (i = 0; i < n; i++) printf("%02x", in[i]);
        printf("\n");
}

int main(int argc, char** argv)
{
        uint8_t seed[32], sk[64], pk[32], sig[64];
        char msg[256];
        if (argc < 2 || !unhex(argv[1], seed, sizeof(seed))) {
                fprintf(stderr, "usage: %s <seed hex> [nonce hex]\n", argv[0]);
                return 2;
        }
        crypto_eddsa_key_pair(sk, pk, seed);
        if (argc < 3) {
                puthex(pk, sizeof(pk));
                return 0;
        }
        snprintf(msg, sizeof(msg), "%s%s", ACCOUNT_SIGNED_PREFIX, argv[2]);
        crypto_eddsa_sign(sig, sk, (const uint8_t*)msg, strlen(msg));
        puthex(sig, sizeof(sig));
        return 0;
}
