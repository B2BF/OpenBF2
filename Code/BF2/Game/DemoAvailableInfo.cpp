// Demo download announcement serialization for server data block 9.
#include "DemoAvailableInfo.h"

#include "../Core/StringUtil.h"
#include "../IO/BitStream.h"

#include <algorithm>
#include <cstring>

namespace dice {
namespace hfe {

namespace {

// Strings travel as an eight-bit length and that many bytes, capped at 256. Retail writes the cap
// into the eight-bit field as 0 and still sends all 256 bytes, so a reader then misparses the
// rest of the block. The defect is preserved.
void writeDemoString(io::BitStream& stream, const std::string& text)
{
    const unsigned int length = std::min<unsigned int>(static_cast<unsigned int>(text.size()), 256);
    stream.writeBits(&length, 8);
    for (unsigned int i = 0; i < length; ++i)
    {
        const int character = text[i];
        stream.writeBits(&character, 8);
    }
}

// Reads a string written by writeDemoString into a zeroed buffer and keeps the bytes before the
// first NUL.
void readDemoString(io::BitStream& stream, std::string& out)
{
    unsigned int length = 0;
    stream.readBits(&length, 8);

    char buffer[257] = {0};
    for (unsigned int i = 0; i < length; ++i)
    {
        int character = 0;
        stream.readBits(&character, 8);
        buffer[i] = static_cast<char>(character);
    }
    out.assign(buffer, std::strlen(buffer));
}

} // anonymous namespace

DemoAvailableInfo::DemoAvailableInfo()
    : m_url()
    , m_mapName()
    , m_dirty(true)
    , m_netBuffer()
{
}

void DemoAvailableInfo::clear()
{
    // Shipped quirk, shared with EndOfRoundInfo: clearing does not mark the packed buffer stale.
    m_url = m_mapName = getEmptyString();
}

const std::string& DemoAvailableInfo::getUrl() const
{
    return m_url;
}

void DemoAvailableInfo::setUrl(const std::string& url)
{
    m_url = url;
    m_dirty = true;
}

const std::string& DemoAvailableInfo::getMapName() const
{
    return m_mapName;
}

void DemoAvailableInfo::setMapName(const std::string& mapName)
{
    m_mapName = mapName;
    m_dirty = true;
}

const void* DemoAvailableInfo::getNetBuffer()
{
    if (m_dirty)
        updateNetBuffer();
    return m_netBuffer.data();
}

unsigned int DemoAvailableInfo::getNetBufferSize()
{
    if (m_dirty)
        updateNetBuffer();
    return static_cast<unsigned int>(m_netBuffer.size());
}

// Neither the stream's error flag nor a short block is reported.
void DemoAvailableInfo::setFromDataBlock(const void* data, unsigned int size)
{
    io::BitStream stream(static_cast<unsigned char*>(const_cast<void*>(data)), size);
    stream.setReadMode();
    stream.setBitSize(8 * size);

    readDemoString(stream, m_url);
    readDemoString(stream, m_mapName);
}

// Packs the URL, then the map name, into a 367-byte staging buffer and trims it to the bytes
// written. Two long strings do not fit: the stream refuses the overflowing writes, the block is
// cut short, and nothing reports it.
void DemoAvailableInfo::updateNetBuffer()
{
    m_netBuffer.resize(367, 0);
    io::BitStream stream(&m_netBuffer[0], static_cast<unsigned int>(m_netBuffer.size()));

    writeDemoString(stream, m_url);
    writeDemoString(stream, m_mapName);

    m_netBuffer.resize((stream.getBitPosition() + 7) >> 3, 0);
    m_dirty = false;
}

} // namespace hfe
} // namespace dice
