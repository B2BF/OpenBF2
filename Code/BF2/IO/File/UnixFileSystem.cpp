// POSIX filesystem implementation. Windows builds use their own filesystem implementation.

#include "UnixFileSystem.h"

#if defined(__unix__) || defined(__linux__) || defined(BF2_PLATFORM_POSIX)

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../Core/Debug.h"
#include "../../Core/Ids.h"
#include "UnixFileStream.h"
#if defined(__APPLE__) && !DEDICATED
#include "../../Platform/macOS/ClientPaths.h"
#endif

namespace dice {
namespace hfe {

// [UNRESOLVED] home. Declared rather than included — it is a free function in dice::hfe used by
// writeFile to find the directory part of a path. `extractFilePath(path, '/')`.
std::string extractFilePath(const std::string& path, char separator);

namespace io {
namespace {

// Fills directory and size information from stat; timestamps are zero except in the macOS client.
// The caller supplies the name.
void buildFileInfo(FileInfo* info, const struct stat* st)
{
    info->isDirectory = (st->st_mode & S_IFMT) == S_IFDIR;   // stat + 16
    info->size        = st->st_size;

    info->creationTimeLow    = 0;                             // the three filetime pairs: Linux
    info->creationTimeHigh   = 0;                             //   leaves every one of them zero
    info->lastAccessTimeLow  = 0;
    info->lastAccessTimeHigh = 0;
    info->lastWriteTimeLow   = 0;
    info->lastWriteTimeHigh  = 0;
#if defined(__APPLE__) && !DEDICATED
    // [CHOSEN, not recovered] The client stands in for BF2.exe's Win32FileSystem, whose FileInfo
    // carries the WIN32_FIND_DATAW file times (PE 0x6767E0): FILETIME, 100 ns since 1601-01-01.
    // RendDX9's effect cache test (EffectManager::compileEffect, PE 0x10034F60) uses a .cfx only
    // when its write time is later than the source's; with every time zero, each effect whose .fx
    // exists was compiled again at every level load and "Optimizing shaders" showed each time.
    auto filetime = [](const struct timespec& t, unsigned int& low, unsigned int& high) {
        const unsigned long long ticks =
            static_cast<unsigned long long>(t.tv_sec + 11644473600LL) * 10000000ull +
            static_cast<unsigned long long>(t.tv_nsec) / 100u;
        low  = static_cast<unsigned int>(ticks);
        high = static_cast<unsigned int>(ticks >> 32);
    };
    filetime(st->st_birthtimespec, info->creationTimeLow, info->creationTimeHigh);
    filetime(st->st_atimespec, info->lastAccessTimeLow, info->lastAccessTimeHigh);
    filetime(st->st_mtimespec, info->lastWriteTimeLow, info->lastWriteTimeHigh);
#endif
}

#if defined(__APPLE__) && !DEDICATED
// [CHOSEN, not recovered] The client image's directory removal. BF2.exe's Win32FileSystem::deleteFile
// (0x676440) removes a directory recursively through the shell (FO_DELETE); the dedicated ELF's
// unlink cannot. LocalProfileManager::deleteProfile (PE 0x4AAF70) deletes a whole profile directory
// through deleteFile, so the macOS client removes the tree depth-first. Symbolic links are removed,
// never followed.
bool removeDirectoryTree(const std::string& path)
{
    DIR* dir = opendir(path.c_str());
    if (!dir)
        return false;
    bool ok = true;
    while (struct dirent* entry = readdir(dir))
    {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
            continue;
        const std::string child = path + "/" + entry->d_name;
        struct stat st;
        if (lstat(child.c_str(), &st) != 0)
        {
            ok = false;
            continue;
        }
        if (S_ISDIR(st.st_mode))
            ok = removeDirectoryTree(child) && ok;
        else if (unlink(child.c_str()) != 0)
            ok = false;
    }
    closedir(dir);
    return rmdir(path.c_str()) == 0 && ok;
}
#endif

} // anonymous namespace

// Construction and destruction


UnixFileSystem::UnixFileSystem()
    : m_refCount(1)
    , m_openStreamCount(0)   // +0x08 - the open-stream count, see the header
{
    // The four std::string members default-construct onto the shared empty representation; that is
    // the four `lock add [_S_empty_rep_storage], 1` in the binary and needs no code here.
}

// 0x84A6920 (D1) / 0x84A69D0 (D0) / 0x84A6A80 (D2), 175 B each. Destroys the four strings in
// reverse declaration order and nothing else.
UnixFileSystem::~UnixFileSystem()
{
}

// IBase quartet


void UnixFileSystem::addRef()
{
    ++m_refCount;
}


int UnixFileSystem::getRef()
{
    return m_refCount;
}


// Releases one reference, deleting the filesystem at zero. Returns the remaining count.
int UnixFileSystem::release()
{
    int remaining = --m_refCount;

    if (remaining == 0 && this != 0)
        delete this;

    return remaining;
}

// Two accepted ids.
void* UnixFileSystem::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IFileSystem)
        return const_cast<UnixFileSystem*>(this);

