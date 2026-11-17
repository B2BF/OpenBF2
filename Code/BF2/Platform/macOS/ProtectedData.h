// [CHOSEN, not recovered] Native CryptProtectData/CryptUnprotectData counterpart for the
// profile password (Game/PasswordCodec.cpp) and the CD key (Game/MatchMaking/Client/CdKeyStore.cpp).
// Owner decision: docs/macos-port.md, "Protected profile data (DPAPI)"; Linux: issue #36.
//
// A protected blob is "OBF2", a version byte (1), a 16-byte IV, the AES-256-CBC (PKCS#7)
// ciphertext and an HMAC-SHA256 over everything before it. The two 256-bit keys are random and
// live in one per-user key file (mode 0600 in a 0700 directory), created on the first protect.
// macOS uses ~/Library/Application Support/OpenBF2/ProtectedData.key; Linux uses
// $XDG_DATA_HOME/OpenBF2/ProtectedData.key, defaulting to ~/.local/share/OpenBF2/ProtectedData.key.
// Like DPAPI, the data is bound to this
// user on this machine: a copied profile does not reveal the secret without the key file. A
// Windows DPAPI blob is not readable here (nor is this blob on Windows); both sides take their
// shipped decode-failure path.

#ifndef PLATFORM_MACOS_PROTECTEDDATA_H
#define PLATFORM_MACOS_PROTECTEDDATA_H

#include <cstddef>
#include <cstdint>
#include <string>

// DATA_BLOB's shape (wincrypt.h), under the tag PasswordCodec.h and CdKeyStore.h forward-declare.
struct _CRYPTOAPI_BLOB
{
    std::uint32_t  cbData;
    unsigned char* pbData;
};
typedef _CRYPTOAPI_BLOB DATA_BLOB;

namespace dice {
namespace hfe {
namespace macos {

// The per-user key file the two functions below use.
std::string defaultProtectedDataKeyPath();

// Protects size bytes into out (pbData from malloc; release with freeProtectedData). Creates the
// key file when it does not exist. Returns false, with out untouched, on any failure.
bool protectData(const unsigned char* data, std::size_t size, DATA_BLOB* out);
// Verifies and decrypts a blob made by protectData into out (pbData from malloc). Returns false
// for anything else: another format, a wrong key, a damaged blob or a missing key file.
bool unprotectData(const unsigned char* data, std::size_t size, DATA_BLOB* out);
// LocalFree's counterpart for the pbData the two functions hand out; null is ignored.
void freeProtectedData(void* data);

// The same with an explicit key file, for tests.
bool protectDataWithKeyFile(const std::string& keyPath, const unsigned char* data, std::size_t size,
                            DATA_BLOB* out);
bool unprotectDataWithKeyFile(const std::string& keyPath, const unsigned char* data,
                              std::size_t size, DATA_BLOB* out);

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // PLATFORM_MACOS_PROTECTEDDATA_H
