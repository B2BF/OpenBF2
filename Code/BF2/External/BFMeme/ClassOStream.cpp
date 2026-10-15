// All distinct table bodies and helpers read from the client PE before writing.
// Source names [INFERRED]; no stream descriptor/factory is published here.
#include "ClassOStream.h"
#if !DEDICATED
#include "System.h"
#include "NamedObjectRegistry.h"
#include "TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
ForwardingOStream::~ForwardingOStream() { delete m_destination; }
bool ForwardingOStream::streamUByte(unsigned char* p, const char* label) { return m_destination->streamUByte(p,label); }
bool ForwardingOStream::streamByte(char* p, const char* label) { return m_destination->streamByte(p,label); }
bool ForwardingOStream::streamUShort(unsigned short* p, const char* label) { return m_destination->streamUShort(p,label); }
bool ForwardingOStream::streamShort(short* p, const char* label) { return m_destination->streamShort(p,label); }
bool ForwardingOStream::streamULong(unsigned long* p, const char* label) { return m_destination->streamULong(p,label); }
bool ForwardingOStream::streamLong(long* p, const char* label) { return m_destination->streamLong(p,label); }
bool ForwardingOStream::streamFloat(float* p, const char* label) { return m_destination->streamFloat(p,label); }
bool ForwardingOStream::streamBool(bool* p, const char* label) { return m_destination->streamBool(p,label); }
bool ForwardingOStream::streamInt(int* p, const char* label) { return m_destination->streamInt(p,label); }
bool ForwardingOStream::streamString(char** p, const char* label) { return m_destination->streamString(p,label); }
bool ForwardingOStream::streamWideString(wchar_t** p, const char* label) { return m_destination->streamWideString(p,label); }
bool ForwardingOStream::streamCharacter(wchar_t* p, const char* label) { return m_destination->streamCharacter(p,label); }
bool ForwardingOStream::streamTexture(System* s, TextureHandle* p, const char* label) { return m_destination->streamTexture(s,p,label); }
bool ForwardingOStream::streamFont(System* s, FontHandle* p, const char* label) { return m_destination->streamFont(s,p,label); }
bool ForwardingOStream::streamSound(System* s, int* p, const char* label) { return m_destination->streamSound(s,p,label); }
bool ForwardingOStream::streamEnum(int* p, EnumNext next, EnumName name, const char* label) { return m_destination->streamEnum(p,next,name,label); }
int ForwardingOStream::write(const void* p, int count) { return m_destination->write(p,count); }
void ForwardingOStream::flush() { m_destination->flush(); }
int ForwardingOStream::getPosition() { return m_destination->getPosition(); }
bool ForwardingOStream::setPosition(int position) { return m_destination->setPosition(position); }
bool ForwardingOStream::writeShortString(System* s, const char* p, const char* label) { return m_destination->writeShortString(s,p,label); }

MemoryOStream::MemoryOStream(int capacity) : m_capacity(capacity), m_position(0)
{ m_bytes = static_cast<char*>(::operator new(static_cast<unsigned int>(capacity))); }
// 402D10 and737D70 reach the same operator-delete/bf_free thunk.
MemoryOStream::~MemoryOStream() { ::operator delete(m_bytes); }
int MemoryOStream::write(const void* bytes, int count) // 83D230, signed comparison and partial write
{
    if (static_cast<int>(static_cast<unsigned int>(m_position) + static_cast<unsigned int>(count)) >= m_capacity)
        count = m_capacity - m_position;
    if (count) std::memcpy(m_bytes + m_position, bytes, count);
    m_position += count;
    return count;
}
int MemoryOStream::getPosition() { return m_position; }
bool MemoryOStream::setPosition(int position)
{
    if (position < 0 || position >= m_capacity) return false;
    m_position = position;
    return true;
}
ClassOStream::ClassOStream(OStream* output)
    : ForwardingOStream(output), m_output(output), m_buffer(0x80000)
{
    m_destination = &m_buffer;
    m_strings[0] = nullptr; // Remaining dictionary slots deliberately untouched.
}
ClassOStream::~ClassOStream()
{
    char** entry = m_strings;
    while (*entry) { ::operator delete(*entry); *entry++ = nullptr; }
    m_destination = m_output; // Buffer destructor precedes owning base destructor.
}
bool ClassOStream::writeShortString(System*, const char* value, const char*)
{
    unsigned short id = 0;
    int index = 0;
    if (value && *value) {
        while (m_strings[index]) {
            if (!std::strcmp(value, m_strings[index])) {
                id = static_cast<unsigned short>(index + 1);
                break;
            }
            ++index;
        }
        if (!id) {
            id = static_cast<unsigned short>(index + 1);
            const unsigned int bytes = static_cast<unsigned int>(std::strlen(value)) + 1;
            char* copy = static_cast<char*>(::operator new(bytes));
            std::memcpy(copy, value, bytes); // Clone helper has no allocation-result guard.
            m_strings[index] = copy;
            if (!copy) return false;
            m_strings[index + 1] = nullptr; // Retail has no dictionary capacity guard.
        }
    }
    return ForwardingOStream::streamUShort(&id, "Ushort");
}
bool ClassOStream::finish(System* system) // 83BE20, switches permanently to original output
{
    m_destination = m_output;
    if (!ForwardingOStream::writeShortString(system, "MemeFile 2.0", "Static string")) return false;
    for (char** entry = m_strings; *entry; ++entry)
        if (!ForwardingOStream::writeShortString(system, *entry, "Static string")) return false;
    if (!ForwardingOStream::writeShortString(system, nullptr, "Static string")) return false;
    const char* bytes = m_buffer.m_bytes;
    const int written = ForwardingOStream::write(bytes, m_buffer.getPosition());
    return written == m_buffer.getPosition(); // Repeated virtual query preserved.
}
bool saveNamedObject(System* system, Object*& value)
{
    if (!value || !value->slot_0x20() || !value->getName()) return true;
    char path[256];
    system->m_registry->objectPath(value, path, 256); // Return ignored; no registry guard.
    OStream* output = system->createOutputStream(path);
    if (!output) {
        systemPrintf(system, "Meme: Error: Creating ostream \"%s\"\r\n", path);
        return false;
    }
    ClassOStream stream(output);
    const char* className = value->getType()->getName();
    if (!stream.writeShortString(system, className, "Class name")
        || !value->visit0x38(system, &stream) || !stream.finish(system)) {
        systemPrintf(system, "Meme: Error: Writing to stream \"%s\"\r\n", path);
        return false;
    }
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(ForwardingOStream) == 8, "forwarding stream layout");
static_assert(sizeof(MemoryOStream) == 16, "memory stream layout");
static_assert(sizeof(ClassOStream) == 0x81C, "class output dictionary layout");
#endif
} }
#endif
