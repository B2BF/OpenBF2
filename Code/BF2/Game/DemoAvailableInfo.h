// A recorded demo's download announcement: url and map, sent by the server as data block 9
// after its auto-demo hook ran. STL members make object size and offsets toolchain-dependent
// (ELF: url +0x00, mapName +0x04, dirty +0x08, net buffer +0x0C; BF2.exe: +0x00 / +0x1C / +0x38
// / +0x3C).

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

    void clear();
    const std::string& getUrl() const;
    void setUrl(const std::string& url);
    const std::string& getMapName() const;
    void setMapName(const std::string& mapName);

    void setFromDataBlock(const void* data, unsigned int size);
    const void* getNetBuffer();
    unsigned int getNetBufferSize();

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
