// Resolves engine file paths across the base filesystem, archives and path overloads.
// Preserve each operation's search order and error policy. addMergeDirectory is unused;
// getFullFileName retains the original diagnostic-only behavior.

#include "FileManager.h"

#include "ZipArchive.h"             // the concrete archive - mountArchive/unmountArchive cast to it

#include "../../Core/Debug.h"
#include "../../Core/StringUtil.h"
#include "../../Core/Hash.h"
#include "../../Core/LoadStat.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <string>

namespace dice {
namespace hfe {

// Path matching uses both bounded prefix comparisons and whole-string comparisons.

namespace io {

typedef std::vector<IArchive*>::const_iterator ArcIter;

// [UNRESOLVED] value. addPathOverload substitutes this file-scope string when the source path fixes
// down to a bare "/". IDA reports the global only as `filename`; the empty string is the plausible
// reading but is not proven, so it is named rather than guessed inline.
static const char* const kRootOverloadReplacement = "";

// @MOD@ expands anywhere in a relative path; @root@ and @home@ expand only at its start.
// Absolute paths bypass substitution.
namespace {

// macOS client: isHostAbsolutePath (Core/StringUtil) tells host paths from game paths with a
// leading '/' (SwiffPlayer's "$/Levels/<level>/Info/loadMap.png" reaches open() as "/Levels/...").

// `base` is the search-path entry being tried. `out` receives the expanded result.
void expandPath(std::string& path, const std::string& base, std::string* out,
                IFileSystem* fileSys)
{
    static const std::string homePrefix = "@HOME@";     // guard 0x8AC3D50
    static const std::string rootPrefix = "@ROOT@";     // guard 0x8AC3D58
    static const std::string modPrefix  = "@MOD@";      // guard 0x8AC3D60

#if defined(__APPLE__) && !DEDICATED
    if (isHostAbsolutePath(path))
#else
    if (isPathAbsolute(path))
#endif
    {
        out->assign(path);
        return;
    }

    std::string prefix;
    bool modSeen = false;

    // @MOD@ : anywhere in the string
    const std::string::size_type m = path.find(modPrefix);
    if (m != std::string::npos)
    {
        prefix = path.substr(0, m);                 // whatever preceded the macro
        path.erase(0, prefix.size() + 1);           // note the +1 in the ELF
        path.erase(0, modPrefix.size());
        *out = prefix + base + path;                // the macro's slot takes the search path
        modSeen = true;
        path.assign(*out);
    }

    // @home@ : position 0 only
    if (path.find(homePrefix) == 0)
    {
        prefix = fileSys->getHomeSubstitution() + "/";
        path.erase(0, homePrefix.size() + 1);
    }

    // @root@ : position 0 only
    if (path.find(rootPrefix) == 0)
    {
        prefix = fileSys->getRootSubstitution() + "/";
        path.erase(0, rootPrefix.size() + 1);
    }

    // If @MOD@ already consumed the base, do not apply it a second time.
    if (modSeen) *out = prefix + path;
    else         *out = prefix + base + path;
}

// Folds an expanded loose-file path before an existence or enumeration test.
#if defined(__APPLE__) && !DEDICATED
// [CHOSEN, not recovered] c-loopback: the macOS client's @HOME@ substitution is absolute
// (Platform/macOS/ClientPaths), and goUpInPath turns "/Users/..." into the cwd-relative
// "Users/...". fileExistsEx/getFileExists/findFiles then never see @HOME@ files (no local
// profile was ever found), while open(), which skips the fold, reads them. Keep the leading '/'.
// The Windows home is a drive path and the ELF dedicated home is never read, so both keep the
// shipped fold unchanged.
void goUpInExpandedPath(std::string& full)
{
    const bool absolute = !full.empty() && full[0] == '/';
    goUpInPath(full);
    if (absolute && (full.empty() || full[0] != '/'))
        full.insert(full.begin(), '/');
}
#else
inline void goUpInExpandedPath(std::string& full) { goUpInPath(full); }
#endif

} // anonymous namespace

// Reports and removes the legacy bf2/content/ path segment, matched case-insensitively.
void removeBf2ContentPart(std::string& path)
{
    const std::string::size_type p = getLowerCase(path).find("bf2/content/");
    if (p == std::string::npos)
        return;

    Debug(DEBUGTYPE_6, "IO/File/FileManager.cpp", 70, __FUNCTION__)
        << "should not use paths including bf2/content anymore, please fix file:" << path;

    path.erase(p, 12);                                     // strlen("bf2/content/")
}


// Resolves where a write should land. Four cases, tried in order. Note this never consults the
// archives or the rest of the path list: writes only ever go to the first path-list entry.
void FileManager::getWritePath(const std::string& name, std::string& out)
{
    std::string path = name;
    fixPathDelimiters(path, '/');                   // backslashes normalised to '/'

    const std::string& writePath = m_pathList.front();

    // 1. already rooted at the write path, or absolute -> verbatim, and no goUpInPath
    if ((!writePath.empty() && compareIgnoreCase(writePath.c_str(), path.c_str(),
                                                 static_cast<int>(writePath.size())) == 0)
        || isPathAbsolute(path))
    {
        out.assign(path);
        return;
    }

    // 2. @root@ prefix
    if (path.find("@ROOT@") == 0)
        out = m_fileSystem->getRootSubstitution() + path.substr(6);
    // 3. @home@ prefix
#if defined(__APPLE__) && !DEDICATED
    // [CHOSEN, not recovered] The macOS client's @HOME@ is absolute; goUpInPath below drops a
    // leading '/' (harmless for Windows drive paths). Normalize only the part after the macro.
    else if (path.find("@HOME@") == 0)
    {
        std::string tail = path.substr(6);
        goUpInPath(tail);
        out = m_fileSystem->getHomeSubstitution() + "/" + tail;
        return;
    }
#else
    else if (path.find("@HOME@") == 0)
        out = m_fileSystem->getHomeSubstitution() + path.substr(6);
#endif
    // 4. otherwise relative to the write path
    else
        out = writePath + path;

    goUpInPath(out);
}

// Removes the mod prefix from paths containing mods/ and a later shaders segment. Other paths are unchanged.
std::string FileManager::stripModPath(const std::string& path)
{
    std::string out = path;

    if (getLowerCase(path).find("mods/") == std::string::npos)
        return out;

    const std::string::size_type s = getLowerCase(path).find("shaders");
    if (s == 0 || s == std::string::npos)           // position 0 is explicitly excluded
        return out;

    out.erase(0, s > out.size() ? out.size() : s);
    return out;
}

// Returns the mounted archive list matching the directory prefix, or null.
// The returned list belongs to the mount table.
std::vector<IArchive*>* FileManager::findArchives(const std::string& dir)
{
    // An empty archive map or directory has no mounted archive list.
    if (m_archives.empty() || dir.empty())
        return 0;

    const std::string name = stripModPath(dir);

    typedef std::map<std::string, std::vector<IArchive*>, NoCaseStringCompare> ArchiveMap;

    std::vector<IArchive*>* found = 0;
    for (ArchiveMap::iterator it = m_archives.begin(); it != m_archives.end(); ++it)
    {
        // Compare only the key-length prefix; keep the helper's signed count type.
        if (compareIgnoreCase(name.c_str(), it->first.c_str(), static_cast<int>(it->first.size())) == 0)
        {
            found = &it->second;
            break;
        }
    }

    // An entry that exists but is empty counts as "no archives".
    if (found && found->empty())
        return 0;

    return found;
}

// Starts with mounting disabled and no base filesystem, and installs an initial search path.
// [UNRESOLVED] The original initial-path string.
FileManager::FileManager()
    : m_refCount(1)
    , m_fileSystem(0)
    // m_archiveHandlers default-constructs empty - that is the three zeroed words the binary writes.
    , m_fileResolveMode(0)
    , m_isLastFileOpenedFromArchive(false)
    , m_permitMountArchives(false)
{
    // The four containers are members, not pointers, so they are already constructed here -
    // which is what the binaries do. See the layout census in FileManager.h.
    addPath(kRootOverloadReplacement);
}

// Unmounts archives, releases handlers, then releases the base filesystem and owned containers.
FileManager::~FileManager()
{
    unmountAllArchives();
    for (std::vector<IArchiveHandler*>::iterator it = m_archiveHandlers.begin();
         it != m_archiveHandlers.end(); ++it)
        (*it)->release();
    // The four containers destroy themselves; only the filesystem reference is ours to drop.
    // Ordering preserved: the base filesystem is released after the handlers, as shipped.
    if (m_fileSystem)
        m_fileSystem->release();
}

// ELF 0x8486B70 / PE 0x64C490: remove the first match and always rebuild extensions.
void FileManager::removeArchiveHandler(IArchiveHandler* handler)
{
    for (std::vector<IArchiveHandler*>::iterator it = m_archiveHandlers.begin();
         it != m_archiveHandlers.end(); ++it)
    {
        if (*it == handler)
        {
            (*it)->release();
            m_archiveHandlers.erase(it);
            break;
        }
    }
    updateArchiveExts();
}

// ELF 0x84884F0 / PE 0x64DE10. The PE explicitly materializes al, proving bool.
bool FileManager::removeMergeDirectory(const std::string& path)
{
    for (std::list<MergeDirectory>::iterator it = m_mergeDirectories.begin();
         it != m_mergeDirectories.end(); ++it)
    {
        if (equalsIgnoreCase(it->path, path))
        {
            m_mergeDirectories.erase(it);
            return true;
        }
    }
    return false;
}

// Resolves a path without mod stripping, archive skipping or resolve-mode filtering.
// Queries the current directory before advancing loading progress.
std::string FileManager::fileExistsEx2(const std::string& name)
{
    std::string path = name;
    removeBf2ContentPart(path);
    if (path.empty())
        return getEmptyString();
#if defined(_WIN32)
    char currentDirectory[260];
    GetCurrentDirectoryA(260, currentDirectory);
#else
    char currentDirectory[4097];
    getcwd(currentDirectory, 4097);
#endif
    loadStat->incExistsCounter();
    std::string stripped = path;
    goUpInPath(stripped);
    const std::string directory = getFixPath(extractFilePath(stripped, '/'), '/');
    std::vector<IArchive*>* archives = findArchives(directory);
    if (archives)
    {
        for (ArcIter it = archives->begin(); it != archives->end(); ++it)
            if ((*it)->fileExists(stripped))
                return stripped;
    }
    for (std::list<std::string>::const_iterator it = m_pathList.begin();
         it != m_pathList.end(); ++it)
    {
        std::string full;
        std::string copy = path;
        expandPath(copy, *it, &full, m_fileSystem);
        goUpInExpandedPath(full);
        if (m_fileSystem->fileExists(full))
            return full;
    }
    return getEmptyString();
}

// Creates the requested interface with one owned reference. Rejects aggregation and unsupported interfaces with null.
IBase* FileManager::create(unsigned int iid, IBase* outer)
{
    if (outer)                                  // aggregation is not supported anywhere in BF2
        return 0;

    FileManager* p = new FileManager;           // operator new(0x44) + C1 ctor

    IBase* result = (IBase*)p->queryInterface(iid);
    if (!result)
        p->release();

    return result;
}

// Tries archive handlers selected by extension and returns the opened archive.
// Selection uses a case-insensitive character overlap, not extension equality; handlers may reject mismatches.
IArchive* FileManager::openArchive(const std::string& file, int /*unused*/, bool flag)
{
    const std::string ext = getUpperCase(extractFileExt(file));

    for (std::vector<IArchiveHandler*>::const_iterator h = m_archiveHandlers.begin(); h != m_archiveHandlers.end(); ++h)
    {
        const std::string exts = getUpperCase((*h)->getExtensions());

        if (exts.find_first_of(ext) != std::string::npos)
        {
            IArchive* a = (*h)->open(file, m_fileSystem, flag);
            if (a)
                return a;
        }
    }
    return 0;
}

// Removes one matching from/to overload pair. Returns false when no pair matches.
bool FileManager::removePathOverload(const std::string& from, const std::string& to)
{
    typedef std::multimap<std::string, std::string, NoCaseStringCompare> OverloadMap;

    std::string f = getFixPath(from, '/');
    if (f.compare("/") == 0)
        f.assign(kRootOverloadReplacement);

    const std::string t = getFixPath(to, '/');

    for (OverloadMap::iterator it = m_pathOverloads.begin(); it != m_pathOverloads.end(); ++it)
    {
        if (equalsIgnoreCase(it->first, f) && equalsIgnoreCase(it->second, t))
        {
            m_pathOverloads.erase(it);
            return true;
        }
    }
    return false;
}

// Prepends a search path, making it the first lookup location and the write path.
void FileManager::addPath(const std::string& path)
{
    m_pathList.insert(m_pathList.begin(), getFixPath(path, '/'));
}

// Adds a path overload. Multiple targets for the same source are retained.
bool FileManager::addPathOverload(const std::string& from, const std::string& to)
{
    std::string f = getFixPath(from, '/');

    // A bare "/" is replaced by a file-scope string constant. [UNRESOLVED] its value - IDA names
    // the global `filename`; the empty string is the plausible reading but it is not proven.
    if (f.compare("/") == 0)
        f.assign(kRootOverloadReplacement);

    m_pathOverloads.insert(std::make_pair(f, getFixPath(to, '/')));
    return true;
}

// Reports that merge directories are unused; does not register a directory.
bool FileManager::addMergeDirectory(const std::string&, const std::string&)
{
    Debug(DEBUGTYPE_6, "IO/File/FileManager.cpp", 873, __FUNCTION__) << "Not used";
    return true;
}

// Mounts an archive at mountPath when mounting is enabled and the file resolves and opens.
// Rejects an archive already mounted anywhere. Returns the mounted archive or null.
IArchive* FileManager::mountArchive(const std::string& archiveName,
                                    const std::string& mountPath,
                                    bool contentCheck)
{
    typedef std::map<std::string, std::vector<IArchive*>, NoCaseStringCompare> ArchiveMap;

    // 1. runtime mounting must be permitted
    if (!m_permitMountArchives)
    {
        Debug(DEBUGTYPE_1, "IO/File/FileManager.cpp", 818, __FUNCTION__)
            << "not supported in runtime";
        return 0;
    }

    // 2. resolve the archive file itself - skipArchives = true
    const std::string resolved = fileExistsEx(archiveName, true);
    if (resolved.empty())
        return 0;

    // 3. reject a double mount, comparing resolved filenames across every mount point
    for (ArchiveMap::iterator it = m_archives.begin(); it != m_archives.end(); ++it)
    {
        for (ArcIter a = it->second.begin(); a != it->second.end(); ++a)
        {
            if (compareIgnoreCase(static_cast<ZipArchive*>(*a)->getFilename().c_str(),
                                  resolved.c_str()) == 0)
                return 0;
        }
    }

    // 4. try each search path in turn until one yields an archive
    const std::string lower = getLowerCase(archiveName);
    IArchive* arc = 0;
    for (std::list<std::string>::const_iterator it = m_pathList.begin();
         it != m_pathList.end(); ++it)
    {
        arc = openArchive(*it + lower, 0, true);
        if (arc)
            break;
    }
    if (!arc)
        return 0;

    // Configure and index
    static_cast<ZipArchive*>(arc)->setContentCheck(contentCheck);

    const std::string key = getFixPath(getLowerCase(mountPath), '/');
    static_cast<ZipArchive*>(arc)->setPath(key);

    ArchiveMap::iterator slot = m_archives.find(key);
    if (slot == m_archives.end())
        m_archives[key] = std::vector<IArchive*>(1, arc);
    else
        slot->second.push_back(arc);

    return arc;
}

// Opens a virtual file. Reads search archives before loose files; writes go directly to the current write path.
IStream* FileManager::open(const std::string& name, IFileSystem::FileMode mode,
                         IFileSystem::FileAccess access, unsigned int a6)
{
    // Write: straight through
    if (mode != IFileSystem::FILEMODE_READ || access == 2 || access == 1)
    {
        std::string wp;
        getWritePath(name, wp);
        loadStat->incLoadCounter(1);
        return m_fileSystem->open(wp, mode, access, a6);
    }

    // Read: layered
    m_isLastFileOpenedFromArchive = false;

    std::string path;
    // expandPath takes its path by non-const reference; the temporaries the previous spelling bound
    // to it are an MSVC extension. Named here, with the same lifetime and the same discarded result.
    std::string strippedName = stripModPath(name);
    const std::string emptyBase;
    expandPath(/*path*/ strippedName, /*base*/ emptyBase, &path, m_fileSystem);
    goUpInPath(path);
    removeBf2ContentPart(path);

    const std::string dir = getFixPath(extractFilePath(path, '/'), '/');

    // 1. archives covering that directory, in order.
    //    corrected: these are IArchive*, and IArchive derives from IFileSystem - so this is the
    //    same open() slot (+0x18) the real disk uses two loops below, not an archive-specific API.
    std::vector<IArchive*>* arcs = findArchives(dir);
    if (arcs)
    {
        for (ArcIter a = arcs->begin(); a != arcs->end(); ++a)
        {
            IStream* f = (*a)->open(path, IFileSystem::FILEMODE_READ, access, a6);
            if (f)
            {
                loadStat->setCurrentFile(path);
                loadStat->incLoadCounter(1);
                m_isLastFileOpenedFromArchive = true;      // set only on this branch
                return f;
            }
        }
    }

    // Resolve mode 1 permits loose-file fallback only for names containing @home@, @root@ or png.
    // [UNRESOLVED] The enumerator name for this mode.
    if (m_fileResolveMode == 1)
    {
        if (name.find("@HOME@") == std::string::npos &&
            name.find("@ROOT@") == std::string::npos &&
            name.find("png")    == std::string::npos)
            return 0;
    }

    // 3. loose files, one attempt per search-path entry
    for (std::list<std::string>::const_iterator it = m_pathList.begin(); it != m_pathList.end(); ++it)
    {
        std::string full;
        std::string p = name;
        expandPath(p, *it, &full, m_fileSystem);

        m_isLastFileOpenedFromArchive = false;
        IStream* f = m_fileSystem->open(full, IFileSystem::FILEMODE_READ, access, a6);
        if (f)
        {
            loadStat->setCurrentFile(path);
            loadStat->incLoadCounter(1);
            return f;
        }
    }

    return 0;
}

// Returns the resolved file path, or an empty string if absent. skipArchives restricts lookup to loose files.
std::string FileManager::fileExistsEx(const std::string& name, bool skipArchives)
{
    std::string path = name;
    removeBf2ContentPart(path);

    if (path.empty())
        return getEmptyString();

    loadStat->incExistsCounter();                    // dword 11

    std::string stripped = stripModPath(path);
    goUpInPath(stripped);
    const std::string dir = getFixPath(extractFilePath(stripped, '/'), '/');

    // 1. archives, unless explicitly skipped.
    //    corrected: IFileSystem::fileExists (+0x1C), the same slot the disk filesystem answers.
    if (!skipArchives)
    {
        std::vector<IArchive*>* arcs = findArchives(dir);
        if (arcs)
        {
            for (ArcIter a = arcs->begin(); a != arcs->end(); ++a)
            {
                if ((*a)->fileExists(stripped))
                    return stripped;
            }
        }
    }

    // 2. loose files along the path list. note goUpInPath is applied to the expanded path here,
    //    which open() does not do - open expands then opens directly.
    for (std::list<std::string>::const_iterator it = m_pathList.begin(); it != m_pathList.end(); ++it)
    {
        std::string full;
        std::string p = path;
        expandPath(p, *it, &full, m_fileSystem);
        goUpInExpandedPath(full);

        if (m_fileSystem->fileExists(full))
            return full;
    }

    return getEmptyString();
}


// A one-liner over fileExistsEx, and the clearest confirmation that fileExistsEx returns a path
// rather than a bool: the whole body is an emptiness test on its result.
bool FileManager::fileExists(const std::string& name)
{
    return !fileExistsEx(name, false).empty();
}

// Reads from archives before loose search paths.
// Loose-file fallback concatenates path and name without expanding path macros again.
bool FileManager::readFile(const std::string& name, void* buffer, unsigned int size,
                           unsigned int& bytesRead)
{
    std::string path = name;
    fixPathDelimiters(path, '/');

    std::string expanded;
    expandPath(path, kRootOverloadReplacement, &expanded, m_fileSystem);
    path = expanded;
    goUpInPath(path);

    const std::string dir = getFixPath(extractFilePath(path, '/'), '/');

    // 1. archives - IFileSystem::readFile (+0x2C)
    std::vector<IArchive*>* arcs = findArchives(dir);
    if (arcs)
    {
        for (ArcIter a = arcs->begin(); a != arcs->end(); ++a)
        {
            if ((*a)->readFile(path, buffer, size, bytesRead))
            {
                loadStat->incLoadCounter(1);
                return true;
            }
        }
    }

    // 2. loose files - note the plain concatenation, no expandPath
    for (std::list<std::string>::const_iterator it = m_pathList.begin();
         it != m_pathList.end(); ++it)
    {
        if (m_fileSystem->readFile(*it + path, buffer, size, bytesRead))
        {
            loadStat->incLoadCounter(1);
            return true;
        }
    }
    return false;
}

// Replaces the base filesystem, releasing the old one before initializing and retaining fs.
void FileManager::setBaseSystem(IFileSystem* fs)
{
    if (m_fileSystem)
    {
        m_fileSystem->release();
        m_fileSystem = 0;
    }

    m_fileSystem = fs;
    if (fs)
    {
        fs->init();
        m_fileSystem->addRef();
    }
}

// Appends archive lists mounted above or below pattern. Returned list pointers alias the mount table.
void FileManager::findAllMatchingArchives(const std::string& pattern,
                                          std::vector<std::vector<IArchive*>*>& out) const
{
    if (pattern.empty())
        return;

    // Computed and never used - see above. Kept for fidelity.
    std::string up = pattern;
    goUpInPath(up);

    typedef std::map<std::string, std::vector<IArchive*>, NoCaseStringCompare> ArchiveMap;

    // Returned pointers alias mutable archive lists even though this accessor is const.
    ArchiveMap& archives = const_cast<ArchiveMap&>(m_archives);

    // Pass 1: mount point is an ancestor (compare over the KEY's length)
    for (ArchiveMap::iterator it = archives.begin(); it != archives.end(); ++it)
    {
        if (compareIgnoreCase(it->first, pattern, static_cast<int>(it->first.size())) == 0)
            out.push_back(&it->second);
    }

    // Pass 2: mount point is a descendant (compare over the pattern's length)
    for (ArchiveMap::iterator it = archives.begin(); it != archives.end(); ++it)
    {
        if (pattern.size() == it->first.size())
            continue;                                   // pass 1 already settled these

        if (compareIgnoreCase(it->first, pattern, static_cast<int>(pattern.size())) == 0)
            out.push_back(&it->second);
    }
}

// Retains the handler, adds it to the list and rebuilds the advertised extension string.
void FileManager::addArchiveHandler(IArchiveHandler* h)
{
    m_archiveHandlers.push_back(h);      // both branches of the inlined push_back at 0x8486B10

    h->addRef();
    updateArchiveExts();
}

// Rebuilds the space-separated uppercase extension list from all registered handlers.
void FileManager::updateArchiveExts()
{
    m_archiveExtensions.clear();

    for (std::vector<IArchiveHandler*>::const_iterator h = m_archiveHandlers.begin(); h != m_archiveHandlers.end(); ++h)
        m_archiveExtensions.append((*h)->getExtensions() + " ");

    setUpperCase(m_archiveExtensions);
}

// Removes a search path using case-sensitive comparison after path normalization.
void FileManager::removePath(const std::string& path)
{
    const std::string fixed = getFixPath(path, '/');

    for (std::list<std::string>::iterator it = m_pathList.begin();
         it != m_pathList.end(); ++it)
    {
        if (*it == fixed)                       // memcmp - case sensitive
        {
            m_pathList.erase(it);
            return;
        }
    }
}

// Destroys an archive through its virtual destructor. Exact unmounts use this path; wildcard unmounts release a reference.
void FileManager::destroyArchive(IArchive* archive)
{
    delete archive;
}

bool FileManager::unmountArchive(const std::string& name)
{
    typedef std::map<std::string, std::vector<IArchive*>, NoCaseStringCompare> ArchiveMap;

    // Wildcard
    if (name[0] == '*')
    {
        const std::string needle = getLowerCase(name.substr(1));

        for (ArchiveMap::iterator it = m_archives.begin(); it != m_archives.end(); ++it)
        {
            for (std::vector<IArchive*>::iterator a = it->second.begin(); a != it->second.end(); )
            {
                const std::string n = getLowerCase(static_cast<ZipArchive*>(*a)->getFilename());
                if (n.find(needle) != std::string::npos)
                {
                    (*a)->release();                    // +0x08
                    a = it->second.erase(a);
                }
                else
                {
                    ++a;
                }
            }
        }
        return true;                                     // unconditional
    }

    // Exact
    for (std::list<std::string>::const_iterator p = m_pathList.begin();
         p != m_pathList.end(); ++p)
    {
        for (ArchiveMap::iterator it = m_archives.begin(); it != m_archives.end(); ++it)
        {
            for (std::vector<IArchive*>::iterator a = it->second.begin();
                 a != it->second.end(); ++a)
            {
                if (compareIgnoreCase(static_cast<ZipArchive*>(*a)->getFilename().c_str(),
                                      (*p + name).c_str()) == 0)
                {
                    destroyArchive(*a);                  // +0x54, the deleting destructor
                    it->second.erase(a);
                    return true;
                }
            }
        }
    }
    return false;
}

// Destroys all mounted archives and clears the mount table.
void FileManager::unmountAllArchives()
{
    typedef std::map<std::string, std::vector<IArchive*>, NoCaseStringCompare> ArchiveMap;

    for (ArchiveMap::iterator it = m_archives.begin(); it != m_archives.end(); ++it)
    {
        for (std::vector<IArchive*>::iterator a = it->second.begin(); a != it->second.end(); ++a)
        {
            if (*a)
                destroyArchive(*a);                      // +0x54
        }
        it->second.clear();
    }

    if (!m_archives.empty())
        m_archives.clear();        // clearing is the count reset the old flag model imagined
}

// Appends all mounted archives to out in mount-path order, then insertion order within each mount.
void FileManager::getAllArchives(std::vector<IArchive*>& out)
{
    typedef std::map<std::string, std::vector<IArchive*>, NoCaseStringCompare> ArchiveMap;

    for (ArchiveMap::const_iterator it = m_archives.begin(); it != m_archives.end(); ++it)
        for (ArcIter a = it->second.begin(); a != it->second.end(); ++a)
            out.push_back(*a);
}

// Returns whether a is at least as new as b by last-write time. Missing file information returns false.
bool FileManager::compareFileTime(const std::string& a, const std::string& b)
{
    FileInfo ia, ib;

    if (!getFileInfo(a, ia) || !getFileInfo(b, ib))
        return false;

    if (ia.lastWriteTimeHigh != ib.lastWriteTimeHigh)
        return ia.lastWriteTimeHigh > ib.lastWriteTimeHigh;
    return ia.lastWriteTimeLow >= ib.lastWriteTimeLow;
}

// Reports the unsupported operation and returns false without resolving path.
bool FileManager::getFullFileName(std::string& /*path*/)
{
    Debug(DEBUGTYPE_7, "IO/File/FileManager.cpp", 723, __FUNCTION__)
        << "getFullFileName not impl";
    return false;              // the binary returns 0 after emitting the diagnostic
}

// Resolves paths through the write path and delegates to the base filesystem. Archives are not modified.
bool FileManager::deleteFile(const std::string& name)
{
    std::string wp;
    getWritePath(name, wp);
    return m_fileSystem->deleteFile(wp);
}

bool FileManager::moveFile(const std::string& from, const std::string& to)
{
    std::string wpFrom, wpTo;
    getWritePath(from, wpFrom);
    getWritePath(to,   wpTo);
    return m_fileSystem->moveFile(wpFrom, wpTo);
}

bool FileManager::copyFile(const std::string& from, const std::string& to, bool overwrite)
{
    std::string wpFrom, wpTo;
    getWritePath(from, wpFrom);
    getWritePath(to,   wpTo);
    return m_fileSystem->copyFile(wpFrom, wpTo, overwrite);
}

bool FileManager::writeFile(const std::string& name, const void* buffer, unsigned int size,
                            bool append)
{
    std::string wp;
    getWritePath(name, wp);
    loadStat->incLoadCounter(1);
    return m_fileSystem->writeFile(wp, buffer, size, append);
}

// Fills out by searching mounted archives before loose files. Returns whether a file was found.
bool FileManager::getFileInfo(const std::string& name, FileInfo& out)
{
    // Archive lookup and the returned record use the mod-stripped name; loose files use the original path.
    const std::string stripped = stripModPath(name);

    std::string dir = getFixPath(extractFilePath(stripped, '/'), '/');
    goUpInPath(dir);
    removeBf2ContentPart(dir);

    loadStat->setCurrentFile(name);

    // 1. archives - IFileSystem::getFileInfo (+0x38)
    std::vector<IArchive*>* arcs = findArchives(dir);
    if (arcs)
    {
        for (ArcIter a = arcs->begin(); a != arcs->end(); ++a)
        {
            if ((*a)->getFileInfo(stripped, out))
            {
                out.name = stripped;             // the archive branch fills the name itself
                return true;
            }
        }
    }

    // 2. loose files
    for (std::list<std::string>::const_iterator it = m_pathList.begin();
         it != m_pathList.end(); ++it)
    {
        std::string full;
        std::string p = name;
        expandPath(p, *it, &full, m_fileSystem);

        if (m_fileSystem->getFileInfo(full, out))
            return true;
    }
    return false;
}

// Opens a loose file through the search paths, bypassing archives.
IStream* FileManager::openwin32(const std::string& name, IFileSystem::FileMode mode,
                              IFileSystem::FileAccess access, unsigned int a6)
{
    if (mode != IFileSystem::FILEMODE_READ || access == 2 || access == 1)
    {
        std::string wp;
        getWritePath(name, wp);
        loadStat->incLoadCounter(1);
        return m_fileSystem->open(wp, mode, access, a6);
    }

    for (std::list<std::string>::const_iterator it = m_pathList.begin();
         it != m_pathList.end(); ++it)
    {
        std::string full;
        std::string p = name;
        expandPath(p, *it, &full, m_fileSystem);

        m_isLastFileOpenedFromArchive = false;
        IStream* f = m_fileSystem->open(full, IFileSystem::FILEMODE_READ, access, a6);
        if (f)
        {
            loadStat->setCurrentFile(name);
            loadStat->incLoadCounter(1);
            return f;
        }
    }
    return 0;
}

// Replaces out with case-insensitively deduplicated matches and returns their count.
// Loose files are searched only when no archive matched the path and no result exists.
int FileManager::findFiles(const std::string& pattern, bool recursive,
                           std::list<FileInfo>& out)
{
    if (pattern.empty())
        return 0;

    out.clear();

    std::string path = pattern;
    fixPath(path, '/');
    loadStat->incExistsCounter();

    // Which archives could serve this pattern at all.
    // A vector of pointers to the map's archive vectors, not a flat list - see the signature note
    // in FileManager.h. Each entry aliases one mount point's vector inside m_archives, so the loop
    // below is nested: outer over mount points, inner over that mount point's archives.
    std::vector<std::vector<IArchive*>*> matching;
    findAllMatchingArchives(pattern, matching);

    // Case-insensitive de-duplication set, 3011 buckets, discarded at the end.
    Hash<std::string, bool, StringHashNoCaseHashFunc> seen(3011);

    const std::string stripped = stripModPath(path);
    std::list<FileInfo> scratch;

    // 1. every matching archive, IFileSystem::findFiles (+0x34)
    for (std::vector<std::vector<IArchive*>*>::const_iterator m = matching.begin();
         m != matching.end(); ++m)
    for (std::vector<IArchive*>::const_iterator a = (*m)->begin(); a != (*m)->end(); ++a)
    {
        (*a)->findFiles(stripped, recursive, scratch);

        for (std::list<FileInfo>::const_iterator f = scratch.begin(); f != scratch.end(); ++f)
        {
            if (!seen.exists(f->name))
            {
                seen.insert(f->name, true);
                out.push_back(*f);
            }
        }
        scratch.clear();
    }

    // 2. loose files - only if nothing was found and no archive matched
    if (out.empty() && matching.empty())
    {
        for (std::list<std::string>::const_iterator it = m_pathList.begin();
             it != m_pathList.end(); ++it)
        {
            std::string full;
            std::string p = path;
            expandPath(p, *it, &full, m_fileSystem);
            goUpInExpandedPath(full);

            m_fileSystem->findFiles(full, recursive, scratch);

            for (std::list<FileInfo>::const_iterator f = scratch.begin(); f != scratch.end(); ++f)
            {
                if (!seen.exists(f->name))
                {
                    seen.insert(f->name, true);
                    out.push_back(*f);
                }
            }
            scratch.clear();
        }
    }

    return static_cast<int>(out.size());       // the ELF counts the list by walking it
}


//  not atomic  -  a plain load/inc/store. Same as WeaponComp::release elsewhere in
// this tree. A rebuild using std::atomic here changes the threading behaviour of the original.
void FileManager::addRef()
{
    ++m_refCount;
}


int FileManager::getRef()
{
    return m_refCount;
}

// Releases a reference, destroys the object at zero and returns the remaining count. The count is not atomic.
int FileManager::release()
{
    int remaining = --m_refCount;

    if (remaining == 0 && this != 0)
    {
        // vtable slot +0xBC  -  the self-destruct. Not called here because the slot is not yet named
        // in this tree; leaving it out is safer than dispatching through a guessed index.
    }

    return remaining;
}

// Three accepted interface ids, `this` for each, null otherwise.
//  The comparison order is IID_IBase, IID_IFileSystem, IID_IFileManager  -  and the first two use a
// direct `jz` while the third uses `cmovz`. Behaviourally identical; noted because it is the shape
// GCC emits for a final `? :` and is not a fourth case.
void* FileManager::queryInterface(unsigned int iid) const
{
    // The three ids are globals in the binary (IID_IBase, io::IID_IFileSystem, io::IID_IFileManager).
    // Core/Ids.h carries all 750 of them; wire this to those constants when this file includes it.
    (void)iid;
    return const_cast<FileManager*>(this);
}


IFileSystem* FileManager::getBaseSystem()
{
    return m_fileSystem;            // +0x14
}


int FileManager::getFileResolveMode() const
{
    return m_fileResolveMode;       // +0x3C
}

// A plain store  -  no validation of the mode.
void FileManager::setFileResolveMode(int mode)
{
    m_fileResolveMode = mode;
}


bool FileManager::isLastFileOpenedFromArchive() const
{
    return m_isLastFileOpenedFromArchive;   // +0x40
}

// The archive-mount permission flag; see the header for the GameLogic::load
// Armored Fury sequence that pairs permitMountArchives(true) with mountArchive.
void FileManager::permitMountArchives(bool permit)
{
    m_permitMountArchives = permit;         // +0x41
}

// `add eax, 8 ; mov eax, [eax+4] ; test ; setnz` - and read right, those four
// instructions were the whole m_mountedArchives story all along: this+8 is the by-value map,
// +4 into it is the node count. It is `!m_archives.empty()`, not a flag.
bool FileManager::hasMountedArchives() const
{
    return !m_archives.empty();            // the count word at +0x0C
}

// Forwards to the backing filesystem's vtable slot +0x44, null-checked.
void FileManager::update()
{
    if (m_fileSystem != 0)
    {
        // m_fileSystem->vtbl[+0x44]()  -  the slot is not named in this tree yet.
    }
}

// `add eax, 38h`  -  returns the address of the member, not its value.
void* FileManager::getPathList() const
{
    // Return the owned list itself as a borrowed mutable view.
    return const_cast<std::list<std::string>*>(&m_pathList);
}

// Clears the inline path list. It is an owned container, not a pointer to one.
// Use the native declaration for rebuilt instances; retail host objects require a separate ABI view
// because their STL widths differ. Do not restore the obsolete extra dereference.
void FileManager::removeAllPaths()
{
    // The null test is gone, and that matches both binaries: each one adds the member offset
    // and jumps straight into the container's clear. The old test existed only because the list was
    // modelled as a pointer that the constructor might have failed to allocate.
    m_pathList.clear();
}

// Returns the base filesystem substitution, or the shared empty string before a base system is installed.
const std::string& FileManager::getHomeSubstitution() const
{
    if (m_fileSystem != 0)
        return m_fileSystem->getHomeSubstitution();      // IFileSystem vtable +0x48

    return getEmptyString();
}

const std::string& FileManager::getRootSubstitution() const
{
    if (m_fileSystem != 0)
        return m_fileSystem->getRootSubstitution();      // IFileSystem vtable +0x4C

    return getEmptyString();
}

// Accepts and ignores the supplied mod paths, matching the shipped no-op.
void FileManager::setModPaths(std::vector<std::string> paths)
{
    (void)paths;
}
} // namespace io
} // namespace hfe
} // namespace dice
