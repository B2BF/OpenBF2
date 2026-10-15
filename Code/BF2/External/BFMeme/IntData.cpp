// [INFERRED] path. Complete primary94AFC8, metadata94B0C0, constructor82D8C0,
// factory82DB60, eager870360 and every differing body read before recovery.
// The class is reachable from Bf2HudBuilder::setNodeLogicShowVariable, which
// pairs it with IntRefData under an EqualData.
#include "IntData.h"
#if !DEDICATED
#include "DataFormatting.h"
#include "Stream.h"
#include "TypeDescriptor.h"
#include <cstdlib>
#include <new>
namespace dice { namespace meme {

IntData::IntData(int value, const char* name)
    : Data(name), m_value(value), m_cachedValue(0), m_text(nullptr) {}
IntData::~IntData()
{
    ::operator delete(m_text);
    m_text = nullptr;
}
TypeDescriptor* IntData::getClassType()
{
    // Full eight-slot metadata, Data parent and independent counterA3990C.
    static ClassTypeDescriptor<IntData, Data> type("dice::meme::IntData");
    return &type;
}
TypeDescriptor* IntData::getType() { return getClassType(); }
unsigned int IntData::getObjectSize() { return sizeof(IntData); }
namespace {
// Eager870360 constructs the descriptor and storesA39910, exactly like
// FloatData's8703E0. Native static lifetime supplies the guard and teardown.
TypeDescriptor* const registeredIntDataType = IntData::getClassType();
}

bool IntData::streamProperties(System* system, Stream* stream)
{
    if (!Data::streamProperties(system, stream)) return false;
    return stream->streamInt(&m_value, "Value <do not edit>") != false;
}
bool IntData::setFloat(System*, float value)
{
    // Round half away from zero, then the ordinary truncating x86 cast
    // (fnstcw / or 0C00h / fistp / fldcw). A NaN takes the negative arm.
    // Text/cache refresh is deferred until character access.
    m_value = static_cast<int>(value < 0.0f ? value - 0.5f : value + 0.5f);
    return true;
}
bool IntData::setString(System* system, const char* value)
{
    // No null guard or validation; preserve prefix parsing and virtual result.
    return setFloat(system, static_cast<float>(std::atof(value)));
}
float IntData::getFloat(System*) { return static_cast<float>(m_value); }
int IntData::getCharacter(System*, int character)
{
    if (!m_text) {
        // Retail reserves only twelve bytes, without bounding formatter output.
        m_text = static_cast<char*>(::operator new(12));
        formatStringDataInt(m_value, m_text);
        m_cachedValue = m_value;
    } else if (m_value != m_cachedValue) {
        m_cachedValue = m_value;
        formatStringDataInt(m_value, m_text);
    }
    // The index is unchecked and the returned narrow byte is sign-extended.
    return static_cast<signed char>(m_text[character]);
}

#if defined(_M_IX86)
static_assert(sizeof(IntData) == 20, "client BFMeme IntData");
static_assert(sizeof(ClassTypeDescriptor<IntData, Data>) == 16, "IntData metadata");
#endif
} }
#endif
