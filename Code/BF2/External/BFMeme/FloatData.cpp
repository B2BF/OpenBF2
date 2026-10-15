// [INFERRED] path. Complete primary94B218, metadata94B2F8, constructor82DFC0,
// factory82E200, eager8703E0 and every differing body read before recovery.
#include "FloatData.h"
#if !DEDICATED
#include "DataFormatting.h"
#include "Stream.h"
#include "TypeDescriptor.h"
#include <cstdlib>
#include <new>
namespace dice { namespace meme {

FloatData::FloatData(float value, const char* name)
    : Data(name), m_value(value), m_cachedValue(0.0f), m_text(nullptr) {}
FloatData::~FloatData()
{
    ::operator delete(m_text);
    m_text = nullptr;
}
TypeDescriptor* FloatData::getClassType()
{
    // Full eight-slot metadata, Data parent and independent counterA39944.
    static ClassTypeDescriptor<FloatData, Data> type("dice::meme::FloatData");
    return &type;
}
TypeDescriptor* FloatData::getType() { return getClassType(); }
unsigned int FloatData::getObjectSize() { return sizeof(FloatData); }
namespace {
// Registers the descriptor eagerly while native static lifetime provides shared initialization and teardown.
TypeDescriptor* const registeredFloatDataType = FloatData::getClassType();
}

bool FloatData::streamProperties(System* system, Stream* stream)
{
    if (!Data::streamProperties(system, stream)) return false;
    return stream->streamFloat(&m_value, "Value <do not edit>") != false;
}
bool FloatData::setFloat(System*, float value)
{
    m_value = value; // Text/cache refresh is deferred until character access.
    return true;
}
bool FloatData::setString(System* system, const char* value)
{
    // No null guard or validation; preserve prefix parsing and virtual result.
    return setFloat(system, static_cast<float>(std::atof(value)));
}
float FloatData::getFloat(System*) { return m_value; }
int FloatData::getCharacter(System*, int character)
{
    if (!m_text) {
        // Retail reserves only 18 bytes, without bounding formatter output.
        m_text = static_cast<char*>(::operator new(18));
        formatStringDataFloat(m_value, m_text, 9);
        m_cachedValue = m_value;
    } else if (m_value != m_cachedValue) {
        m_cachedValue = m_value;
        formatStringDataFloat(m_value, m_text, 9);
    }
    // Equality reuses the buffer (+0/-0 included); NaN forces reformatting.
    // The index is unchecked and the returned narrow byte is sign-extended.
    return static_cast<signed char>(m_text[character]);
}

#if defined(_M_IX86)
static_assert(sizeof(FloatData) == 20, "client BFMeme FloatData");
static_assert(sizeof(ClassTypeDescriptor<FloatData, Data>) == 16, "FloatData metadata");
#endif
} }
#endif
