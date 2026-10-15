// Full client instructions7D36E0..7D37F1 read. No dedicated ELF counterpart.
#include "FileOStream.h"
#if !DEDICATED
#include "../../IO/File/FileManager.h"
namespace dice { namespace meme {
FileOStream::FileOStream(const char* path) : m_file(nullptr)
{
    const std::string name(path);
    hfe::io::IStream* opened = hfe::io::fileManager->open(name,
        static_cast<hfe::io::FileMode>(3), hfe::io::FILEACCESS_2, 0);
    if (m_file) { m_file->release(); m_file = nullptr; }
    m_file = static_cast<hfe::io::INonlinearStream*>(opened);
}
FileOStream::~FileOStream() { if (m_file) m_file->release(); }
int FileOStream::write(const void* buffer, int bytes)
{ return static_cast<int>(m_file->write(buffer, bytes)); }
int FileOStream::getPosition() { return m_file->getPosition(); }
bool FileOStream::setPosition(int position)
{ return static_cast<unsigned char>(m_file->seek(hfe::io::SEEKORIGIN_BEGIN, position)) != 0; }
#if defined(_M_IX86)
static_assert(sizeof(FileOStream) == 8, "client file-output stream layout");
#endif
} }
#endif
