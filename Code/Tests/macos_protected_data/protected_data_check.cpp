// [TEMP] [CHOSEN, not recovered] Checks the native DPAPI counterpart (Platform/macOS/ProtectedData)
// through the production PasswordCodec: round trip, the Profile.con hex form, tampering, a wrong
// key, a missing key file, a foreign (DPAPI-shaped) blob, the key file's permissions and the CD-key
// translation (CdKeyStore). Run it
// Linux creates an isolated XDG_DATA_HOME under its argument directory. The existing
// macOS runner supplies its own scratch home. No original profiles are read.
// [RETIRE-WHEN: native-protected-data-tests] Fold into the tree's unit-test runner.
#include "../../BF2/Game/PasswordCodec.h"
#include "../../BF2/Game/MatchMaking/Client/CdKeyStore.h"
#include "../../BF2/Platform/macOS/ProtectedData.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <climits>
#include <fcntl.h>
#include <vector>
#if defined(BF2_LINUX_CLIENT)
#include <openssl/evp.h>
// [TEMP] Linker wrappers force real API failure paths without changing production code.
static bool failRandom = false, failCipher = false;
extern "C" int __real_RAND_bytes(unsigned char*, int);
extern "C" int __wrap_RAND_bytes(unsigned char* out, int size)
{ return failRandom ? 0 : __real_RAND_bytes(out, size); }
extern "C" int __real_EVP_CipherFinal_ex(EVP_CIPHER_CTX*, unsigned char*, int*);
extern "C" int __wrap_EVP_CipherFinal_ex(EVP_CIPHER_CTX* context, unsigned char* out, int* size)
{ return failCipher ? 0 : __real_EVP_CipherFinal_ex(context, out, size); }
#endif

