// [INFERRED] path. Full primary94E038, metadata94E0B4 and every differing
// body/factory/eager instruction read.
#include "ToggleData.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
namespace {
// Metadata835D00 has a real description; the generic null description is wrong
// for this class. Local descriptor spelling [INFERRED], full eight-slot closure.
class ToggleDataDescriptor : public ClassTypeDescriptor<ToggleData, Data>
{
public:
    ToggleDataDescriptor() : ClassTypeDescriptor("dice::meme::ToggleData") {}
    const char* getDescription() override
    { return "If 'Toggle data' is 0 'Data 1' will be returned, otherwise 'Data 2'"; }
};
}
ToggleData::ToggleData() : Data(nullptr), m_toggle(nullptr), m_first(nullptr),
    m_second(nullptr) {}                                // 835CD1..CEE; inherited dtor835D40
TypeDescriptor* ToggleData::getClassType()
{
    static ToggleDataDescriptor type;
    return &type;
}
TypeDescriptor* ToggleData::getType() { return getClassType(); }
unsigned int ToggleData::getObjectSize() { return sizeof(ToggleData); }
namespace {
// CRT96E1E0 ->871690 eagerly constructs descriptorA3A064, guardA3A074,
// and storesA3A078. Native static lifetime shares the getter's descriptor.
TypeDescriptor* const registeredToggleDataType = ToggleData::getClassType();
}
bool ToggleData::streamProperties(System* system, Stream* stream)
{
    if (!Data::streamProperties(system, stream)) return false;
    if (!stream->streamData(system, m_toggle, "Toggle data", nullptr)) return false;
    if (!stream->streamData(system, m_first, "Data 1", nullptr)) return false;
    return stream->streamData(system, m_second, "Data 2", nullptr) != false;
}
bool ToggleData::setString(System* system, const char* value)
{
    // The selected reference is loaded after the selector callback. Missing,
    // nonzero and unordered selectors choose Data 2; neither branch falls back.
    Data* selected = m_toggle && m_toggle->getFloat(system) == 0.0f ? m_first : m_second;
    return selected ? selected->setString(system, value) : false;
}
bool ToggleData::setFloat(System* system, float value)
{
    Data* selected = m_toggle && m_toggle->getFloat(system) == 0.0f ? m_first : m_second;
    return selected ? selected->setFloat(system, value) : false;
}
float ToggleData::getFloat(System* system)
{
    Data* selected = m_toggle && m_toggle->getFloat(system) == 0.0f ? m_first : m_second;
    if (!selected) return 0.0f;
    return selected->getFloat(system);
}
int ToggleData::getCharacter(System* system, int character)
{
    Data* selected = m_toggle && m_toggle->getFloat(system) == 0.0f ? m_first : m_second;
    return selected ? selected->getCharacter(system, character) : 0;
}
#if defined(_M_IX86)
static_assert(sizeof(ToggleData) == 20, "client ToggleData");
static_assert(sizeof(ToggleDataDescriptor) == 16, "client ToggleData metadata");
#endif
} }
#endif
