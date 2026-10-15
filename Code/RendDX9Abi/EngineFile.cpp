// See EngineFile.h for what is recovered here and what is not.

#ifdef BF2_IMAGE_RENDDX9

#include "EngineFile.h"
#include "PluginEntry.h"
#include "BfMemory.h"
#include "Frontier.h"

namespace renddx9abi {

namespace {

// The two literals the shipped call passes:  open(path, 2, 0, 0).

// [UNRESOLVED] the enumerators. FileManager.h names the parameters FileMode and FileAccess but
// records their enumerators as unrecovered, and nothing in this image spells them out. What is
// certain is the pair the effect compiler uses, because it is read from the call site - so they are
// reproduced as the literals they are rather than dressed up in an invented enum.
const int kFileModeShipped   = 2;
const int kFileAccessShipped = 0;

} // anonymous namespace

EngineFileData readEngineFile(const char* path)
{
    EngineFileData out;

    if (!::dice::hfe::fileManager || !path)
        return out;

    HostString name(path);

    // FileManager +0x18 - IStream* open(const std::string&, FileMode, FileAccess, unsigned int)
    HostStream* stream = ::dice::hfe::fileManager->open(
        name, static_cast<HostFileMode>(kFileModeShipped),
        static_cast<HostFileAccess>(kFileAccessShipped), 0);

    if (!stream)
        return out;

    const int size = static_cast<HostNonlinearStream*>(stream)->getSize();   // +0x38 getSize
    if (size <= 0)
    {
        stream->release();                                                   // +0x08 release
        return out;
    }

    // The original allocates with the engine allocator here (sub_1005F340), not the CRT's - the
    // buffer is handed on to D3DXCreateEffect and then freed with sub_1005F370.
    unsigned char* buffer = static_cast<unsigned char*>(engineAlloc(static_cast<unsigned int>(size)));
    if (!buffer)
    {
        stream->release();
        return out;
    }

    const unsigned int got = stream->read(buffer, static_cast<unsigned int>(size));

    stream->release();                                                       // +0x08 release

    out.data = buffer;
    out.size = got;
    return out;
}

void freeEngineFile(EngineFileData& file)
{
    if (file.data)
        engineFree(file.data);
    file.data = 0;
    file.size = 0;
}

bool engineFileExists(const std::string& path)
{
    HostString hostPath = makeHostString(path);
    return ::dice::hfe::fileManager->fileExists(hostPath);
}

void engineFilePrepareWrite(const std::string& path)
{
    HostString hostPath = makeHostString(path);
    HostStream* stream = ::dice::hfe::fileManager->open(
        hostPath, static_cast<HostFileMode>(3), static_cast<HostFileAccess>(2), 0);
    if (stream)
        stream->release();
}

void engineFileGetWritePath(const std::string& path, std::string& out)
{
    const HostString hostPath = makeHostString(path);
    HostString hostOut;
    ::dice::hfe::fileManager->getWritePath(hostPath, hostOut);
    assignFromHostString(out, hostOut);
}

// Closes any held stream before opening the new path through the engine file interface.

bool EngineStream::open(const std::string& path)
{
    close();

    if (!::dice::hfe::fileManager)
        return false;

    HostString hostPath = makeHostString(path);
    HostStream* stream = ::dice::hfe::fileManager->open(
        hostPath, static_cast<HostFileMode>(kFileModeShipped),
        static_cast<HostFileAccess>(kFileAccessShipped), 0);
    m_stream = static_cast<HostNonlinearStream*>(stream);

    return m_stream != 0;
}

void EngineStream::close()
{
    if (!m_stream)
        return;
    m_stream->release();
    m_stream = 0;
}

unsigned int EngineStream::read(void* buffer, unsigned int size)
{
    if (!m_stream)
        return 0;
    return m_stream->read(buffer, size);
}

int EngineStream::seek(int origin, int offset)
{
    if (!m_stream)
        return -1;
    return m_stream->seek(static_cast<HostSeekOrigin>(origin), offset);
}

int EngineStream::getSize()
{
    if (!m_stream)
        return 0;
    return m_stream->getSize();
}

} // namespace renddx9abi

#endif // BF2_IMAGE_RENDDX9
