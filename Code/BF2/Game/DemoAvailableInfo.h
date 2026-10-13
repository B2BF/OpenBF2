// The announcement a server sends when an auto-recorded demo can be downloaded: the demo's URL and
// map name, packed for server data block 9. STL members make object size and offsets
// toolchain-dependent. [INFERRED] placement beside EndOfRoundInfo, its link-order neighbour.

#ifndef GAME_DEMOAVAILABLEINFO_H
#define GAME_DEMOAVAILABLEINFO_H

#include <string>
#include <vector>

namespace dice {
namespace hfe {

class DemoAvailableInfo
{
public:
    DemoAvailableInfo();

    // Empties both strings. Leaves the packed buffer as it was.
    void clear();
    const std::string& getUrl() const;
    void setUrl(const std::string& url);
    const std::string& getMapName() const;
    void setMapName(const std::string& mapName);

    // Return the packed block, repacking first when a setter has changed a field.
    const void* getNetBuffer();
    unsigned int getNetBufferSize();
    // Reads a packed block into both strings. Leaves the packed buffer as it was.
    void setFromDataBlock(const void* data, unsigned int size);

private:
    void updateNetBuffer();

    std::string m_url;
    std::string m_mapName;
    bool m_dirty;
    std::vector<unsigned char> m_netBuffer;
};

} // namespace hfe
} // namespace dice

#endif // GAME_DEMOAVAILABLEINFO_H
