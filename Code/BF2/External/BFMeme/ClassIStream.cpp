// Complete41-slot tables94EF00/94F1F0 and all distinct bodies read in BF2.exe.
#include "ClassIStream.h"
#if !DEDICATED
#include "System.h"
#include "TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
ForwardingIStream::ForwardingIStream(IStream* input, bool ownsInput)
    : IStream(nullptr), m_input(input), m_ownsInput(ownsInput)
{
    if (m_input) m_input->m_source = this;
}
ForwardingIStream::~ForwardingIStream()
{
    if (m_input) {
        if (m_ownsInput) delete m_input;
        else m_input->m_source = m_source; // Actual upstream link, not the old child's link.
    }
}
bool ForwardingIStream::streamUByte(unsigned char* p, const char* label) { return m_input->streamUByte(p,label); }
bool ForwardingIStream::streamByte(char* p, const char* label) { return m_input->streamByte(p,label); }
bool ForwardingIStream::streamUShort(unsigned short* p, const char* label) { return m_input->streamUShort(p,label); }
bool ForwardingIStream::streamShort(short* p, const char* label) { return m_input->streamShort(p,label); }
bool ForwardingIStream::streamULong(unsigned long* p, const char* label) { return m_input->streamULong(p,label); }
bool ForwardingIStream::streamLong(long* p, const char* label) { return m_input->streamLong(p,label); }
bool ForwardingIStream::streamFloat(float* p, const char* label) { return m_input->streamFloat(p,label); }
bool ForwardingIStream::streamBool(bool* p, const char* label) { return m_input->streamBool(p,label); }
bool ForwardingIStream::streamInt(int* p, const char* label) { return m_input->streamInt(p,label); }
bool ForwardingIStream::streamString(char** p, const char* label) { return m_input->streamString(p,label); }
bool ForwardingIStream::streamWideString(wchar_t** p, const char* label) { return m_input->streamWideString(p,label); }
bool ForwardingIStream::streamCharacter(wchar_t* p, const char* label) { return m_input->streamCharacter(p,label); }
bool ForwardingIStream::streamTexture(System* s, TextureHandle* p, const char* label) { return m_input->streamTexture(s,p,label); }
bool ForwardingIStream::streamFont(System* s, FontHandle* p, const char* label) { return m_input->streamFont(s,p,label); }
bool ForwardingIStream::streamSound(System* s, int* p, const char* label) { return m_input->streamSound(s,p,label); }
bool ForwardingIStream::streamCollection(System* s, ObjectCollection* p, const char* label) { return m_input->streamCollection(s,p,label); }
bool ForwardingIStream::streamEnum(int* p, EnumNext next, EnumName name, const char* label) { return m_input->streamEnum(p,next,name,label); }
bool ForwardingIStream::streamObject(System* s, Object*& p, const char* label, TypeDescriptor* type) { return m_input->streamObject(s,p,label,type); }
int ForwardingIStream::getAvailableBytes() { return m_input->getAvailableBytes(); }
int ForwardingIStream::skip(int count) { return m_input->skip(count); }
int ForwardingIStream::read(void* p, int count) { return m_input->read(p,count); }
int ForwardingIStream::getPosition() { return m_input->getPosition(); }
bool ForwardingIStream::setPosition(int position) { return m_input->setPosition(position); }
bool ForwardingIStream::readShortString(System* s, char* p, int capacity, const char* label) { return m_input->readShortString(s,p,capacity,label); }
ClassIStream::ClassIStream(IStream* input) : ForwardingIStream(input,true), m_payloadStart(0)
{ m_strings[0] = nullptr; }
ClassIStream::~ClassIStream()
{
    char** entry = m_strings;
    while (*entry) { ::operator delete(*entry); *entry++ = nullptr; }
}
unsigned int ClassIStream::getObjectSize() { return sizeof(ClassIStream); }
bool ClassIStream::init(System* system)
{
    char version[256];
    if (!ForwardingIStream::readShortString(system,version,256,"Static string")) return false;
    if (std::strcmp(version,"MemeFile 2.0")) {
        systemPrintf(system,"Meme: Error: Wrong file version(%s)\r\n",version);
        return false;
    }
    char name[256];
    int count = 0;
    if (!ForwardingIStream::readShortString(system,name,256,"Static string")) return false;
    while (*name) {
        const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
        char* copy = static_cast<char*>(::operator new(bytes));
        std::memcpy(copy,name,bytes);
        m_strings[count++] = copy;
        if (!ForwardingIStream::readShortString(system,name,256,"Static string")) return false;
    }
    // Retail writes the sentinel only after the complete dictionary is read.
    // A failed later read can therefore leave uninitialized entries at teardown.
    m_strings[count] = nullptr;
    m_payloadStart = ForwardingIStream::getPosition();
    return true;
}
int ClassIStream::getPosition() { return m_input->getPosition() - m_payloadStart; }
bool ClassIStream::setPosition(int position) { return m_input->setPosition(m_payloadStart + position); }
bool ClassIStream::readShortString(System*, char* buffer, int, const char*)
{
    buffer[0] = 0; // Cleared even if the following ushort read fails.
    unsigned short id;
    if (!ForwardingIStream::streamUShort(&id,"Ushort")) return false;
    if (id) std::strcpy(buffer,m_strings[id - 1]); // Capacity and index are not checked.
    return true;
}
bool readNamedObject(System* system, Object*& value, IStream* input, const char* path)
{
    char className[64];
    if (!input->readShortString(system,className,64,"Class name")) {
        systemPrintf(system,"Meme: Error: Reading from istream \"%s\"\r\n",path);
        return false;
    }
    TypeDescriptor* type = TypeDescriptor::find(className);
    if (!type) {
        systemPrintf(system,"Meme: Error: Class '%s' not found\r\n",className);
        return false;
    }
    if (!type->isDerivedFrom(Object::getClassType())) {
        // The PE passes the path to this class-name diagnostic.
        systemPrintf(system,"Meme: Error: Loading class '%s' need to be inherited from object\r\n",path);
        return false;
    }
    systemPrintf(system,"Meme: Loading: %s\r\n",path);
    value = static_cast<Object*>(type->create()); // No allocation-result or onCreate call.
    value->getType()->addInstance();
    system->assignObjectPath(value,path);
    if (!value->visit0x34(system,input)) {
        systemPrintf(system,"Meme: Error: Reading from istream \"%s(%s)\"\r\n",className,path);
        return false;
    }
    return true;
}
bool loadNamedObject(System* system, Object*& value, const char* path)
{
    if (!path) return false;
    DeletionStream deletion(false);
    deletion.streamObject(system,value,"Object",nullptr);
    value = system->findNamedObject(path);
    if (value) return true;
    IStream* input = system->openNamedInputStream(path); // Actual file/cache decorator selection.
    if (!input) {
        systemPrintf(system,"Meme: Error: Unable to open istream \"%s\"\r\n",path);
        return false;
    }
    {
        ClassIStream stream(input);
        if (stream.init(system)) return readNamedObject(system,value,&stream,path);
    }
    systemPrintf(system,"Meme: Info: Trying old format \"%s\"\r\n",path);
    input = system->openNamedInputStream(path);
    if (!input) {
        systemPrintf(system,"Meme: Error: Unable to open istream \"%s\"\r\n",path);
        return false;
    }
    const bool result = readNamedObject(system,value,input,path);
    system->deleteInputStream(input);
    return result;
}
bool readObject(System* system, Object*& value, IStream* input)
{
    DeletionStream deletion(false);
    deletion.streamObject(system,value,"Object",nullptr);
    const int start = input->getPosition();
    unsigned long size;
    if (!input->streamULong(&size,"Ulong")) return false;
    char objectName[128], className[64];
    if (!input->readShortString(system,objectName,128,"Object name")) return false;
    if (!input->readShortString(system,className,64,"Class name")) return false;
    const unsigned long end = static_cast<unsigned long>(start) + size;
    if (*objectName) {
        value = system->findNamedObject(objectName);
        if (!value) {
            if (*className) {
                TypeDescriptor* type = TypeDescriptor::find(className);
                if (type && type->isDerivedFrom(Object::getClassType())) {
                    value = static_cast<Object*>(type->create());
                    system->assignObjectPath(value,objectName);
                    if (!value->visit0x34(system,input))
                        systemPrintf(system,"Meme: Error reading from istream \"%s\"\r\n",className);
                    // Named objects are counted even when their property visitor fails.
                    value->getType()->addInstance();
                } else {
                    systemPrintf(system,"Meme: Error: Loading: Unknown class: %s\r\n",className);
                }
                if (static_cast<unsigned long>(input->getPosition()) != end)
                    systemPrintf(system,"Meme: Warning: reading differs from saved file \"%s\"\r\n",className);
            } else {
                loadNamedObject(system,value,objectName); // Failure is ignored in the PE.
            }
        }
    } else {
        TypeDescriptor* type = TypeDescriptor::find(className); // Also called for empty text.
        if (*className && !type) {
            systemPrintf(system,"Meme: Error: Loading: Unknown class: %s\r\n",className);
        } else if (type && type->isDerivedFrom(Object::getClassType())) {
            value = static_cast<Object*>(type->create());
            if (!value->visit0x34(system,input)) {
                systemPrintf(system,"Meme: Error: Reading from istream \"%s\"\r\n",className);
                return false; // Anonymous failure neither counts the object nor skips the frame.
            }
            value->getType()->addInstance();
            if (static_cast<unsigned long>(input->getPosition()) != end)
                systemPrintf(system,"Meme: Warning: Reading differs from saved file \"%s\"\r\n",className);
        }
    }
    input->skip(static_cast<int>(size - static_cast<unsigned long>(input->getPosition())
        + static_cast<unsigned long>(start))); // Return ignored; signed backward skip is possible.
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(ForwardingIStream) == 16, "forwarding input layout");
static_assert(sizeof(ClassIStream) == 0x814, "class input dictionary layout");
#endif
} }
#endif
