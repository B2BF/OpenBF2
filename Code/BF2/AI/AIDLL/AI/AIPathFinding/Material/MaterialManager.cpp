
#include "MaterialManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x322E90, __tcf_0 0x322EF0, _GLOBAL__I 0x322F20).
#include <iostream>

#include "../../AIUtils/checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x322880 (C2; C1 0x322AF0). Nine materials at cost 1, then Invalid -1 and both danger
// objects 1000.
MaterialManager::MaterialManager()
{
    for (unsigned char i = 0; i < 9; ++i)
    {
        MaterialContainer* const container = new MaterialContainer();
        checkPointer(container);
        m_materials.push_back(container);
    }

    setMaterialCost(0, -1.0f);
    setMaterialCost(5, 1000.0f);
    setMaterialCost(7, 1000.0f);
}

// ELF 0x322D60 (D2; D1 0x322DC0). Deletes each owned material-cost record.
MaterialManager::~MaterialManager()
{
    for (std::vector<MaterialContainer*>::iterator i = m_materials.begin();
         i != m_materials.end(); ++i)
        delete *i;
}

// A material is walkable when its cost compares >= 0; material zero starts at -1.
// All accessors require a valid material index.
// ELF 0x322E60.
bool MaterialManager::isValidMaterial(unsigned char material) const
{
    return m_materials[material]->m_cost >= 0.0f;
}

// ELF 0x322E20.
void MaterialManager::setMaterialCost(unsigned char material, float cost)
{
    m_materials[material]->m_cost = cost;
}

// ELF 0x322E40.
float MaterialManager::getMaterialCost(unsigned char material) const
{
    return m_materials[material]->m_cost;
}

} // namespace ai
} // namespace hfe
} // namespace dice
