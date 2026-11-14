// [INFERRED] path. Full client PE410742/4C2510/4C25D0/4C2590/479830/4C3030/4C2A60/4C1D90/4C1FF0/
// /4C1B90/689430/4C1BA0/4C1BB0/4C1BC0/4B3AB0/4B1C80/4C1C00/4C1DB0 read. The HTTP side
// (4C1B70/4C1B80/4C1BD0 and the callbacks 4C1EC0/4C2D70/4B3A90) is recovered on the HTTPRequest
// base (Game/Persistence/HTTPRequest.cpp); 4C2DF0 (download) remains a stop.
#include "DemoLibrary.h"
#if !DEDICATED
#include "DemoHeader.h"
#include "../LocalProfileManager.h"
#include "../../Menu/ISwiffHost.h"       // postMessage - reportError's {1, text, 0} record
#include "../../Core/ClassManager.h"
#include "../../Core/Ids.h"
#include "../../Core/StringUtil.h"
#include "../../IO/File/FileManager.h"
#include "../../IO/Stream/ZStream.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if !defined(_WIN32)
#include <strings.h>                  // strcasecmp - the _stricmp of 4C26E0
#endif
#include <vector>
#include <list>
#include <new>

namespace dice { namespace hfe {
extern ClassManager* classManager;

DemoLibraryEntry::DemoLibraryEntry() : version(0) {} // constructs the five strings; the int is written by refresh

// 4C2510 (124 B): the HTTPRequest base's constructor (59C390) runs first; then the table,
// refcount 1, the empty entry vector, the four strings, progress 0, the two flags clear and the
// pending bookmark -1.
DemoLibrary::DemoLibrary()
    : m_refCount(1), m_downloadProgress(0.0f), m_flagA4(false), m_pendingBookmark(-1), m_downloadFailed(false)
{
}

// 4C25D0 (131 B): the four strings in reverse order, the entry rows and their storage, then the
// HTTPRequest base's destructor (59C3C0).
DemoLibrary::~DemoLibrary() {}

// (66 B): the settings-factory shape - aggregation refused, 176 retail bytes, construction
// only on a non-null allocation, unconditional queryInterface (the shipped null defect is kept).
IBase* DemoLibrary::create(unsigned int iid, IBase* outer)
{
    if (outer) return nullptr;
    void* allocation = ::operator new(sizeof(DemoLibrary));
    DemoLibrary* owner = allocation ? new (allocation) DemoLibrary : nullptr;
    IBase* result = static_cast<IBase*>(owner->queryInterface(iid));
    if (!result) owner->release();
    return result;
}
void DemoLibrary::addRef() { ++m_refCount; }
int DemoLibrary::getRef() { return m_refCount; }
// (26 B): decrement, destroy at zero through the destructor slot of the withheld base's table.
int DemoLibrary::release()
{
    const int references = --m_refCount;
    if (!references) delete this;
    return references;
}
// (36 B): IBase (1), then IDemoLibrary (121065).
void* DemoLibrary::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IDemoLibrary)
        return const_cast<DemoLibrary*>(this);
    return nullptr;
}

// (8 B): the scan's own result is discarded.
bool DemoLibrary::refresh()
{
    refreshEntries();
    return true;
}

// Clears existing entries, then scans the default profile's demos directory when
// a current profile exists. Adds readable bf2demo headers and releases the inflate
// wrapper before its underlying stream.
bool DemoLibrary::refreshEntries()
{
    std::vector<DemoLibraryEntry>().swap(m_entries);
    IBase* manager = classManager->getSingleton(std::string("LocalProfileManager"));
    if (!manager || !static_cast<ILocalProfileManager*>(manager)->getCurrentProfile())   // +44
        return false;
    std::string directory("@HOME@/Profiles/Default/demos/");
    std::list<io::FileInfo> files;
    io::fileManager->findFiles(directory, false, files);                                  // +34
    for (std::list<io::FileInfo>::iterator it = files.begin(); it != files.end(); ++it) {
        io::FileInfo info = *it;
        if (info.isDirectory)
            continue;
        if (!(extractFileExt(info.name) == "bf2demo"))
            continue;
        std::string path(std::string("@HOME@/Profiles/Default/demos/") += info.name);
        io::IStream* stream = io::fileManager->open(path, io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0);
        if (!stream)
            continue;
        void* allocation = ::operator new(sizeof(io::ZStream));
        if (allocation) {
            io::ZStream* reader = new (allocation) io::ZStream(stream, io::ZStream::ZSTREAMMODE_READ);
            if (reader) {
                DemoHeader header;
                if (header.readFrom(reader)) {
                    DemoLibraryEntry entry;
                    entry.version = header.version;
                    entry.serverName = header.serverName;
                    entry.dateTime = header.dateTime;
                    entry.mapName = header.mapName;
                    entry.path = path;
                    entry.name = extractFileNameWithoutExt(entry.path);
                    m_entries.push_back(entry);
                }
                reader->release();
            }
        }
        stream->release();
    }
    return true;
}