    return 0;
}

// The class-factory entry point — the canonical shape for all 188 registry classes.
IBase* UnixFileSystem::create(unsigned int iid, IBase* outer)
{
    if (outer)                                  // aggregation is not supported anywhere in BF2
        return 0;

    UnixFileSystem* p = new UnixFileSystem;      // operator new(0x1C)

    IBase* result = (IBase*)p->queryInterface(iid);
    if (!result)
        p->release();

    return result;
}

// Normalizes a path in place by folding case and converting backslashes to forward slashes.
void UnixFileSystem::fixUnixPath(std::string& path)
{
    std::string::size_type first = 0;
    bool keepCase = false;
#if defined(__APPLE__) && !DEDICATED
    // [CHOSEN, not recovered] The ELF lowercases whole paths, which are relative game paths there.
    // The macOS client's @HOME@ substitution is an absolute user path (clientDataDirectory):
    // lowercasing it named a different path on case-sensitive volumes and under the runner's
    // sandbox, so a fresh home's Profile.con/Global.con writes silently failed. Keep that prefix
    // as it is; everything after it is lowered as before.
    const std::string& home = macos::clientDataDirectory();
    if (!home.empty() && path.compare(0, home.size(), home) == 0)
    {
        first = home.size();
        // [CHOSEN, not recovered] Paths below <clientDataDirectory>/Profiles/ keep their case, as
        // the retail Windows client's file system does. BF2.exe validateProfileFiles (PE 0x4A8596)
        // accepts the enumerated "Default" directory by case-sensitive equality; a lowered
        // "default" was rejected, so every start recreated it and its save wrote an empty
        // GlobalSettings.defaultUser over Global.con, and the login list never preselected the
        // saved profile (no PREFIX field, no SELECT ACCOUNT). Content paths stay lowered.
        // docs/macos-profile-prefix.md
        static const char kProfiles[] = "/profiles/";
        const std::string::size_type n = sizeof(kProfiles) - 1;
        if (path.size() >= first + n)
        {
            keepCase = true;
            for (std::string::size_type i = 0; i < n && keepCase; ++i)
            {
                const char c = path[first + i] == '\\' ? '/' : (char)tolower((unsigned char)path[first + i]);
                keepCase = c == kProfiles[i];
            }
        }
    }
#endif
    for (std::string::size_type i = first; i < path.size(); ++i)
    {
        if (path[i] == '\\')
            path[i] = '/';
        else if (!keepCase)
            path[i] = (char)tolower((unsigned char)path[i]);
    }
}

// Opening

// Opens a Unix file stream. The fourth flags argument is unused; path handling belongs to the stream.
IStream* UnixFileSystem::open(const std::string& name, FileMode mode, FileAccess access, unsigned int)
{
    UnixFileStream* stream = new UnixFileStream(*this);     // operator new(0x1C)
    if (!stream)
        return 0;

    if (!stream->open(name, mode, access))
    {
        stream->release();
        return 0;
    }
    return (IStream*)stream;
}

// Queries


// the return is `stat(...) != -1`, not `== 0`. Identical for a conforming stat, but it is what is
// written, and logError fires on any non-zero result.
bool UnixFileSystem::fileExists(const std::string& name)
{
    std::string path(name);
    fixUnixPath(path);

    struct stat st;
    int r = stat(path.c_str(), &st);
    if (r)
        logError("stat", path);

    return r != -1;
}


// Reads file information through a normalized path while retaining the caller's original name in out.
bool UnixFileSystem::getFileInfo(const std::string& name, FileInfo& out)
{
    std::string path(name);
    fixUnixPath(path);

    struct stat st;
    if (stat(path.c_str(), &st) == -1)
    {
        logError("stat", path);
        return false;
    }

    out.name = name;                    // original, not `path`
    buildFileInfo(&out, &st);
    return true;
}

// A stub — unconditionally 1, neither argument is touched.
bool UnixFileSystem::compareFileTime(const std::string&, const std::string&)
{
    return true;
}


// in-place, and returns bool — IFileSystem.h had it as `std::string getFullFileName(...) const`.
// The 4096-byte buffer is the binary's own stack allocation for realpath.
bool UnixFileSystem::getFullFileName(std::string& path)
{
    char resolved[4096];

    if (!realpath(path.c_str(), resolved))
    {
        logError("realpath", path);
        return false;
    }

    path.assign(resolved, strlen(resolved));
    return true;
}

// Mutation


bool UnixFileSystem::deleteFile(const std::string& name)
{
    std::string path(name);
    fixUnixPath(path);

#if defined(__APPLE__) && !DEDICATED
    struct stat st;
    if (lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    {
        if (!removeDirectoryTree(path))
        {
            logError("rmdir", path);
            return false;
        }
        return true;
    }
#endif
    if (unlink(path.c_str()) == -1)
    {
        logError("unlink", path);
        return false;
    }
    return true;
}

// The error detail is built as `from + " -> " + to`.
bool UnixFileSystem::moveFile(const std::string& from, const std::string& to)
{
    std::string f(from), t(to);
    fixUnixPath(f);
    fixUnixPath(t);

    if (rename(f.c_str(), t.c_str()) == -1)
    {
        logError("rename", f + " -> " + t);
        return false;
    }
    return true;
}


// it hard-links. `link()`, not a byte copy — see UnixFileSystem.h. And the flag is
// fail-if-exists, not overwrite.
bool UnixFileSystem::copyFile(const std::string& from, const std::string& to, bool failIfExists)
{
    std::string f(from), t(to);
    fixUnixPath(f);
    fixUnixPath(t);

    if (failIfExists && fileExists(t))
        return false;

    if (link(f.c_str(), t.c_str()) == -1)
    {
        logError("link", f + " -> " + t);
        return false;
    }
    return true;
}

// Whole-file read, capped at the caller's buffer size.
bool UnixFileSystem::readFile(const std::string& name, void* buffer, unsigned int size,
                              unsigned int& bytesRead)
{
    std::string path(name);
    fixUnixPath(path);

    UnixFileStream stream(*this);                       // on the stack, not heap
    if (!stream.open(path, (FileMode)2, (FileAccess)0)) // mode 2 == read; confirms FILEMODE_READ
    {
        bytesRead = 0;
        return false;
    }

    unsigned int n = stream.getSize();
    if (n > size)
        n = size;

    bytesRead = stream.read(buffer, n);
    return true;
}


// Creates parent directories and writes the buffer, appending when requested.
bool UnixFileSystem::writeFile(const std::string& name, const void* buffer, unsigned int size,
                               bool append)
{
    std::string path(name);
    fixUnixPath(path);

    createDirectory(extractFilePath(path, '/'));

    UnixFileStream stream(*this);
    if (!stream.open(path, (FileMode)(!append), (FileAccess)2))
        return false;

    stream.write(buffer, size);
    return true;
}

// Creates directories at slash boundaries with mode 0755; existing directories succeed.
// The final component is created only when followed by a slash.
bool UnixFileSystem::createDirectory(const std::string& path)
{
    for (std::string::size_type i = 0; i < path.size(); ++i)
    {
        if (path[i] != '/')
            continue;
#if defined(__APPLE__) && !DEDICATED
        // [CHOSEN, not recovered] The ELF only sees relative paths. The macOS client writes
        // under an absolute @HOME@, whose leading '/' made the first part "" and mkdir("")
        // fail with ENOENT, which aborted the walk: a fresh home never got Profiles/Default/.
        if (i == 0)
            continue;
#endif

        std::string part(path, 0, i);
        if (mkdir(part.c_str(), 0755) == -1 && errno != EEXIST)
        {
            logError("mkdir", part + " (towards " + path + ")");
            return false;
        }
    }
    return true;
}

// Enumeration

// The helper is seeded with the folded path's length so every result comes back
// relative to it.
int UnixFileSystem::findFiles(const std::string& path, bool recursive, std::list<FileInfo>& out)
{
    std::string p(path);
    fixUnixPath(p);

    return findFiles_recursionHelper(p, recursive, out, (unsigned int)p.size());
}

// Appends entries under path and returns the number added, including directories.
// Names remain relative to the original baseLen through recursion; entries that fail stat are skipped.
int UnixFileSystem::findFiles_recursionHelper(const std::string& path, bool recursive,
                                              std::list<FileInfo>& out, unsigned int baseLen)
{
    DIR* dir = opendir(path.c_str());
    if (!dir)
    {
        logError("opendir", path);
        return 0;
    }

    int count = 0;
    FileInfo info;                                  // reused across iterations, as in the binary
    struct dirent entry;
    struct dirent* result = 0;

    while (readdir_r(dir, &entry, &result) == 0 && result)
    {
        if (memcmp(entry.d_name, ".", 2) == 0 || memcmp(entry.d_name, "..", 3) == 0)
            continue;

        const std::string filename = path + "/" + std::string(entry.d_name);

        info.name = filename.substr(baseLen + 1);   // relative to the original root

        struct stat st;
        if (stat(filename.c_str(), &st) == -1)
        {
            logError("stat", filename);
            continue;                               // skipped, and not counted
        }

        buildFileInfo(&info, &st);
        out.push_back(info);
        ++count;

        if (recursive && info.isDirectory)
            count += findFiles_recursionHelper(filename, recursive, out, baseLen);
    }

    if (closedir(dir) == -1)
        logError("closedir", path);

    return count;
}

// Path substitutions — how @home@ and @root@ expand. See FileManager::getWritePath.


// the warning is unconditional — it fires on every call, not on a write attempt, and the function
// then hands back "/tmp" anyway. Line 435 of this file, and the source of its attested path.
const std::string& UnixFileSystem::getHomeSubstitution() const
{
#if defined(__APPLE__) && !DEDICATED
    // [CHOSEN, not recovered] Client data is independent of the read-only game installation.
    return macos::clientDataDirectory();
#else
    Debug(DEBUGTYPE_6, __FILE__, 435, std::string())
        << "dep: dedicated servers aren't allowed to write to @HOME@";

    static const std::string home("/tmp");
    return home;
#endif
}

// @root@ is the current directory.
const std::string& UnixFileSystem::getRootSubstitution() const
{
    static const std::string root(".");
    return root;
}

// empty. Every error path above funnels here and nothing comes out. See the
// header block in UnixFileSystem.h — preserved deliberately.
void UnixFileSystem::logError(const char*, const std::string&)
{
}

// empty (push ebp / mov ebp,esp / pop ebp / ret). The header block lists it among
// the three members that do nothing; the body itself was missing from this file until the Linux
// port's first strict link asked for it.
void UnixFileSystem::update()
{
}

// Default filesystem initialization requires no work.
void IFileSystem::init()
{
}

// 0x848CD30, `return 0;`
bool IFileSystem::isLastFileOpenedFromArchive() const
{
    return false;
}

} // namespace io
} // namespace hfe
} // namespace dice

#endif // POSIX
