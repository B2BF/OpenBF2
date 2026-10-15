// [INFERRED] path beside the attested BFMeme cluster. Complete primary9328C0,
// metadata94B438, named ctor794D70/factory82EA40 and differing bodies read.
#include "EqualData.h"
#if !DEDICATED
#include "TypeDescriptor.h"
namespace dice { namespace meme {

EqualData::EqualData(const char* name) : LogicalData(name) {} // Both operands null.
TypeDescriptor* EqualData::getClassType()
{
    // Complete eight-slot descriptor; independent counterA3999C.
    static ClassTypeDescriptor<EqualData, LogicalData> type("dice::meme::EqualData");
    return &type;
}
TypeDescriptor* EqualData::getType() { return getClassType(); }
namespace {
// CRT96E0EC ->8704F0 registers the same descriptor and storesA39A78.
// Native static lifetime supplies the getter guard and teardown.
TypeDescriptor* const registeredEqualDataType = EqualData::getClassType();
}

float EqualData::getFloat(System* system)
{
    if (!m_first || !m_second) return 0.0f;
    // Unlike And/OrData, retain this operand across the first callback.
    Data* const second = m_second;
    const float firstValue = m_first->getFloat(system);    // PE explicitly spills float32.
    return firstValue == second->getFloat(system) ? 1.0f : 0.0f;
}

#if defined(_M_IX86)
static_assert(sizeof(EqualData) == 16, "client BFMeme EqualData");
static_assert(sizeof(ClassTypeDescriptor<EqualData, LogicalData>) == 16, "EqualData metadata");
#endif
} }
#endif