using namespace dice::hfe;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main(int argc, char** argv)
{
#if defined(BF2_LINUX_CLIENT)
    if (argc != 2) return 2;
    const std::string pattern = std::string(argv[1]) + "/bf2-protected-XXXXXX";
    std::vector<char> scratch(pattern.begin(), pattern.end()); scratch.push_back(0);
    if (!mkdtemp(scratch.data()) || setenv("XDG_DATA_HOME", scratch.data(), 1) != 0) return 1;
#else
    (void)argc; (void)argv;
    const char* home = std::getenv("HOME");
    CHECK(home && std::strstr(home, "/out/"));          // never the user's real home
    if (!home || !std::strstr(home, "/out/"))
        return 1;
#endif
    const std::string keyPath = macos::defaultProtectedDataKeyPath();
    unlink(keyPath.c_str());

    // Missing key file: nothing decodes, and decode leaves the text unchanged.
    std::string text("4f42463201aabbccdd");
    CHECK(!PasswordCodec::decode(text) && text == "4f42463201aabbccdd");

    // Encode creates the key; the hex written to Profile.con decodes back to the password.
    std::string hex;
    {
        PasswordCodec codec;
        CHECK(!codec.encode(""));
        CHECK(codec.encode("s3cret pass"));
        std::stringstream out;
        codec.write(out);
        hex = out.str();
    }
    CHECK(hex.size() > 2 * (4 + 1 + 16 + 16 + 32) - 1 && hex.find_first_not_of("0123456789abcdef") == std::string::npos);
    CHECK(hex.compare(0, 10, "4f42463201") == 0);
    std::string decoded(hex);
    CHECK(PasswordCodec::decode(decoded) && decoded == "s3cret pass");

    // Two encodings differ (random IV) and both decode.
    std::string hex2;
    { PasswordCodec codec; codec.encode("s3cret pass"); std::stringstream out; codec.write(out); hex2 = out.str(); }
    CHECK(hex2 != hex);

    // Key file: regular, 64 bytes, mode 0600.
    struct stat st;
    CHECK(stat(keyPath.c_str(), &st) == 0 && st.st_size == 64 && (st.st_mode & 0777) == 0600);
#if defined(BF2_LINUX_CLIENT)
    CHECK(keyPath == std::string(scratch.data()) + "/OpenBF2/ProtectedData.key");
    CHECK(stat((std::string(scratch.data()) + "/OpenBF2").c_str(), &st) == 0 && (st.st_mode & 0777) == 0700);
#endif

    // A flipped ciphertext nibble fails the MAC and leaves the text unchanged.
    std::string tampered(hex);
    tampered[2 * 30] = tampered[2 * 30] == '0' ? '1' : '0';
    std::string t2(tampered);
    CHECK(!PasswordCodec::decode(t2) && t2 == tampered);

    // A DPAPI-shaped blob (starts 01000000d08c9ddf) is not this format.
    std::string dpapi("01000000d08c9ddf0115d1118c7a00c04fc297eb0100000000000000");
    std::string d2(dpapi);
    CHECK(!PasswordCodec::decode(d2) && d2 == dpapi);

    // The CD key: an unmarked key comes back marked ("x9392" + hex), and the marked form
    // translates back to the key; a marked payload that does not decode is returned verbatim.
    const std::string marked = translateCdKey("ABCD-EFGH-IJKL-MNOP");
    CHECK(marked.compare(0, 5, "x9392") == 0 && marked.size() > 5 + 2 * 64);
    CHECK(translateCdKey(marked) == "ABCD-EFGH-IJKL-MNOP");
    CHECK(translateCdKey("x9392abcdef") == "abcdef");

    // Fixed AES-CBC/PKCS7 + HMAC vector made with openssl enc and Python hmac,
    // independently of protectData. Key bytes are 0..63, IV bytes 0xa0..0xaf.
    const char goldenHex[] =
        "4f42463201a0a1a2a3a4a5a6a7a8a9aaabacadaeaf3fea9d4114f115866fcaaf1f82ad2d82"
        "d4ef21235d7c523bb89bc921fed4de695c74350ca65b637c27507f869c8445f70789a194b038e2a308dff4f610b046c1";
    std::vector<unsigned char> golden((sizeof(goldenHex) - 1) / 2);
    for (std::size_t i = 0; i < golden.size(); ++i) {
        const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        golden[i] = static_cast<unsigned char>(nibble(goldenHex[i * 2]) * 16 + nibble(goldenHex[i * 2 + 1]));
    }
    const std::string goldenKey = keyPath + ".fixture";
    const int fixtureFile = open(goldenKey.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
    CHECK(fixtureFile >= 0);
    if (fixtureFile >= 0) {
        unsigned char fixtureKeys[64];
        for (unsigned i = 0; i < sizeof(fixtureKeys); ++i) fixtureKeys[i] = static_cast<unsigned char>(i);
        CHECK(write(fixtureFile, fixtureKeys, sizeof(fixtureKeys)) == sizeof(fixtureKeys));
        close(fixtureFile);
        DATA_BLOB plain = {};
        CHECK(macos::unprotectDataWithKeyFile(goldenKey, golden.data(), golden.size(), &plain));
        const char expected[] = "native-profile-format";
        CHECK(plain.cbData == sizeof(expected) && plain.pbData && std::memcmp(plain.pbData, expected, sizeof(expected)) == 0);
        macos::freeProtectedData(plain.pbData);
        unlink(goldenKey.c_str());
    }

    // Block boundaries and zero-length binary data use the same blob policy.
    for (std::size_t size : {0u, 1u, 15u, 16u, 17u, 31u, 32u, 1024u}) {
        std::vector<unsigned char> plain(size);
        for (std::size_t i = 0; i < size; ++i) plain[i] = static_cast<unsigned char>(i);
        DATA_BLOB encrypted = {}, restored = {};
        const bool encoded = macos::protectData(plain.data(), plain.size(), &encrypted);
        CHECK(encoded);
        if (!encoded) continue;
        CHECK(macos::unprotectData(encrypted.pbData, encrypted.cbData, &restored));
        CHECK(restored.cbData == size && (!size || std::memcmp(restored.pbData, plain.data(), size) == 0));
        macos::freeProtectedData(restored.pbData);
        // Every protected byte is authenticated, including version and IV.
        if (size == 17) {
#if defined(BF2_LINUX_CLIENT)
            failCipher = true;
            DATA_BLOB rejectedCrypto = {123, nullptr};
            CHECK(!macos::unprotectData(encrypted.pbData, encrypted.cbData, &rejectedCrypto));
            CHECK(rejectedCrypto.cbData == 123 && rejectedCrypto.pbData == nullptr);
            failCipher = false;
#endif
            for (std::size_t i = 0; i < encrypted.cbData; ++i) {
                encrypted.pbData[i] ^= 1;
                DATA_BLOB rejected = {123, nullptr};
                CHECK(!macos::unprotectData(encrypted.pbData, encrypted.cbData, &rejected));
                CHECK(rejected.cbData == 123 && rejected.pbData == nullptr);
                encrypted.pbData[i] ^= 1;
            }
            for (std::size_t truncated = 0; truncated < encrypted.cbData; ++truncated) {
                DATA_BLOB rejected = {123, nullptr};
                CHECK(!macos::unprotectData(encrypted.pbData, truncated, &rejected));
                CHECK(rejected.cbData == 123 && rejected.pbData == nullptr);
            }
        }
        macos::freeProtectedData(encrypted.pbData);
    }
#if defined(BF2_LINUX_CLIENT)
    unsigned char byte = 0;
    DATA_BLOB untouched = {123, &byte};
    CHECK(!macos::protectData(&byte, static_cast<std::size_t>(INT_MAX), &untouched));
    CHECK(!macos::unprotectData(&byte, static_cast<std::size_t>(INT_MAX) + 1, &untouched));
    CHECK(!macos::protectData(nullptr, 1, &untouched));
    CHECK(untouched.cbData == 123 && untouched.pbData == &byte);
    failRandom = true;
    CHECK(!macos::protectData(&byte, 1, &untouched)); // IV generation, existing key.
    const std::string failedKey = std::string(scratch.data()) + "/rng-failure.key";
    CHECK(!macos::protectDataWithKeyFile(failedKey, &byte, 1, &untouched));
    CHECK(access(failedKey.c_str(), F_OK) != 0);     // Key generation failed before file creation.
    failRandom = false;
    failCipher = true;
    CHECK(!macos::protectData(&byte, 1, &untouched));
    failCipher = false;
    CHECK(untouched.cbData == 123 && untouched.pbData == &byte);
#endif

    // Another key file: the blob does not decode.
    unlink(keyPath.c_str());
    { PasswordCodec codec; codec.encode("other"); }      // makes a fresh key
    std::string d3(hex);
    CHECK(!PasswordCodec::decode(d3) && d3 == hex);

#if defined(BF2_LINUX_CLIENT)
    unlink(keyPath.c_str());
    rmdir((std::string(scratch.data()) + "/OpenBF2").c_str());
    rmdir(scratch.data());
#endif

    std::printf(failures ? "protected_data_check: %d failure(s)\n" : "protected_data_check: all passed\n", failures);
    return failures ? 1 : 0;
}
