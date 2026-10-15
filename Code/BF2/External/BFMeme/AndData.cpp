// [INFERRED] path. Full primary9329B0 and metadata94B63C, named constructor
//794DD0/factory82EC70 and all differing bodies read before recovery.
#include "AndData.h"
#if !DEDICATED
#include "TypeDescriptor.h"
namespace dice { namespace meme {

AndData::AndData(const char* name) : LogicalData(name) {}   // 794DD0, both operands start null
TypeDescriptor* AndData::getClassType()
{
    // Full metadata, independent static counterA399B0; factory passes null name.
    static ClassTypeDescriptor<AndData, LogicalData> type("dice::meme::AndData");
    return &type;
}
TypeDescriptor* AndData::getType() { return getClassType(); }
namespace {
// CRT96E100 ->870680 registers the same descriptor and storesA39A80.
TypeDescriptor* const registeredAndDataType = AndData::getClassType();
}

float AndData::getFloat(System* system)
{
    if (!m_first || !m_second) return 0.0f; // Both guards precede the first sample.
    if (m_first->getFloat(system) == 0.0f) return 0.0f;
    // Reload the second operand after the callback, without another null guard.
    // Both nonzero and unordered samples satisfy the retail comparison.
    return m_second->getFloat(system) != 0.0f ? 1.0f : 0.0f;
}

#if defined(_M_IX86)
static_assert(sizeof(AndData) == 16, "client BFMeme AndData");
static_assert(sizeof(ClassTypeDescriptor<AndData, LogicalData>) == 16, "AndData metadata");
#endif
} }
#endif
