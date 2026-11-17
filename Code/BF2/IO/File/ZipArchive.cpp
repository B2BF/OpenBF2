// ZIP archive and streaming readers backed by the vendored minizip library.

#include "ZipArchive.h"

#include "../../Core/Debug.h"       // the five attested sites
#include "../../Core/Ids.h"         // CID_MemStream, the IID globals
#include "../../Core/SmartItf.h"    // open()'s small-file path
#include "../../Core/StringUtil.h"  // setLowerCase, getLowerCase, fixPathDelimiters, fixPath,
                                    // extractFilePath, extractFileName, equalsIgnoreCase,
                                    // getEmptyString
#include "../Stream/MemStream.h"    // IMemStream - the small-file open target

#include <cstring>                  // memset, memcpy
#if defined(BF2_LINUX_CLIENT) && !DEDICATED
#include "../../Platform/Linux/ClientFilePaths.h"
#endif

namespace dice {
namespace hfe {
namespace io {

// Starts with one reference, retains handler and enables content checking.
ZipArchive::ZipArchive(ZipArchiveHandler& handler)
    : m_refCount(1)             // +0x04
    , m_handler(&handler)       // +0x08
    , m_filename()              // +0x0C
    , m_path()                  // +0x10
    , m_string14()              // +0x14 - dead member, constructed and destroyed only
    , m_entries()               // +0x18
    , m_freeHandles()           // +0x20
    , m_handles()               // +0x24
    , m_contentCheck(true)      // +0x2C
{
    m_handler->addRef();
}

// Releases the handler before destroying members and closing pooled file handles.
ZipArchive::~ZipArchive()
{
    m_handler->release();
}

// IBase

void ZipArchive::addRef() { ++m_refCount; }
int  ZipArchive::getRef() { return m_refCount; }

// the ELF convention - test the count after decrement, dispatch D0 through vptr+0x54.
int ZipArchive::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// const, three iids on the one subobject.
void* ZipArchive::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IFileSystem || iid == IID_IArchive)
        return const_cast<ZipArchive*>(this);
    return 0;
}

// FileHandle

// both members zero - an uninitialised handle is inert.
ZipArchive::FileHandle::FileHandle()
    : fileHandle(0)
    , buffer(0)
{
}

// close before free, each guarded independently.
ZipArchive::FileHandle::~FileHandle()
{
    if (fileHandle)
        unzClose(fileHandle);
    if (buffer)
        delete[] buffer;
}

// unzOpen the archive by its (NUL-terminated - the ELF's c_str() store is visible)
// name; only on success is the 64 KB staging buffer allocated.
bool ZipArchive::FileHandle::init(const std::string& archiveName)
{
#if defined(BF2_LINUX_CLIENT) && !DEDICATED
    // [CHOSEN, not recovered] This minizip open bypasses UnixFileSystem. Resolve only the disk
    // spelling; archive keys, entry lookup and FileManager's mount order stay unchanged.
    std::string physicalName = archiveName;
    linuxclient::resolveClientPath(physicalName);
    fileHandle = unzOpen(physicalName.c_str());
#else
    fileHandle = unzOpen(archiveName.c_str());
#endif
    if (!fileHandle)
        return false;

    buffer = new char[0x10000];
    return true;
}

// The handle pool

// Free list empty -> push_front a fresh FileHandle into the owning list, init it
// against m_filename (erasing it again and answering null when unzOpen fails), and push its
// address onto the free list. Then, always: take the free list's front, erase that node, hand the
// handle out. Handles therefore cycle; none is ever destroyed before the archive is.
ZipArchive::FileHandle* ZipArchive::openFileHandle()
{
    if (m_freeHandles.empty())
    {
        m_handles.push_front(FileHandle());
        FileHandle* fresh = &m_handles.front();

        if (!fresh->init(m_filename))
        {
            m_handles.pop_front();
            return 0;
        }

        m_freeHandles.push_front(fresh);
    }

    FileHandle* handle = m_freeHandles.front();
    m_freeHandles.pop_front();
    return handle;
}

// close the current file only (the archive handle stays open for reuse), back onto the
// free list. Null-safe.
ZipArchive::FileHandle* ZipArchive::closeFileHandle(FileHandle* handle)
{
    if (handle)
    {
        unzCloseCurrentFile(handle->fileHandle);
        m_freeHandles.push_front(handle);
    }
    return handle;
}

// File-time comparison is unsupported and emits a fatal diagnostic.
bool ZipArchive::compareFileTime(const std::string& /*a*/, const std::string& /*b*/)
{
    Debug(DEBUGTYPE_6, "IO/File/ZipArchive.cpp", 271) << "ZIP findFiles not impl yet";
    return false;
}

// 0x848B340, dice line 295. Same copy-paste text, same fatality.
bool ZipArchive::getFullFileName(std::string& /*path*/)
{
    Debug(DEBUGTYPE_6, "IO/File/ZipArchive.cpp", 295) << "ZIP findFiles not impl yet";
    return false;
}

// Opens file and loads its entry table. A corrupt entry table emits a fatal diagnostic.
// baseSystem and flag are ignored.
bool ZipArchive::open(const std::string& file, IFileSystem* /*baseSystem*/, bool /*flag*/)
{
    m_filename.assign(file);

    FileHandle* handle = openFileHandle();
    if (!handle)
        return false;

    const bool ok = readEntries(handle);
    if (!ok)
        Debug(DEBUGTYPE_6, "IO/File/ZipArchive.cpp", 395) << "couldn't read entries from " << file;

    closeFileHandle(handle);
    return ok;
}

// Indexes nondirectory entries by lowercase name; the first duplicate wins.
// Rewinds the archive afterward. An empty archive succeeds; metadata read errors return false.
bool ZipArchive::readEntries(FileHandle* handle)
{
    unz_global_info globalInfo;
    if (unzGetGlobalInfo(handle->fileHandle, &globalInfo) != UNZ_OK)
        return false;

    char nameBuffer[0x1000];

    for (uLong i = 0; i < globalInfo.number_entry; ++i)
    {
        Entry entry;
        memset(&entry, 0, sizeof(entry));           // the ELF memsets all 0x58 bytes

        if (unzGetCurrentFileInfo(handle->fileHandle, &entry.info,
                                  nameBuffer, 0x1000, 0, 0, 0, 0) != UNZ_OK)
            return false;

        const std::string lower = getLowerCase(std::string(nameBuffer));
        if (!lower.empty())
        {
            const char last = lower[lower.size() - 1];
            if (last != '\\' && last != '/')
            {
                unzGetFilePos(handle->fileHandle, &entry.pos);
                m_entries.insert(std::make_pair(lower, entry));
            }
        }

        unzGoToNextFile(handle->fileHandle);
    }

    unzGoToFirstFile(handle->fileHandle);
    return true;
}

// lookup

// Lowercases path and removes a mount-path-sized prefix when the mount name occurs anywhere in it.
// Callers must supply a path whose mount name is at the beginning.
bool ZipArchive::checkAndFixPathToFitZip(std::string& path)
{
    setLowerCase(path);
    fixPathDelimiters(path, '/');

    if (path.find(m_path) == std::string::npos)
        return false;

    path = path.substr(m_path.size());
    return true;
}

// a map find; null on miss, the node's Entry on hit.
ZipArchive::Entry* ZipArchive::findEntry(const std::string& fixedPath)
{
    std::map<std::string, Entry>::iterator it = m_entries.find(fixedPath);
    if (it == m_entries.end())
        return 0;
    return &it->second;
}


bool ZipArchive::fileExists(const std::string& name)
{
    std::string fixed(name);
    if (!checkAndFixPathToFitZip(fixed))
        return false;

    return findEntry(fixed) != 0;
}

// The stream-open slot

// see the header's regime note. FILEMODE_READ == 2; anything below that, or a
// write-ish access, is refused outright.
IStream* ZipArchive::open(const std::string& name, FileMode mode, FileAccess access,
                          unsigned int /*flags*/)
{
    if (static_cast<unsigned int>(mode) <= 1 || access == FILEACCESS_2 || access == FILEACCESS_1)
        return 0;

    std::string fixed(name);
    if (!checkAndFixPathToFitZip(fixed))
        return 0;

    Entry* entry = findEntry(fixed);
    if (!entry)
        return 0;

    const unsigned int uncompressed = entry->info.uncompressed_size;

    if (uncompressed <= 0xFFFF)
    {
        // Small entry: inflate the whole thing now into a MemStream. No null check after
        // create - a missing CID_MemStream registration crashes here in the original too.
        FileHandle* handle = openFileHandle();
        if (!handle || unzGoToFilePos(handle->fileHandle, &entry->pos) != UNZ_OK)
            return 0;

        unzOpenCurrentFile(handle->fileHandle);

        SmartItf<IMemStream, &IID_IMemStream> memStream;
        memStream.create(CID_MemStream);

        memStream->setSize(uncompressed);
        void* buffer = memStream->getPtr();

        const int got = unzReadCurrentFile(handle->fileHandle, buffer, uncompressed);
        closeFileHandle(handle);

        if (got != static_cast<int>(uncompressed))
        {
            memStream->release();
            return 0;
        }

        // The holder has no destructor (see SmartItf.h), so the single reference from create()
        // walks out with the return - the ELF's addRef-then-smart-release nets the same count.
        IMemStream* result = memStream;
        return result;
    }

    // Large entry: a streaming reader over a pooled handle.
    return new ZipReadStream(*this, *entry);
}

// Whole-entry read / fake write

// the two preserved defects are marked inline.
bool ZipArchive::readFile(const std::string& name, void* buffer, unsigned int size,
                          unsigned int& bytesRead)
{
    bytesRead = 0;

    std::string fixed(name);
    if (!checkAndFixPathToFitZip(fixed))
        return false;

    Entry* entry = findEntry(fixed);
    if (!entry)
        return false;

    if (entry->info.uncompressed_size > size)
        return false;

    FileHandle* handle = openFileHandle();              // not null-checked - the original derefs
    if (unzGoToFilePos(handle->fileHandle, &entry->pos) != UNZ_OK)
        return false;                                   // handle leaked from the pool - binary's

    unzOpenCurrentFile(handle->fileHandle);
    const int got = unzReadCurrentFile(handle->fileHandle, buffer, entry->info.uncompressed_size);
    closeFileHandle(handle);

    if (got != static_cast<int>(entry->info.uncompressed_size))
        return false;

    bytesRead = static_cast<unsigned int>(got);
    return true;
}

// fake success, reproduced: see the header. Writes nothing, ever.
bool ZipArchive::writeFile(const std::string& name, const void* /*buffer*/, unsigned int size,
                           bool /*append*/)
{
    std::string fixed(name);
    if (!checkAndFixPathToFitZip(fixed))
        return false;

    Entry* entry = findEntry(fixed);
    if (!entry)
        return false;

    if (entry->info.uncompressed_size > size)
        return false;

    FileHandle* handle = openFileHandle();              // borrowed and returned, for nothing -
    closeFileHandle(handle);                            // the binary's exact ritual
    return true;
}

// Enumeration

// see the header for the two match modes and the FileInfo field notes.
int ZipArchive::findFiles(const std::string& pattern, bool recursive, std::list<FileInfo>& out)
{
    std::string lowered = getLowerCase(pattern);
    fixPath(lowered, '/');                              // normalise and guarantee a trailing '/'

    out.clear();

    for (std::map<std::string, Entry>::iterator it = m_entries.begin();
         it != m_entries.end(); ++it)
    {
        const std::string full = m_path + it->first;

        FileInfo info;                                  // the six trailing dwords stay
        info.isDirectory = false;                       //   uninitialised, as inserted by the ELF
        info.size = it->second.info.uncompressed_size;  // the size field - see the header

        if (!recursive)
        {
            const std::string dir = extractFilePath(full, '/');
            if (!equalsIgnoreCase(lowered, dir))
                continue;
            info.name = extractFileName(full);
        }
        else
        {
            if (!equalsIgnoreCase(lowered, full, static_cast<int>(lowered.size())))
                continue;
            info.name = full.substr(lowered.size());
        }

        out.push_back(info);
    }

    return static_cast<int>(out.size());
}

// name is the caller'S string, the size is stored, isDirectory untouched.
bool ZipArchive::getFileInfo(const std::string& name, FileInfo& out)
{
    std::string fixed(name);
    if (!checkAndFixPathToFitZip(fixed))
        return false;

    Entry* entry = findEntry(fixed);
    if (!entry)
        return false;

    out.name = name;
    out.size = entry->info.uncompressed_size;
    out.creationTimeLow    = 0;
    out.creationTimeHigh   = 0;
    out.lastAccessTimeLow  = 0;
    out.lastAccessTimeHigh = 0;
    out.lastWriteTimeLow   = 0;
    out.lastWriteTimeHigh  = 0;
    return true;
}

// The trivia

bool ZipArchive::deleteFile(const std::string&)                        { return false; }
bool ZipArchive::moveFile(const std::string&, const std::string&)      { return false; }
bool ZipArchive::copyFile(const std::string&, const std::string&, bool){ return false; }
void ZipArchive::update()                                              {}

const std::string& ZipArchive::getHomeSubstitution() const { return getEmptyString(); }
const std::string& ZipArchive::getRootSubstitution() const { return getEmptyString(); }

IArchiveHandler*   ZipArchive::getHandler()                  { return m_handler; }
const std::string& ZipArchive::getFilename() const           { return m_filename; }
void               ZipArchive::setPath(const std::string& p) { m_path.assign(p); }
void               ZipArchive::setContentCheck(bool check)   { m_contentCheck = check; }
bool               ZipArchive::getContentCheck()             { return m_contentCheck; }

// ZipReadStream

// refcount, backrefs, addref the archive, then init() - construction opens a handle
// and the entry immediately.
ZipReadStream::ZipReadStream(ZipArchive& archive, ZipArchive::Entry& entry)
    : m_refCount(1)
    , m_archive(&archive)
    , m_entry(&entry)
    , m_handle(0)
    , m_position(0)
    , m_buffered(0)
    , m_bufferPos(0)
{
    m_archive->addRef();
    init();
}

// Opens the same archive entry as a fresh stream at position zero; does not copy the cursor.
ZipReadStream::ZipReadStream(const ZipReadStream& other)
    : m_refCount(1)
    , m_archive(other.m_archive)
    , m_entry(other.m_entry)
    , m_handle(0)
    , m_position(0)
    , m_buffered(0)
    , m_bufferPos(0)
{
    m_archive->addRef();
    init();
}

// hand the handle back, drop the archive reference.
ZipReadStream::~ZipReadStream()
{
    m_archive->closeFileHandle(m_handle);
    m_archive->release();
}

// borrow a handle, teleport to the entry, open it. no null check on the borrowed
// handle - an archive whose backing file vanished crashes here, in the original too.
int ZipReadStream::init()
{
    m_bufferPos = 0;
    m_buffered = 0;
    m_position = 0;

    m_handle = m_archive->openFileHandle();
    unzGoToFilePos(m_handle->fileHandle, &m_entry->pos);
    return unzOpenCurrentFile(m_handle->fileHandle);
}

// IBase

void ZipReadStream::addRef() { ++m_refCount; }
int  ZipReadStream::getRef() { return m_refCount; }

// D1 + operator delete, direct - the class has no virtual destructor (its vtable ends
// at getName), so destruction through the static type is the binary's own shape.
int ZipReadStream::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* ZipReadStream::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IStream || iid == IID_INonlinearStream)
        return const_cast<ZipReadStream*>(this);
    return 0;
}

