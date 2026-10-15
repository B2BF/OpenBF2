// Complete17-slot table94EEB8 and eight-slot descriptor94EE84 read in BF2.exe.
#include "Event.h"
#if !DEDICATED
#include "Node.h"
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
Event::Event() : m_inputIndex(0) {} // Factory writes only this byte; padding untouched.
TypeDescriptor* Event::getClassType()
{
    static ClassTypeDescriptor<Event, Object> type("dice::meme::Event");
    return &type;
}
TypeDescriptor* Event::getType() { return getClassType(); }
unsigned int Event::getObjectSize() { return sizeof(Event); }
bool Event::streamProperties(System* system, Stream* stream)
{
    return Object::streamProperties(system, stream)
        && stream->streamUByte(&m_inputIndex, "Input index (0 == all indexes)");
}
bool Event::matches(System*, const NodeInputEvent* input) // 83AED0, null input not guarded.
{ return m_inputIndex == 0 || input->index == m_inputIndex; }
#if defined(_M_IX86)
static_assert(sizeof(Event) == 8, "BFMeme Event layout");
#endif
} }
#endif
