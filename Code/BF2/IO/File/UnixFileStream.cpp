// POSIX file-stream operations, split from the original UnixFileSystem translation unit.
// [CHOSEN, not recovered] file organization and filesystem-access gate.
// [RETIRE-WHEN: unixfilesystem-friend-unixfilestream] Remove the remaining conditional pairs after
// validating the granted friendship/static error-hook declarations across supported builds.
// Access is enabled by default. Disabling the gate omits open-count updates and the empty error hook.

#include "UnixFileStream.h"

#if defined(__unix__) || defined(__linux__) || defined(BF2_PLATFORM_POSIX)

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../Core/Ids.h"
#include "../../Core/StringUtil.h"   // dice::hfe::extractFilePath, 0x83D0BC0 (attested TU)
#include "UnixFileSystem.h"

#ifndef BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS
#define BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS 1   // UnixFileSystem.h now grants the access; gates kept until the pairs are deleted
#endif
#if !BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS
#pragma message("IO/File/UnixFileStream.cpp: BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS=0 - the UnixFileSystem open-stream count and logError calls are compiled out; see the access-gate note")
#endif

namespace dice {
namespace hfe {
namespace io {

// Starts with one reference and an invalid descriptor, borrowing fileSystem.
UnixFileStream::UnixFileStream(UnixFileSystem& fileSystem)
    : m_refCount(1)
    , m_fd(-1)
    , m_access(FileAccess(0))
    , m_unknown14(0)
    , m_fileSystem(fileSystem)
{
}


// Duplicates the source descriptor without checking failure. The auxiliary string starts empty rather than being copied.
UnixFileStream::UnixFileStream(const UnixFileStream& other)
    : m_refCount(1)
    , m_fd(-1)
    , m_access(other.m_access)
    , m_unknown14(other.m_unknown14)
    , m_fileSystem(other.m_fileSystem)
{
    m_fd = dup(other.m_fd);
#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
    ++m_fileSystem.m_openStreamCount;
#endif
}

// 0x84A7DB0 (D2) / 0x84A7E00 (D1), 70 B; 0x84A7E50 (D0), 84 B. close(), then the string member
// is torn down; D0 adds operator delete.
UnixFileStream::~UnixFileStream()
{
    close();
}

// IBase quartet


void UnixFileStream::addRef()
{
    ++m_refCount;
}


int UnixFileStream::getRef()
{
    return m_refCount;
}

// Releases one reference, deleting this stream at zero. Returns the remaining count.
int UnixFileStream::release()
{
    int remaining = --m_refCount;

    if (remaining == 0 && this != 0)
        delete this;

    return remaining;
}

// Three accepted ids, in this order; no IAsyncStream, no adjustment.
void* UnixFileStream::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IStream || iid == IID_INonlinearStream)
        return const_cast<UnixFileStream*>(this);

