// [INFERRED] path beside the attested BFMeme cluster. Complete primary932938,
// metadata94B61C, named ctor794DA0/factory82EC00 and differing bodies read.
#include "OrData.h"
#if !DEDICATED
#include "TypeDescriptor.h"
namespace dice { namespace meme {

OrData::OrData(const char* name) : LogicalData(name) {}       // Both operands null.
TypeDescriptor* OrData::getClassType()
{
    // Complete eight-slot descriptor; independent counterA399AC.
    static ClassTypeDescriptor<OrData, LogicalData> type("dice::meme::OrData");
    return &type;
}
TypeDescriptor* OrData::getType() { return getClassType(); }
namespace {
// CRT96E0FC ->870630 registers the same descriptor and storesA39A84.
TypeDescriptor* const registeredOrDataType = OrData::getClassType();
}

float OrData::getFloat(System* system)
{
    // Both guards precede the first callback, even if it would return true.
    if (!m_first || !m_second) return 0.0f;
    if (m_first->getFloat(system) != 0.0f) return 1.0f;
    // Reload after the first callback without a second null guard.
    // Nonzero and unordered values both satisfy the retail comparisons.
    return m_second->getFloat(system) != 0.0f ? 1.0f : 0.0f;
}

#if defined(_M_IX86)
static_assert(sizeof(OrData) == 16, "client BFMeme OrData");
static_assert(sizeof(ClassTypeDescriptor<OrData, LogicalData>) == 16, "OrData metadata");
#endif
} }
#endif
