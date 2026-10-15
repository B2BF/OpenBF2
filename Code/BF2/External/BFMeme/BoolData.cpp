// [INFERRED] path. Complete client primary94B0F8, metadata94B1DC and all
// differing bodies read before recovery. Unlike Node, Data's28 slots are
// complete; no partial-interface publication or callable frontier is needed.
#include "BoolData.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
#include <cstring>
namespace dice { namespace meme {

BoolData::BoolData() : Data(nullptr), m_value(false), m_field09(0) {}
BoolData::~BoolData() {}
TypeDescriptor* BoolData::getClassType()
{
    // Independent counterA39928; descriptorA3992C has the complete eight slots.
    static ClassTypeDescriptor<BoolData, Data> type("dice::meme::BoolData");
    return &type;
}
TypeDescriptor* BoolData::getType() { return getClassType(); }
unsigned int BoolData::getObjectSize() { return sizeof(BoolData); }
namespace {
// CRT96E0D8 ->8703A0 registers the same descriptor and stores it atA39940.
TypeDescriptor* const registeredBoolDataType = BoolData::getClassType();
}

bool BoolData::streamProperties(System* system, Stream* stream)
{
    if (!Data::streamProperties(system, stream)) return false;
    return stream->streamBool(&m_value, "Value <do not edit>") != false;
}
bool BoolData::setFloat(System*, float value)
{
    m_value = value == 1.0f;
    return true;
}
bool BoolData::setString(System*, const char* value)
{
    m_value = _stricmp(value, "true") == 0 || std::strcmp(value, "1") == 0;
    return true;
}
float BoolData::getFloat(System*)
{
    // Retail tests the stored byte for exactly1; text tests nonzero instead.
    return *reinterpret_cast<const unsigned char*>(&m_value) == 1 ? 1.0f : 0.0f;
}
int BoolData::getCharacter(System*, int character)
{
    const bool value = *reinterpret_cast<const unsigned char*>(&m_value) != 0;
    if (static_cast<unsigned int>(character) >= (value ? 4u : 5u)) return 0;
    return (value ? "true" : "false")[character];
}

#if defined(_M_IX86)
static_assert(sizeof(BoolData) == 12, "client BFMeme BoolData");
static_assert(sizeof(ClassTypeDescriptor<BoolData, Data>) == 16, "BoolData metadata");
#endif
} }
#endif