// The read path

// Serve from the staged buffer first; then either refill the 64 KB stage (small
// remainder) or bypass it entirely (remainder over 0xFFFF), resetting the stage. Both gate
// calls - getSize and getAvailableBytes - are virtual calls on self in the binary; kept virtual.
unsigned int ZipReadStream::read(void* dest, unsigned int n)
{
    if (m_position >= static_cast<unsigned int>(getSize()))
        return 0;

    if (!n)
        return 0;

    char* out = static_cast<char*>(dest);
    unsigned int copied = 0;

    if (m_bufferPos < m_buffered)
    {
        const unsigned int staged = m_buffered - m_bufferPos;
        if (staged >= n)
        {
            memcpy(out, m_handle->buffer + m_bufferPos, n);
            m_position += n;
            m_bufferPos += n;
            return n;
        }

        memcpy(out, m_handle->buffer + m_bufferPos, staged);
        m_position += staged;
        out += staged;
        n -= staged;
        copied = staged;
    }

    if (n <= 0xFFFF)
    {
        const unsigned int remaining = static_cast<unsigned int>(getAvailableBytes());
        unsigned int chunk = 0x10000;
        if (remaining < 0x10000)
            chunk = remaining;

        const unsigned int got =
            unzReadCurrentFile(m_handle->fileHandle, m_handle->buffer, chunk);
        m_buffered = got;

        unsigned int take = got;
        if (n <= got)
            take = n;
        m_bufferPos = take;

        memcpy(out, m_handle->buffer, take);
        m_position += take;
        return take + copied;
    }

    m_buffered = 0;
    m_bufferPos = 0;
    const int got = unzReadCurrentFile(m_handle->fileHandle, out, n);
    m_position += got;
    return static_cast<unsigned int>(got) + copied;
}

