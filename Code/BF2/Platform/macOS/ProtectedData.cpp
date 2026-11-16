// [CHOSEN, not recovered] Native profile protection; Linux: issue #36. See ProtectedData.h.
#include "ProtectedData.h"

#if (defined(__APPLE__) || defined(BF2_LINUX_CLIENT)) && !(defined(DEDICATED) && DEDICATED)

#if defined(__APPLE__)
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>
#else
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <climits>
#include <memory>
#endif
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace dice {
namespace hfe {
namespace macos {

namespace {

const unsigned char kMagic[4] = { 'O', 'B', 'F', '2' };
const unsigned char kVersion = 1;
const std::size_t kKeyBytes = 32;               // AES-256 key, then the HMAC-SHA256 key
const std::size_t kIvBytes = 16;
const std::size_t kMacBytes = 32;
const std::size_t kHeaderBytes = sizeof(kMagic) + 1 + kIvBytes;

#if defined(BF2_LINUX_CLIENT)
struct EraseOnExit {
    unsigned char* bytes;
    std::size_t size;
    ~EraseOnExit() { OPENSSL_cleanse(bytes, size); }
};
#endif

bool randomBytes(unsigned char* data, std::size_t size)
{
#if defined(__APPLE__)
    arc4random_buf(data, size);
    return true;
#else
    return size <= INT_MAX && RAND_bytes(data, static_cast<int>(size)) == 1;
#endif
}

std::string homeDirectory()
{
    if (const char* home = getenv("HOME"))
        if (*home)
            return home;
    if (const passwd* pw = getpwuid(getuid()))
        if (pw->pw_dir)
            return pw->pw_dir;
    return std::string();
}

// Creates every missing directory of path with mode 0700; existing ones are left as they are.
void makeDirectories(const std::string& path)
{
    for (std::string::size_type at = 1; at <= path.size(); ++at)
        if (at == path.size() || path[at] == '/')
            mkdir(path.substr(0, at).c_str(), 0700);
}

bool readAll(int fd, unsigned char* out, std::size_t size)
{
    std::size_t done = 0;
    while (done < size) {
        const ssize_t n = read(fd, out + done, size - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

bool writeAll(int fd, const unsigned char* data, std::size_t size)
{
    std::size_t done = 0;
    while (done < size) {
        const ssize_t n = write(fd, data + done, size - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

// Reads the 64 key bytes; creates the file (and its directory) first when create is set and it
// does not exist. Only a regular file of exactly 64 bytes is accepted.
bool loadKeys(const std::string& path, bool create, unsigned char keys[2 * kKeyBytes])
{
    if (path.empty())
        return false;
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0 && errno == ENOENT && create) {
        const std::string::size_type slash = path.rfind('/');
        if (slash != std::string::npos && slash > 0)
            makeDirectories(path.substr(0, slash));
        unsigned char fresh[2 * kKeyBytes];
#if defined(BF2_LINUX_CLIENT)
        EraseOnExit eraseFresh{fresh, sizeof(fresh)};
#endif
        if (!randomBytes(fresh, sizeof(fresh))) return false;
        const int wfd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
        if (wfd >= 0) {
            const bool ok = writeAll(wfd, fresh, sizeof(fresh)) && fsync(wfd) == 0;
            close(wfd);
            memset(fresh, 0, sizeof(fresh));
            if (!ok) {
                unlink(path.c_str());
                return false;
            }
        }
        // Either this call created it or another process won the race; read what is there.
        fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    }
    if (fd < 0)
        return false;
    struct stat st;
    const bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode)
                    && st.st_size == static_cast<off_t>(2 * kKeyBytes)
                    && readAll(fd, keys, 2 * kKeyBytes);
    close(fd);
    return ok;
}

bool mac(const unsigned char* macKey, const unsigned char* data, std::size_t size,
         unsigned char out[kMacBytes])
{
#if defined(__APPLE__)
    CCHmac(kCCHmacAlgSHA256, macKey, kKeyBytes, data, size, out);
    return true;
#else
    unsigned int written = 0;
    return HMAC(EVP_sha256(), macKey, static_cast<int>(kKeyBytes), data, size, out, &written)
        && written == kMacBytes;
#endif
}

bool crypt(bool encrypt, const unsigned char* key, const unsigned char* iv,
           const unsigned char* data, std::size_t size, unsigned char* out,
           std::size_t capacity, std::size_t& written)
{
#if defined(__APPLE__)
    return CCCrypt(encrypt ? kCCEncrypt : kCCDecrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding,
                   key, kKeyBytes, iv, data, size, out, capacity, &written) == kCCSuccess;
#else
    // EVP uses signed int input/output lengths and requires one spare AES block.
    // Reject oversized profile values before narrowing or allocating their buffers.
    if (size > INT_MAX - kIvBytes || capacity < size + kIvBytes) return false;
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>
        context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int first = 0, last = 0;
    if (!context || EVP_CipherInit_ex(context.get(), EVP_aes_256_cbc(), nullptr, key, iv, encrypt ? 1 : 0) != 1
        || EVP_CIPHER_CTX_set_padding(context.get(), 1) != 1
        || EVP_CipherUpdate(context.get(), out, &first, data, static_cast<int>(size)) != 1
        || first < 0 || static_cast<std::size_t>(first) > capacity - kIvBytes
        || EVP_CipherFinal_ex(context.get(), out + first, &last) != 1
        || last < 0 || static_cast<std::size_t>(last) > capacity - static_cast<std::size_t>(first))
        return false;
    written = static_cast<std::size_t>(first) + static_cast<std::size_t>(last);
    return true;
#endif
}

} // namespace

std::string defaultProtectedDataKeyPath()
{
#if defined(BF2_LINUX_CLIENT)
    if (const char* data = getenv("XDG_DATA_HOME"))
        if (*data == '/') return std::string(data) + "/OpenBF2/ProtectedData.key";
#endif
    const std::string home = homeDirectory();
    if (home.empty())
        return std::string();
#if defined(BF2_LINUX_CLIENT)
    return home + "/.local/share/OpenBF2/ProtectedData.key";
#else
    return home + "/Library/Application Support/OpenBF2/ProtectedData.key";
#endif
}

bool protectDataWithKeyFile(const std::string& keyPath, const unsigned char* data, std::size_t size,
                            DATA_BLOB* out)
{
#if defined(BF2_LINUX_CLIENT)
    if ((!data && size) || size > INT_MAX - kIvBytes) return false;
#endif
    unsigned char keys[2 * kKeyBytes];
#if defined(BF2_LINUX_CLIENT)
    EraseOnExit eraseKeys{keys, sizeof(keys)};
#endif
    if (!out || !loadKeys(keyPath, true, keys))
        return false;
    std::vector<unsigned char> blob(kHeaderBytes + size + kIvBytes + kMacBytes);
    memcpy(&blob[0], kMagic, sizeof(kMagic));
    blob[sizeof(kMagic)] = kVersion;
    unsigned char* iv = &blob[sizeof(kMagic) + 1];
    if (!randomBytes(iv, kIvBytes)) {
        memset(keys, 0, sizeof(keys));
        return false;
    }
    std::size_t cipherBytes = 0;
    if (!crypt(true, keys, iv, data, size, &blob[kHeaderBytes], size + kIvBytes, cipherBytes)) {
        memset(keys, 0, sizeof(keys));
        return false;
    }
    const std::size_t macAt = kHeaderBytes + cipherBytes;
    const bool authenticated = mac(keys + kKeyBytes, &blob[0], macAt, &blob[macAt]);
    memset(keys, 0, sizeof(keys));
    if (!authenticated) return false;
    const std::size_t total = macAt + kMacBytes;
    unsigned char* result = static_cast<unsigned char*>(malloc(total));
    if (!result)
        return false;
    memcpy(result, &blob[0], total);
    out->cbData = static_cast<std::uint32_t>(total);
    out->pbData = result;
    return true;
}

bool unprotectDataWithKeyFile(const std::string& keyPath, const unsigned char* data,
                              std::size_t size, DATA_BLOB* out)
{
#if defined(BF2_LINUX_CLIENT)
    if (size > INT_MAX) return false;
#endif
    if (!out || !data || size < kHeaderBytes + kIvBytes + kMacBytes
        || memcmp(data, kMagic, sizeof(kMagic)) != 0 || data[sizeof(kMagic)] != kVersion
        || (size - kHeaderBytes - kMacBytes) % kIvBytes != 0)
        return false;
    unsigned char keys[2 * kKeyBytes];
#if defined(BF2_LINUX_CLIENT)
    EraseOnExit eraseKeys{keys, sizeof(keys)};
#endif
    if (!loadKeys(keyPath, false, keys))
        return false;
    const std::size_t macAt = size - kMacBytes;
    unsigned char expected[kMacBytes];
    if (!mac(keys + kKeyBytes, data, macAt, expected)) {
        memset(keys, 0, sizeof(keys));
        return false;
    }
#if defined(BF2_LINUX_CLIENT)
    const bool different = CRYPTO_memcmp(expected, data + macAt, kMacBytes) != 0;
#else
    unsigned char diff = 0;
    for (std::size_t i = 0; i < kMacBytes; ++i)
        diff |= static_cast<unsigned char>(expected[i] ^ data[macAt + i]);
    const bool different = diff != 0;
#endif
    if (different) {
        memset(keys, 0, sizeof(keys));
        return false;
    }
    const std::size_t cipherBytes = macAt - kHeaderBytes;
#if defined(BF2_LINUX_CLIENT)
    const std::size_t plainCapacity = cipherBytes + kIvBytes;
#else
    const std::size_t plainCapacity = cipherBytes;
#endif
    unsigned char* plain = static_cast<unsigned char*>(malloc(plainCapacity ? plainCapacity : 1));
    if (!plain) {
        memset(keys, 0, sizeof(keys));
        return false;
    }
    std::size_t plainBytes = 0;
    const bool decrypted = crypt(false, keys, data + sizeof(kMagic) + 1,
                                 data + kHeaderBytes, cipherBytes, plain, plainCapacity, plainBytes);
    memset(keys, 0, sizeof(keys));
    if (!decrypted) {
#if defined(BF2_LINUX_CLIENT)
        OPENSSL_cleanse(plain, plainCapacity);
#endif
        free(plain);
        return false;
    }
    out->cbData = static_cast<std::uint32_t>(plainBytes);
    out->pbData = plain;
    return true;
}

bool protectData(const unsigned char* data, std::size_t size, DATA_BLOB* out)
{
    return protectDataWithKeyFile(defaultProtectedDataKeyPath(), data, size, out);
}

bool unprotectData(const unsigned char* data, std::size_t size, DATA_BLOB* out)
{
    return unprotectDataWithKeyFile(defaultProtectedDataKeyPath(), data, size, out);
}

void freeProtectedData(void* data)
{
    free(data);
}

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // native client
