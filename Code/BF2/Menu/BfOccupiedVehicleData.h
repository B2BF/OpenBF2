// Descriptor-attested dice::meme::BfOccupiedVehicleData; path [INFERRED].
// Complete28-slot table936D10 and descriptor9369C8.
#ifndef MENU_BFOCCUPIEDVEHICLEDATA_H
#define MENU_BFOCCUPIEDVEHICLEDATA_H
#include "../External/BFMeme/Data.h"
#if !DEDICATED
#include <map>
namespace dice { namespace meme {
class BfOccupiedVehicleData : public Data {
public:
    explicit BfOccupiedVehicleData(const char* name = nullptr);
    ~BfOccupiedVehicleData() override; // 7C8F90, only map/base destruction
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override; // 504E70, native sizeof
    bool streamProperties(System*, Stream*) override;
    static TypeDescriptor* getClassType();
    bool setOccupantIcon(int position, int icon); // 7C8DB0; [INFERRED] name
    TextureHandle getOccupantTexture(System*, int position); // 7C8CC0; System argument unused
private:
    friend struct BfOccupiedVehicleDataFixture;
    std::map<int, int> m_occupantIcons; // PE+8, signed key and icon index
    // Retail allocation/getSize48 leaves exactly seven handles after its12-byte
    // map. Constructor and getter nevertheless access index7: shipped overflow.
    // Do not enlarge this array or silently repair the allocation.
#if defined(_M_IX86)
    TextureHandle m_textures[7]; // PE+14..2F; native+10..2B
#else
    // [CHOSEN, not recovered] x64 (https://github.com/B2BF/OpenBF2/issues/7): the shipped index-7
    // write lands in the x86 heap block's padding (44 bytes in an 8-byte-granular block), but on
    // x64 the 8 bytes after the object can be the next block's header, so the write corrupted the
    // heap at random during HUD loading. x64 owns the eighth slot the shipped code writes and reads.
    TextureHandle m_textures[8];
#endif
};
#if defined(_M_IX86)
static_assert(sizeof(BfOccupiedVehicleData) == 44, "native x86 Data/map/seven handles");
#endif
} }
#endif
#endif
