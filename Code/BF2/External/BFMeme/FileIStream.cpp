// All instructions7D3540..7D36D1 read; original source/type names unavailable.
#include "FileIStream.h"
#if !DEDICATED
#include "../../Core/ClassManager.h"
#include "../../Core/Ids.h"
#include "../../IO/File/File.h"
#include "../../IO/Stream/MemStream.h"
namespace dice { namespace hfe { extern ClassManager* classManager; } }
namespace dice { namespace meme {
FileIStream::FileIStream(const char* path) : IStream(nullptr), m_memory(nullptr)
{
    hfe::io::File file;
    if (file.open(path, hfe::io::FILEMODE_READ, static_cast<hfe::io::FileAccess>(0), 0)) {
        if (m_memory) m_memory->release(); // preserved pre-assignment test
        m_memory = static_cast<hfe::io::IMemStream*>(hfe::classManager->createInstance(
            hfe::io::CID_MemStream, hfe::io::IID_IMemStream, nullptr));
        m_memory->setSize(file.getSize()); // no allocation-result guard
        const int bytes = file.getSize(); // repeated read precedes getPtr in PE
        void* buffer = m_memory->getPtr();
        file.read(buffer, bytes); // retail ignores partial reads
        file.close();
    }
}
FileIStream::~FileIStream() { if (m_memory) m_memory->release(); }
int FileIStream::getAvailableBytes() { return m_memory ? m_memory->getAvailableBytes() : 0; }
int FileIStream::skip(int bytes)
{ return static_cast<unsigned char>(m_memory->skip(bytes)); }
int FileIStream::read(void* buffer, int bytes)
{ return static_cast<int>(m_memory->read(buffer, bytes)); }
int FileIStream::getPosition() { return m_memory->getPosition(); }
bool FileIStream::setPosition(int position)
{ return static_cast<unsigned char>(m_memory->seek(hfe::io::SEEKORIGIN_BEGIN, position)) != 0; }
#if defined(_M_IX86)
static_assert(sizeof(FileIStream) == 12, "client file-input visitor layout");
#endif
} }
#endif
