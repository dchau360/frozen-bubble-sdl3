// The game's side of account sign-in (src/playeraccount.cpp): recovery-code
// parsing and formatting, that a code always yields the same key, and that
// the signature it sends as AUTHSIG verifies the way server/account.c checks
// it. Never lets the module save a code, so it leaves no account.txt behind.
#include <cstdio>
#include <cstdint>
#include <string>

#include "../third_party/monocypher/monocypher.h"
#include "playeraccount.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static bool FromHex(const std::string& s, uint8_t* out, size_t n) {
    if (s.size() != n * 2) return false;
    for (size_t i = 0; i < n; ++i) {
        unsigned v;
        if (std::sscanf(s.c_str() + i * 2, "%2x", &v) != 1) return false;
        out[i] = static_cast<uint8_t>(v);
    }
    return true;
}

int main() {
    using namespace playeraccount;

    // Typing leniency: case, dashes, spaces, and the look-alike letters.
    CHECK(NormalizeCode("abcd-efgh-jkmn-pqrs") == "ABCDEFGHJKMNPQRS");
    CHECK(NormalizeCode("OOOO-IIII-LLLL-2345") == "0000111111112345");
    CHECK(NormalizeCode("ABCD-EFGH-JKMN-PQR").empty());     // 15 chars
    CHECK(NormalizeCode("ABCD-EFGH-JKMN-PQRST").empty());   // 17 chars
    CHECK(NormalizeCode("ABCD-EFGH-JKMN-PQRU").empty());    // U isn't in the alphabet
    CHECK(NormalizeCode("ABCD-EFGH-JKMN-PQR!").empty());
    CHECK(FormatCode("ABCDEFGHJKMNPQRS") == "ABCD-EFGH-JKMN-PQRS");

    CHECK(!UseCode("not a code", false));
    CHECK(UseCode("7K3M-9QX2-HD4R-B8TN", false));
    CHECK(Code() == "7K3M9QX2HD4RB8TN");
    const std::string pk1 = PublicKeyHex();
    CHECK(pk1.size() == 64);

    // The same code is the same account; a different code is not.
    CHECK(UseCode("7k3m 9qx2 hd4r b8tn", false));
    CHECK(PublicKeyHex() == pk1);
    CHECK(UseCode("0000-0000-0000-0001", false));
    CHECK(PublicKeyHex() != pk1);
    CHECK(UseCode("7K3M9QX2HD4RB8TN", false));

    // AUTHSIG: a signature over the server's prefix plus the nonce, checked
    // the way server/account.c's account_finish() checks it.
    const std::string nonce(64, 'a');
    const std::string sigHex = SignChallenge(nonce);
    uint8_t sig[64], pk[32];
    CHECK(FromHex(sigHex, sig, sizeof(sig)));
    CHECK(FromHex(pk1, pk, sizeof(pk)));
    const std::string msg = "frozen-bubble account v1 " + nonce;
    CHECK(crypto_eddsa_check(sig, pk, reinterpret_cast<const uint8_t*>(msg.data()), msg.size()) == 0);
    const std::string other = "frozen-bubble account v1 " + std::string(64, 'b');
    CHECK(crypto_eddsa_check(sig, pk, reinterpret_cast<const uint8_t*>(other.data()), other.size()) != 0);
    CHECK(SignChallenge("short").empty());
    CHECK(SignChallenge(std::string(64, 'z')).empty());

    if (failures) return 1;
    std::printf("playeraccount: all checks passed\n");
    return 0;
}
