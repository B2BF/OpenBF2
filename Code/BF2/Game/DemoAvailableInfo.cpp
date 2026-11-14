// Demo download announcement serialization (data block 9).
// Recovered from the symbolized dedicated ELF (0x8054B10..0x8055120); the client's reader is
// BF2.exe 0x5BCB40, its constructor 0x5BCC50.
#include "DemoAvailableInfo.h"

#include "../Core/StringUtil.h"               // getEmptyString - clear()
#include "../IO/BitStream.h"

#include <cstring>

namespace dice {
namespace hfe {

namespace {

// Each string is an 8-bit length, at most 256 (which the 8-bit field writes as 0, as shipped),
// then its bytes.
void writeAnnouncedString(io::BitStream& stream, const std::string& text)
{
    const unsigned int length = text.size() < 0x100 ? static_cast<unsigned int>(text.size()) : 0x100;
    stream.writeBits(&length, 8);
    for (unsigned int i = 0; i < length; ++i)
    {
        const int character = static_cast<signed char>(text[i]);
        stream.writeBits(&character, 8);
    }
}

// The reader fills a zeroed 257-byte buffer and keeps strlen of it, so an embedded NUL truncates.
void readAnnouncedString(io::BitStream& stream, std::string& out)
{
    unsigned int length = 0;
    stream.readBits(&length, 8);
    char buffer[257];
    std::memset(buffer, 0, sizeof(buffer));
    for (unsigned int i = 0; i < length; ++i)
    {
        unsigned int character = 0;
        stream.readBits(&character, 8);
        buffer[i] = static_cast<char>(character);
    }
    out.assign(buffer, std::strlen(buffer));
}

} // namespace

// DemoAvailableInfo - ELF 0x8054B10 / 0x8054B60.
DemoAvailableInfo::DemoAvailableInfo()
    : m_url()
    , m_mapName()
    , m_dirty(true)
    , m_netBuffer()
{
}

// clear - ELF 0x8054BB0: both strings emptied (map first); the buffer is not dirtied, as shipped.
void DemoAvailableInfo::clear()
{
    m_url = (m_mapName = getEmptyString());
}

const std::string& DemoAvailableInfo::getUrl() const { return m_url; }               // 0x8054BF0

void DemoAvailableInfo::setUrl(const std::string& url)                               // 0x8054C00
{
    m_url = url;
    m_dirty = true;
}

const std::string& DemoAvailableInfo::getMapName() const { return m_mapName; }       // 0x8054C30

void DemoAvailableInfo::setMapName(const std::string& mapName)                       // 0x8054C40
{
    m_mapName = mapName;
    m_dirty = true;
}

// setFromDataBlock - ELF 0x8054C70, 459 B. No stream-error report.
void DemoAvailableInfo::setFromDataBlock(const void* data, unsigned int size)
{
    io::BitStream stream(static_cast<unsigned char*>(const_cast<void*>(data)), size);
    stream.setReadMode();
    stream.setBitSize(8 * size);
    readAnnouncedString(stream, m_url);
    readAnnouncedString(stream, m_mapName);
}

const void* DemoAvailableInfo::getNetBuffer()                                        // 0x8054E40
{
    if (m_dirty)
        updateNetBuffer();
    return m_netBuffer.empty() ? 0 : &m_netBuffer[0];
}

unsigned int DemoAvailableInfo::getNetBufferSize()                                   // 0x8054E70
{
    if (m_dirty)
        updateNetBuffer();
    return static_cast<unsigned int>(m_netBuffer.size());
}

// updateNetBuffer - ELF 0x8054EA0, 640 B: a 367-byte staging buffer trimmed to the bits written.
void DemoAvailableInfo::updateNetBuffer()
{
    m_netBuffer.resize(0x16F, 0);
    io::BitStream stream(&m_netBuffer[0], static_cast<unsigned int>(m_netBuffer.size()));
    writeAnnouncedString(stream, m_url);
    writeAnnouncedString(stream, m_mapName);
    m_netBuffer.resize((stream.getBitPosition() + 7) >> 3, 0);
    m_dirty = false;
}

} // namespace hfe
} // namespace dice
