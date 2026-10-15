// Path [INFERRED]. Full instruction reads839B60..83A1DF, table94EC38.
#include "IStream.h"
#if !DEDICATED
#include "System.h"
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] Factored identical resource-name read sequences839E00/839EB0/839F60.
bool readResourceName(IStream* input, char* name)
{
    unsigned char length;
    if (!input->getSource()->streamUByte(&length, "Ubyte")) return false;
    if (length && input->getSource()->read(name, length) != length) return false;
    name[length] = 0;
    return true;
}
}
IStream::IStream(IStream* source) : m_source(source ? source : this) {}
IStream::~IStream() {} // 794CA0, borrowed source is not deleted
unsigned int IStream::getObjectSize() { return sizeof(IStream); }
IStream* IStream::getSource() // 839B60; repeated forwarding, no cycle/null guard
{
    IStream* source = this;
    while (source->m_source != source) source = source->m_source;
    return source;
}
bool IStream::streamUByte(unsigned char* value, const char*) { return getSource()->read(value, 1) == 1; }
bool IStream::streamByte(char* value, const char*) { return getSource()->read(value, 1) == 1; }
bool IStream::streamUShort(unsigned short* value, const char*) { return getSource()->read(value, 2) == 2; }
bool IStream::streamShort(short* value, const char*) { return getSource()->read(value, 2) == 2; }
bool IStream::streamULong(unsigned long* value, const char*) { return getSource()->read(value, 4) == 4; }
bool IStream::streamLong(long* value, const char*) { return getSource()->read(value, 4) == 4; }
bool IStream::streamFloat(float* value, const char*) { return getSource()->read(value, 4) == 4; }
bool IStream::streamBool(bool* value, const char* label) // 839C10, clear before attempted read
{
    *value = false;
    unsigned char byte;
    if (!getSource()->streamUByte(&byte, label)) return false;
    if (byte) *value = true;
    return true;
}
bool IStream::streamInt(int* value, const char* label)
{
    long temporary;
    if (!getSource()->streamLong(&temporary, label)) return false;
    *value = temporary;
    return true;
}
bool IStream::streamString(char** value, const char*) // 839C90, same shipped count-comparison defect
{
    ::operator delete(*value); *value = nullptr;
    int length;
    if (!getSource()->streamInt(&length, "Int")) return false;
    if (length) {
        *value = static_cast<char*>(::operator new(static_cast<unsigned int>(length) + 1));
        if (static_cast<int>(getSource()->read(*value, length) == 0) == length) return false;
        (*value)[length] = 0;
    }
    return true;
}
bool IStream::streamWideString(wchar_t** value, const char*)
{
    ::operator delete(*value); *value = nullptr;
    int length;
    if (!getSource()->streamInt(&length, "Int")) return false;
    if (length) {
        *value = static_cast<wchar_t*>(::operator new((static_cast<unsigned int>(length) + 1) * 2));
        if (getSource()->read(*value, 2 * length) != 2 * length) return false;
        (*value)[length] = 0;
    }
    return true;
}
bool IStream::streamCharacter(wchar_t* value, const char* label) // 839DC0, wire value is32 bits
{
    int temporary;
    if (!getSource()->streamInt(&temporary, label)) return false;
    *value = static_cast<wchar_t>(temporary);
    return true;
}
bool IStream::streamTexture(System* system, TextureHandle* texture, const char*)
{
    *texture = 0;
    char name[256];
    if (!readResourceName(this, name)) return false;
    *texture = system->loadTexture(name, true);
    return true;
}
bool IStream::streamFont(System* system, FontHandle* font, const char*)
{
    *font = 0;
    char name[256];
    if (!readResourceName(this, name)) return false;
    *font = system->loadFont(name);
    return true;
}
bool IStream::streamSound(System* system, int* sound, const char*)
{
    *sound = 0;
    char name[256];
    if (!readResourceName(this, name)) return false;
    *sound = system->loadSound(name);
    return true;
}
bool IStream::streamCollection(System* system, ObjectCollection* collection, const char*)
{ return collection->read(system, getSource()); }
bool IStream::streamEnum(int* value, EnumNext, EnumName, const char* label)
{ return getSource()->streamInt(value, label); }
bool IStream::streamObject(System* system, Object*& value, const char*, TypeDescriptor*)
{ return readObject(system, value, getSource()); }
int IStream::getAvailableBytes() { return 0; } // 80D430, real base default
int IStream::skip(int) { return 0; }
int IStream::read(void*, int) { return 0; }
int IStream::getPosition() { return 0; }
bool IStream::setPosition(int) { return false; }
bool IStream::readShortString(System*, char* buffer, int capacity, const char*)
{
    unsigned char length;
    if (!getSource()->streamUByte(&length, "Ubyte")) return false;
    if (length) {
        if (length > capacity) return false;
        if (getSource()->read(buffer, length) != length) return false;
    }
    buffer[length] = 0; // equality is allowed above: caller needs space for the terminator
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(IStream) == 8, "BFMeme input stream layout");
#endif
} }
#endif
