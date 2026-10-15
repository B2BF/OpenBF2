// [INFERRED] path. Complete primary932848 and metadata94B418, factory82E9D0,
// eager8704B0 and every target read. This is a full Data family, with no
// partial-interface exception or new frontier.
#include "LogicalData.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {

LogicalData::LogicalData(const char* name) : Data(name), m_first(nullptr),
    m_second(nullptr) {}                                 // 82E9D0, inlined794DD0/794E00
LogicalData::~LogicalData() {}                            // 835D40; only Data owns its name
TypeDescriptor* LogicalData::getClassType()
{
    // Complete eight-slot metadata; independent static counterA39998.
    static ClassTypeDescriptor<LogicalData, Data> type("dice::meme::LogicalData");
    return &type;
}
TypeDescriptor* LogicalData::getType() { return getClassType(); }
unsigned int LogicalData::getObjectSize() { return sizeof(LogicalData); }
namespace {
// CRT96E0E8 ->8704B0 registers the same descriptor and storesA39A88.
// Native static lifetime owns the getter guard and teardown; spelling [INFERRED].
TypeDescriptor* const registeredLogicalDataType = LogicalData::getClassType();
}

// 794D50, emitted in the Bf2HudBuilder translation unit's comdat group with
// the derived constructors; a straight pair of stores with no null handling.
void LogicalData::setData(Data* first, Data* second)
{
    m_first = first;
    m_second = second;
}

bool LogicalData::streamProperties(System* system, Stream* stream)
{
    if (!Data::streamProperties(system, stream)) return false;
    if (!stream->streamData(system, m_first, "Data 1", nullptr)) return false;
    return stream->streamData(system, m_second, "Data 2", nullptr) != false;
}

int LogicalData::getCharacter(System* system, int character)
{
    // The virtual sample occurs for every character, even an invalid index.
    // Zero selects false; nonzero and unordered samples select true.
    const bool value = getFloat(system) != 0.0f;
    if (static_cast<unsigned int>(character) >= (value ? 4u : 5u)) return 0;
    return (value ? "true" : "false")[character];
}

#if defined(_M_IX86)
static_assert(sizeof(LogicalData) == 16, "client BFMeme LogicalData");
static_assert(sizeof(ClassTypeDescriptor<LogicalData, Data>) == 16, "LogicalData metadata");
#endif
} }
#endif