    return 0;
}

// IStream


// Reads up to size bytes and returns the count transferred before EOF or error.
// Requests of 0x80000000 bytes or more return zero because the internal count is signed.
unsigned int UnixFileStream::read(void* buffer, unsigned int size)
{
    char* p = static_cast<char*>(buffer);
    int left = static_cast<int>(size);

    while (left > 0)
    {
        int n = static_cast<int>(::read(m_fd, p, left));
        if (n == 0)
            break;
        if (n == -1)
        {
#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
            UnixFileSystem::logError("read", std::string("<unknown>"));
#endif
            break;
        }
        left -= n;
        p += n;
    }

    return size - left;
}

// Byte-for-byte the shape of read() over ::write; the tag is "write".
unsigned int UnixFileStream::write(const void* buffer, unsigned int size)
{
    const char* p = static_cast<const char*>(buffer);
    int left = static_cast<int>(size);

    while (left > 0)
    {
        int n = static_cast<int>(::write(m_fd, p, left));
        if (n == 0)
            break;
        if (n == -1)
        {
#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
            UnixFileSystem::logError("write", std::string("<unknown>"));
#endif
            break;
        }
        left -= n;
        p += n;
    }

    return size - left;
}

// A direct lseek, not a call through the virtual seek slot, and it returns
// `lseek(...) != -1` (setnz/movzx) - no logError on failure, unlike seek().
// [PLATFORM] `offset` is unsigned; retail's 32-bit off_t reinterprets values >= 2^31 as a backward
// seek, a 64-bit off_t does not. Written as dice wrote it, with the plain conversion.
int UnixFileStream::skip(unsigned int offset)
{
    return lseek(m_fd, offset, SEEK_CUR) != -1;
}

// Returns size minus current position, querying size first.
int UnixFileStream::getAvailableBytes()
{
    int size     = getSize();
    int position = getPosition();
    return size - position;
}

// `cmp [this+0Ch], 1; setbe` - the unsigned range fold of `== 0 || == 1`.
// m_access is never assigned on this platform (UnixFileStream.h), so this is always true.
bool UnixFileStream::canRead()
{
    return m_access == FileAccess(0) || m_access == FileAccess(1);
}

// `dec; cmp 1; setbe` - the fold of `== 1 || == 2`. Always false here, likewise.
bool UnixFileStream::canWrite()
{
    return m_access == FileAccess(1) || m_access == FileAccess(2);
}

// `new UnixFileStream(*this)` and nothing else: no queryInterface, no null test
// (operator new throws). The copy constructor dup()s the descriptor, so the clone shares the
// open file description - and therefore the file offset - with the original.
IStream* UnixFileStream::clone()
{
    return new UnixFileStream(*this);
}

// Seeks from the requested origin; unknown origins use the current position. Returns one on success, zero on error.
int UnixFileStream::seek(SeekOrigin origin, int offset)
{
    int whence = SEEK_CUR;
    switch (origin)
    {
    case SEEKORIGIN_BEGIN:   whence = SEEK_SET; break;
    case SEEKORIGIN_CURRENT: whence = SEEK_CUR; break;
    case SEEKORIGIN_END:     whence = SEEK_END; break;
    }

    if (lseek(m_fd, offset, whence) == -1)
    {
#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
        UnixFileSystem::logError("lseek", std::string("<unknown>"));
#endif
        return 0;
    }
    return 1;
}

// lseek's raw result, -1 included; nothing checks it.
int UnixFileStream::getPosition()
{
    return static_cast<int>(lseek(m_fd, 0, SEEK_CUR));
}

// Measures size by seeking to the end and restoring position. Returns zero on either query failure;
// restore failure is unchecked and can leave the stream positioned at the end.
int UnixFileStream::getSize()
{
    off_t current = lseek(m_fd, 0, SEEK_CUR);
    if (current == -1)
    {
#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
        UnixFileSystem::logError("lseek", std::string("<unknown>"));
#endif
        return 0;
    }

    off_t end = lseek(m_fd, 0, SEEK_END);
    if (end == -1)
    {
#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
        UnixFileSystem::logError("lseek", std::string("<unknown>"));
#endif
        return 0;
    }

    lseek(m_fd, current, SEEK_SET);
    return static_cast<int>(end);
}

// Opens name with the requested mode and access; flags are unused.
// Creates parent directories for creation modes. Failure returns false and leaves the descriptor at -1.
bool UnixFileStream::open(const std::string& name, FileMode mode, FileAccess access, unsigned int)
{
    std::string file(name);
    UnixFileSystem::fixUnixPath(file);

    int flags = 0;                                          // O_RDONLY for 0 and for anything else
    switch (static_cast<int>(access))
    {
    case 1:  flags = O_RDWR;   break;                       // read/write
    case 2:  flags = O_WRONLY; break;                       // write only
    }

    switch (static_cast<int>(mode))
    {
    case 0:  flags |= O_CREAT | O_TRUNC;          break;    // create always            (0x240)
    case 1:  flags |= O_CREAT | O_TRUNC | O_EXCL; break;    // create new               (0x2C0)
    case 2:                                       break;    // open existing (io::FILEMODE_READ)
    case 3:  flags |= O_CREAT;                    break;    // open always              (0x040)
    }

    if (mode == FileMode(0) || mode == FileMode(1))
        m_fileSystem.createDirectory(extractFilePath(file, '/'));
#if (defined(__APPLE__) || defined(BF2_LINUX_CLIENT)) && !DEDICATED
    // [CHOSEN, not recovered] The macOS client stands in for the Win32 client, whose stream also
    // creates the parent directories for open-always (Win32FileSystem.cpp openFile: modes 0, 1
    // and 3); the ELF does it for modes 0 and 1 only. The screenshot capture opens its file with
    // mode 3 under @HOME@/Screenshots, which nothing else creates.
    else if (mode == FileMode(3))
        m_fileSystem.createDirectory(extractFilePath(file, '/'));
#endif

    m_fd = ::open(file.c_str(), flags, 0600);               // 0x180: owner read/write only
    if (m_fd == -1)
    {
#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
        UnixFileSystem::logError("open", file);
#endif
        return false;
    }

#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
    ++m_fileSystem.m_openStreamCount;
#endif
    return true;
}


// Closes the descriptor and decrements the open count even if close fails.
// The descriptor is not reset, so a manual close can be repeated by destruction.
void UnixFileStream::close()
{
    if (m_fd == -1)
        return;

    if (::close(m_fd) == -1)
    {
#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
        UnixFileSystem::logError("close", std::string("<unknown>"));
#endif
    }

#if BF2_UNIXFILESTREAM_FILESYSTEM_ACCESS    // see the access gate
    --m_fileSystem.m_openStreamCount;
#endif
}

} // namespace io
} // namespace hfe
} // namespace dice

#endif // POSIX
