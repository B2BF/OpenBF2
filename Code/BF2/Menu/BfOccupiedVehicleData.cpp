// Client occupied-vehicle display data.
#include "BfOccupiedVehicleData.h"
#if !DEDICATED
#include "BF2HudManager.h"
#include "Ref2System.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <algorithm>
namespace dice { namespace hfe { extern hud::BF2HudManager* hudManager; } }
namespace dice { namespace meme {
BfOccupiedVehicleData::BfOccupiedVehicleData(const char* name)
    : Data(name), m_occupantIcons(), m_textures{}
{
    using hfe::hudManager;
    if (hudManager && hudManager->getSystem()) {
        m_textures[0] = hudManager->getSystem()->loadTexture("Ingame/GeneralIcons/empty.tga", true);
        m_textures[1] = hudManager->getSystem()->loadTexture("Ingame/Vehicles/Icons/Hud/Positions/vehiclePositionPlayerGreen.tga", true);
        m_textures[2] = hudManager->getSystem()->loadTexture("Ingame/Vehicles/Icons/Hud/Positions/vehiclePositionPlayerBlue.tga", true);
        m_textures[3] = hudManager->getSystem()->loadTexture("Ingame/Vehicles/Icons/Hud/Positions/vehiclePositionPlayerRed.tga", true);
        m_textures[4] = hudManager->getSystem()->loadTexture("Ingame/Vehicles/Icons/Hud/Positions/vehiclePositionEmpty.tga", true);
        m_textures[5] = hudManager->getSystem()->loadTexture("Ingame/Vehicles/Icons/Hud/Positions/vehiclePositionTeamBlue.tga", true);
        m_textures[6] = hudManager->getSystem()->loadTexture("Ingame/Vehicles/Icons/Hud/Positions/vehiclePositionTeamRed.tga", true);
        // PE7C8F79 writes at+30 although both allocation callers request30h.
        m_textures[7] = hudManager->getSystem()->loadTexture("Ingame/Vehicles/Icons/Hud/Positions/vehiclePositionSquad.tga", true);
    }
}
BfOccupiedVehicleData::~BfOccupiedVehicleData() = default;
TypeDescriptor* BfOccupiedVehicleData::getClassType()
{
    static ClassTypeDescriptor<BfOccupiedVehicleData, Data> type("dice::meme::BfOccupiedVehicleData");
    return &type;
}
TypeDescriptor* BfOccupiedVehicleData::getType() { return getClassType(); }
unsigned int BfOccupiedVehicleData::getObjectSize() { return sizeof(BfOccupiedVehicleData); }
bool BfOccupiedVehicleData::streamProperties(System* system, Stream* stream)
{ return Data::streamProperties(system, stream) != false; }
bool BfOccupiedVehicleData::setOccupantIcon(int position, int icon)
{
    auto found = m_occupantIcons.find(position);
    if (found == m_occupantIcons.end()) m_occupantIcons.insert(std::make_pair(position, icon));
    else found->second = icon;
    return true;
}
TextureHandle BfOccupiedVehicleData::getOccupantTexture(System*, int position)
{
    auto found = m_occupantIcons.find(position);
    if (found == m_occupantIcons.end()) return 0;
    return m_textures[std::max(0, std::min(7, found->second))]; // same index7 defect
}
} }
#endif
