// [INFERRED] path. Full primary932A28 and metadata94B6F4, named constructor
//794E00/factory82ED50 and all differing bodies read before recovery.
#include "NotData.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {

NotData::NotData(const char* name) : LogicalData(name) {}   // 794E00, both operands start null
TypeDescriptor* NotData::getClassType()
{
    // Full metadata, independent static counterA399B8; factory passes null name.
    static ClassTypeDescriptor<NotData, LogicalData> type("dice::meme::NotData");
    return &type;
}
TypeDescriptor* NotData::getType() { return getClassType(); }
namespace {
// CRT96E108 ->870720 registers the same descriptor and storesA39A7C.
TypeDescriptor* const registeredNotDataType = NotData::getClassType();
}

bool NotData::streamProperties(System* system, Stream* stream)
{
    // The actual base call is Data, not the two-operand LogicalData visitor.
    if (!Data::streamProperties(system, stream)) return false;
    return stream->streamData(system, m_first, "Data", nullptr) != false;
}
float NotData::getFloat(System* system)
{
    if (!m_first) return 0.0f;
    return m_first->getFloat(system) == 0.0f ? 1.0f : 0.0f;
}

#if defined(_M_IX86)
static_assert(sizeof(NotData) == 16, "client BFMeme NotData");
static_assert(sizeof(ClassTypeDescriptor<NotData, LogicalData>) == 16, "NotData metadata");
#endif
} }
#endif