// nextFreeDemoName - 4C26E0, 689 B: "demo<N>" for the smallest N no .bf2demo in the profile demo
// folder is named after (compared case-insensitively, _stricmp). No current profile: "".
std::string DemoLibrary::nextFreeDemoName()
{
    IBase* manager = classManager->getSingleton(std::string("LocalProfileManager"));
    if (!manager || !static_cast<ILocalProfileManager*>(manager)->getCurrentProfile())  // +44
        return std::string("");
    const std::string directory("@HOME@/Profiles/Default/demos/");
    std::string candidate;
    std::vector<std::string> taken;
    std::list<io::FileInfo> files;
    io::fileManager->findFiles(directory, false, files);                                  // +34
    for (std::list<io::FileInfo>::iterator it = files.begin(); it != files.end(); ++it) {
        io::FileInfo info = *it;
        if (info.isDirectory)
            continue;
        if (extractFileExt(info.name) == "bf2demo")
            taken.push_back(extractFileNameWithoutExt(info.name));
    }
    for (int index = 0;; ++index) {
        candidate = formatString("demo%d", index);
        std::vector<std::string>::size_type i = 0;
#if defined(_WIN32)
        while (i < taken.size() && ::_stricmp(taken[i].c_str(), candidate.c_str()) != 0)
#else
        while (i < taken.size() && ::strcasecmp(taken[i].c_str(), candidate.c_str()) != 0)
#endif
            ++i;
        if (i == taken.size())
            break;
    }
    return candidate;
}

// download - 4C2DF0, 571 B: a busy transfer is cancelled first; the name, url and description
// are kept for the menu, the progress reset. With a current profile, the demo folder is touched
// into existence (dummy.txt written, then deleted) and the file goes to <home>/profiles/Default/
// demos/<nextFreeDemoName()>.bf2demo through the base's asynchronous getFile. A refused request
// reports "Unable to download demo file". The completion callback rescans the library.
bool DemoLibrary::download(const std::string& name, const std::string& url, const std::string& description)
{
    if (HTTPRequest::isUpdating())                                                        // 59C400
        HTTPRequest::cancel();                                                            // 59C440
    m_downloadFailed = false;
    m_downloadName = name;
    m_downloadUrl = url;
    m_downloadDescription = description;
    m_downloadProgress = 0.0f;
    IBase* manager = classManager->getSingleton(std::string("LocalProfileManager"));
    if (!manager || !static_cast<ILocalProfileManager*>(manager)->getCurrentProfile())  // +44
        return false;
    // fileManager +0x54 (the base file system) +0x48: the @HOME@ substitution.
    std::string folder(io::fileManager->getBaseSystem()->getHomeSubstitution());
#if defined(_WIN32)
    folder += "\\profiles\\Default\\demos\\";
#else
    // [PLATFORM] The GameSpy HTTP SDK opens the target with fopen, so the POSIX client spells the
    // folder with '/' and in the lower case UnixFileSystem::fixUnixPath gives every path after the
    // home prefix (the library scans the same folder through it).
    folder += "/profiles/default/demos/";
#endif
    const std::string dummy(folder + "dummy.txt");
    io::fileManager->writeFile(dummy, 0, 0, true);                                        // +30
    io::fileManager->deleteFile(dummy);                                                   // +20
    const std::string target(folder + nextFreeDemoName() + ".bf2demo");
    m_downloadPath = target;                                                              // +30
    if (!HTTPRequest::getFile(url, target, true)) {                                       // 59C5B0
        reportError(std::string("Unable to download demo file"));
        return false;
    }
    return true;
}

