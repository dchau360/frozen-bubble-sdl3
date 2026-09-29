#if defined(_WIN32)
#define _CRT_RAND_S  // rand_s(), before <stdlib.h>
#endif
#include "playeraccount.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../third_party/monocypher/monocypher.h"
#include "gamesettings.h"

#ifdef __WASM_PORT__
#include <emscripten.h>
#include <unistd.h>
#endif

namespace playeraccount {
namespace {

constexpr char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr size_t kCodeLen = 16;
// Salt for turning a code into a key. Changing it changes every account.
constexpr char kSalt[] = "frozen-bubble-account-v1";
// Must match server/account.h's ACCOUNT_SIGNED_PREFIX.
constexpr char kSignedPrefix[] = "frozen-bubble account v1 ";

std::string code;
bool keysReady = false;
uint8_t secretKey[64];
uint8_t publicKey[32];

bool RandomBytes(uint8_t* out, size_t n) {
#if defined(_WIN32)
    for (size_t i = 0; i < n; ++i) {
        unsigned int v;
        if (rand_s(&v) != 0) return false;
        out[i] = static_cast<uint8_t>(v);
    }
    return true;
#elif defined(__APPLE__)
    arc4random_buf(out, n);
    return true;
#elif defined(__WASM_PORT__)
    return getentropy(out, n) == 0;  // crypto.getRandomValues()
#else
    // Linux and Android. getentropy() would do, but Android only has it
    // from API 28 and this app supports 23.
    FILE* f = std::fopen("/dev/urandom", "rb");
    if (!f) return false;
    const size_t got = std::fread(out, 1, n, f);
    std::fclose(f);
    return got == n;
#endif
}

std::string ToHex(const uint8_t* in, size_t n) {
    static const char digits[] = "0123456789abcdef";
    std::string s(n * 2, '0');
    for (size_t i = 0; i < n; ++i) {
        s[i * 2] = digits[in[i] >> 4];
        s[i * 2 + 1] = digits[in[i] & 15];
    }
    return s;
}

bool FromHex(const std::string& s, uint8_t* out, size_t n) {
    if (s.size() != n * 2) return false;
    for (size_t i = 0; i < n * 2; ++i) {
        const char c = s[i];
        const int v = (c >= '0' && c <= '9') ? c - '0'
                    : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                    : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (v < 0) return false;
        if (i % 2 == 0) out[i / 2] = static_cast<uint8_t>(v << 4);
        else out[i / 2] |= static_cast<uint8_t>(v);
    }
    return true;
}

#if !defined(__WASM_PORT__) && !defined(FROZEN_BUBBLE_TEST_ACCESS)
std::string CodeFile() {
    GameSettings* gs = GameSettings::Instance();
    gs->InitPrefPath();
    return std::string(gs->prefPath ? gs->prefPath : "") + "account.txt";
}
#endif

std::string LoadCode() {
#if defined(FROZEN_BUBBLE_TEST_ACCESS)
    return "";  // and never adopt the player's real account either
#elif defined(__WASM_PORT__)
    char* saved = (char*)EM_ASM_PTR({
        var c = localStorage.getItem('fb_account');
        if (!c) return 0;
        var len = lengthBytesUTF8(c) + 1;
        var buf = _malloc(len);
        stringToUTF8(c, buf, len);
        return buf;
    });
    std::string c = saved ? saved : "";
    free(saved);
    return NormalizeCode(c);
#else
    FILE* f = std::fopen(CodeFile().c_str(), "r");
    if (!f) return "";
    char buf[64] = "";
    if (!std::fgets(buf, sizeof(buf), f)) buf[0] = '\0';
    std::fclose(f);
    return NormalizeCode(buf);
#endif
}

void SaveCode(const std::string& c) {
#if defined(FROZEN_BUBBLE_TEST_ACCESS)
    // Test binaries share the real per-user settings directory; a test that
    // connects to a 1.6 server must not leave an account behind in it.
    (void)c;
    return;
#elif defined(__WASM_PORT__)
    EM_ASM({ localStorage.setItem('fb_account', UTF8ToString($0)); }, c.c_str());
#else
    FILE* f = std::fopen(CodeFile().c_str(), "w");
    if (!f) return;  // still usable this session, just not remembered
    std::fprintf(f, "%s\n", c.c_str());
    std::fclose(f);
#endif
}

std::string NewCode() {
    // 16 base32 chars = 80 bits; each takes 5 bits of 10 random bytes.
    uint8_t raw[10];
    if (!RandomBytes(raw, sizeof(raw))) return "";
    std::string c;
    uint32_t acc = 0;
    int bits = 0;
    for (uint8_t b : raw) {
        acc = (acc << 8) | b;
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            c += kAlphabet[(acc >> bits) & 31];
        }
    }
    crypto_wipe(raw, sizeof(raw));
    return c;
}

// Argon2i stretches the code before it becomes a key. 80 random bits are
// already far past guessing; this is margin on top, at a one-time cost of
// a few tens of milliseconds per launch (4 MiB, 3 passes).
void DeriveKeys() {
    if (keysReady || Code().empty()) return;
    constexpr uint32_t kBlocks = 4096;
    std::vector<uint8_t> work(static_cast<size_t>(kBlocks) * 1024);
    crypto_argon2_config config = {CRYPTO_ARGON2_I, kBlocks, 3, 1};
    crypto_argon2_inputs inputs = {
        reinterpret_cast<const uint8_t*>(code.data()),
        reinterpret_cast<const uint8_t*>(kSalt),
        static_cast<uint32_t>(code.size()),
        static_cast<uint32_t>(sizeof(kSalt) - 1)};
    uint8_t seed[32];
    crypto_argon2(seed, sizeof(seed), work.data(), config, inputs, crypto_argon2_no_extras);
    crypto_wipe(work.data(), work.size());
    crypto_eddsa_key_pair(secretKey, publicKey, seed);  // wipes seed
    keysReady = true;
}

}  // namespace

std::string NormalizeCode(const std::string& typed) {
    std::string out;
    for (char ch : typed) {
        if (ch == '-' || ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') continue;
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
        if (ch == 'O') ch = '0';
        if (ch == 'I' || ch == 'L') ch = '1';
        if (!std::strchr(kAlphabet, ch) || ch == '\0') return "";
        out += ch;
    }
    return out.size() == kCodeLen ? out : "";
}

std::string FormatCode(const std::string& c) {
    std::string out;
    for (size_t i = 0; i < c.size(); ++i) {
        if (i && i % 4 == 0) out += '-';
        out += c[i];
    }
    return out;
}

std::string Code() {
    if (code.empty()) code = LoadCode();
    if (code.empty()) {
        code = NewCode();
        if (!code.empty()) SaveCode(code);
    }
    return code;
}

bool UseCode(const std::string& typed, bool save) {
    const std::string c = NormalizeCode(typed);
    if (c.empty()) return false;
    code = c;
    keysReady = false;
    crypto_wipe(secretKey, sizeof(secretKey));
    if (save) SaveCode(code);
    return true;
}

std::string PublicKeyHex() {
    DeriveKeys();
    return keysReady ? ToHex(publicKey, sizeof(publicKey)) : "";
}

std::string SignChallenge(const std::string& nonceHex) {
    uint8_t nonce[32];
    if (!FromHex(nonceHex, nonce, sizeof(nonce))) return "";
    DeriveKeys();
    if (!keysReady) return "";
    const std::string msg = kSignedPrefix + nonceHex;
    uint8_t sig[64];
    crypto_eddsa_sign(sig, secretKey, reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
    return ToHex(sig, sizeof(sig));
}

}  // namespace playeraccount