// 0x848C070, dice line 599 - severity 6: writing to a zip stream is fatal on emission.
unsigned int ZipReadStream::write(const void* /*src*/, unsigned int /*n*/)
{
    Debug(DEBUGTYPE_6, "IO/File/ZipArchive.cpp", 599) << "write() not valid on ZipReadStream";
    return 0;
}

// skip by reading - an inflate stream has no cheaper forward motion. Allocates the
// whole skip distance as a temporary (a 100 MB skip allocates 100 MB), and answers 1 only when
// every byte arrived. skip(0) is success without work.
int ZipReadStream::skip(unsigned int n)
{
    if (!n)
        return 1;

    char* scratch = new char[n];
    const unsigned int got = read(scratch, n);      // virtual on self in the binary
    const bool ok = (got == n);
    delete[] scratch;
    return ok ? 1 : 0;
}

int ZipReadStream::getAvailableBytes()
{
    return static_cast<int>(m_entry->info.uncompressed_size - m_position);
}

// a fresh stream at position zero, via the copy constructor.
IStream* ZipReadStream::clone()
{
    return new ZipReadStream(*this);
}

bool ZipReadStream::canRead()  { return true; }
bool ZipReadStream::canWrite() { return false; }

// Seeks within the supported forward/reset cases. Unsupported seeks emit a fatal diagnostic.
int ZipReadStream::seek(SeekOrigin origin, int offset)
{
    if (origin == SEEKORIGIN_BEGIN)
    {
        const unsigned int target = static_cast<unsigned int>(offset);
        if (target == m_position)
            return 1;

        if (target >= m_position)
            return skip(target - m_position);       // virtual on self in the binary

        // Backward: close, teleport back to the entry, reopen, skip from zero.
        m_bufferPos = 0;
        m_buffered = 0;
        m_position = 0;
        unzCloseCurrentFile(m_handle->fileHandle);
        unzGoToFilePos(m_handle->fileHandle, &m_entry->pos);
        unzOpenCurrentFile(m_handle->fileHandle);
        return skip(target);
    }

    if (origin == SEEKORIGIN_CURRENT && offset >= 0)
        return skip(static_cast<unsigned int>(offset));

    if (origin == SEEKORIGIN_END && offset == 0)
    {
        m_position = static_cast<unsigned int>(getSize());  // virtual on self in the binary
        return 1;
    }

    Debug(DEBUGTYPE_7, "IO/File/ZipArchive.cpp", 634)
        << "seek() not all modes supported in ZipReadStream, mode:" << static_cast<int>(origin)
        << " offset:" << offset;
    return 0;
}

int ZipReadStream::getPosition() { return static_cast<int>(m_position); }
int ZipReadStream::getSize()     { return static_cast<int>(m_entry->info.uncompressed_size); }

// the literal, in a function-local static (guard 0x8AC3D90, object 0x8AC3D98).
const std::string& ZipReadStream::getName()
{
    static const std::string name("Name not stored per zipStream");
    return name;
}


} // namespace io
} // namespace hfe
} // namespace dice