unsigned int DemoLibrary::getNumEntries()
{
    return static_cast<unsigned int>(m_entries.size());
}

//: the count is read through the table.
DemoLibraryEntry* DemoLibrary::getEntry(unsigned int index)
{
    if (index < getNumEntries())
        return &m_entries[index];
    return nullptr;
}

// (113 B): past the count nothing happens; otherwise the row's file is deleted (the result
// is not read) and the row erased.
bool DemoLibrary::removeEntry(unsigned int index)
{
    if (index >= getNumEntries())
        return false;
    io::fileManager->deleteFile(m_entries[index].path);                                  // +20
    m_entries.erase(m_entries.begin() + index);
    return true;
}

// 4C1B70: `add ecx,4; jmp 59C420` - the base's think (no SDK call while both handles are -1).
// The tail jump leaves no defined answer in EAX; the only caller (SwiffHost::update) drops it.
int DemoLibrary::update()
{
    HTTPRequest::update();
    return 0;
}
// 4C1B80: `add ecx,4; jmp 59C400` - the base's pending-transfer test.
bool DemoLibrary::isDownloading()
{
    return HTTPRequest::isUpdating();
}
// 4C1BD0: through the table's +28 (isDownloading); when busy, the base's cancel (59C440), then
// fileManager +20 deletes the target file (+30), and true. Idle answers the +28 false.
bool DemoLibrary::cancelDownload()
{
    if (!isDownloading())
        return false;
    HTTPRequest::cancel();
    io::fileManager->deleteFile(m_downloadPath);                                          // +20
    return true;
}
// 4C1EC0 (this = base +4): failed flag (+AC) set, the base failure (59C640), the message posted
// when it is not "" (operator!= 87F324 against 87FB54), then the partial target (+30) deleted.
void DemoLibrary::onGetFailure(const std::string& message)
{
    m_downloadFailed = true;
    HTTPRequest::onGetFailure(message);
    if (message != "")
        reportError(message);
    io::fileManager->deleteFile(m_downloadPath);                                          // +20
}
// 4C2D70: failed flag cleared, the base completion (59C660); a pending bookmark (+A8 != -1) goes
// to LocalProfileManager +B0 when that singleton exists, then is reset to -1; then the rescan 4C2A60.
void DemoLibrary::onGetFileComplete()
{
    m_downloadFailed = false;
    HTTPRequest::onGetFileComplete();
    if (m_pendingBookmark != -1) {
        IBase* manager = classManager->getSingleton(std::string("LocalProfileManager"));
        if (manager)
            static_cast<ILocalProfileManager*>(manager)->removeDemoBookmark(static_cast<unsigned int>(m_pendingBookmark)); // +B0
        m_pendingBookmark = -1;
    }
    refreshEntries();
}
// 4B3A90 (ICF-shared): the progress float is stored at +A0.
void DemoLibrary::onProgress(float progress) { m_downloadProgress = progress; }

const std::string& DemoLibrary::getDownloadName() { return m_downloadName; }
const std::string& DemoLibrary::getDownloadUrl() { return m_downloadUrl; }
const std::string& DemoLibrary::getDownloadDescription() { return m_downloadDescription; }
float DemoLibrary::getDownloadProgress() { return m_downloadProgress; }
bool DemoLibrary::getFlagA4() { return m_flagA4; }
void DemoLibrary::setFlagA4(bool value) { m_flagA4 = value; }
void DemoLibrary::setPendingBookmark(int index) { m_pendingBookmark = index; }
bool DemoLibrary::hasDownloadFailed() { return m_downloadFailed; }

// (117 B): the SwiffHost singleton, when present, receives the message record {1, text, 0}
// through +1E4; absent, nothing happens. Reached by the download path and the failure callback.
void DemoLibrary::reportError(const std::string& message)
{
    IBase* host = classManager->getSingleton(std::string("SwiffHost"));
    if (host)
        static_cast<ui::ISwiffHost*>(host)->postMessage(SwiffHostMessage(1, message, 0));   // +0x1E4
}

} }
#endif
