// Path [INFERRED]. Full instructions837D10..837FC6, DeletionStream83BC90..
// 83BD7C, and lifecycle82AD20/82AE60 read from BF2.exe.
#include "Stream.h"
#if !DEDICATED
#include "Data.h"
#include "Style.h"
#include "Event.h"
#include "Action.h"
#include "Function.h"
namespace dice { namespace meme {
TypeDescriptor* getEventClassType() { return Event::getClassType(); }
TypeDescriptor* getActionClassType() { return Action::getClassType(); }
TypeDescriptor* getFunctionClassType() { return Function::getClassType(); }
bool Stream::isPassive() { return false; }
bool Stream::streamUByte(unsigned char*, const char*) { return isPassive(); }
bool Stream::streamByte(char*, const char*) { return isPassive(); }
bool Stream::streamUShort(unsigned short*, const char*) { return isPassive(); }
bool Stream::streamShort(short*, const char*) { return isPassive(); }
bool Stream::streamULong(unsigned long*, const char*) { return isPassive(); }
bool Stream::streamLong(long*, const char*) { return isPassive(); }
bool Stream::streamFloat(float*, const char*) { return isPassive(); }
bool Stream::streamBool(bool*, const char*) { return isPassive(); }
bool Stream::streamInt(int*, const char*) { return isPassive(); }
bool Stream::streamString(char**, const char*) { return isPassive(); }
bool Stream::streamWideString(wchar_t**, const char*) { return isPassive(); }
bool Stream::streamCharacter(wchar_t*, const char*) { return isPassive(); }
bool Stream::streamTexture(System*, TextureHandle*, const char*) { return isPassive(); }
bool Stream::streamFont(System*, FontHandle*, const char*) { return isPassive(); }
bool Stream::streamSound(System*, int*, const char*) { return isPassive(); }
bool Stream::streamCollection(System*, ObjectCollection*, const char*) { return isPassive(); }
bool Stream::streamEnum(int*, EnumNext, EnumName, const char*) { return isPassive(); }
bool Stream::streamObject(System*, Object*&, const char*, TypeDescriptor*) { return isPassive(); }

bool Stream::streamEvent(System* system, Event*& value, const char* name, TypeDescriptor* type)
{ return streamObject(system, reinterpret_cast<Object*&>(value), name, type ? type : getEventClassType()); }
bool Stream::streamAction(System* system, Action*& value, const char* name, TypeDescriptor* type)
{ return streamObject(system, reinterpret_cast<Object*&>(value), name, type ? type : getActionClassType()); }
bool Stream::streamData(System* system, Data*& value, const char* name, TypeDescriptor* type)
{ return streamObject(system, reinterpret_cast<Object*&>(value), name, type ? type : Data::getClassType()); }
bool Stream::streamEffect(System* system, Effect*& value, const char* name, TypeDescriptor* type)
{ return streamObject(system, reinterpret_cast<Object*&>(value), name, type ? type : getEffectClassType()); }
bool Stream::streamFunction(System* system, Function*& value, const char* name, TypeDescriptor* type)
{ return streamObject(system, reinterpret_cast<Object*&>(value), name, type ? type : getFunctionClassType()); }
bool Stream::streamNode(System* system, Node*& value, const char* name, TypeDescriptor* type)
{ return streamObject(system, reinterpret_cast<Object*&>(value), name, type ? type : getNodeClassType()); }
bool Stream::streamStyle(System* system, Style*& value, const char* name, TypeDescriptor* type)
{ return streamObject(system, reinterpret_cast<Object*&>(value), name, type ? type : Style::getClassType()); }
bool Stream::streamTree(System* system, Tree*& value, const char* name, TypeDescriptor* type)
{ return streamObject(system, reinterpret_cast<Object*&>(value), name, type ? type : getTreeClassType()); }
// Source identities of the two Node wrapper flavors remain inferred. Both
// are the same837FC0 tail dispatch, preserving the supplied type argument.
bool Stream::streamNode0x84(System* system, Node*& value, const char* name, TypeDescriptor* type)
{ return streamNode(system, value, name, type); }
bool Stream::streamNode0x88(System* system, Node*& value, const char* name, TypeDescriptor* type)
{ return streamNode(system, value, name, type); }

bool DeletionStream::isPassive() { return true; }
bool DeletionStream::streamCollection(System* system, ObjectCollection* collection, const char*)
{ return collection->clear(system); } // 83BC90, no null check
bool DeletionStream::streamObject(System* system, Object*& value, const char*, TypeDescriptor*)
{
    if (!value) return true;
    if (m_preserveNamed) {
        if (value->getName()) {
            Object* saved = value;
            value = nullptr;
            if (!saved->visit0x2c(system, this)) return false;
            value = saved;
            return true;
        }
        Object* saved = value;
        value = nullptr;
        if (!saved->visit0x2c(system, this)) return false;
        return destroyObject(saved);
    }
    Object* saved = value;
    value = nullptr;
    if (saved->getName()) return true; // named object is borrowed; keep it alive
    if (!saved->visit0x2c(system, this)) return false;
    return destroyObject(saved);
}
bool DeletionStream::destroyObject(Object*& value)
{ return destroyRuntimeObject(reinterpret_cast<RuntimeObject*&>(value)); }
bool deleteObject(System* system, Object*& value)
{
    DeletionStream deletion(false);
    return deletion.streamObject(system, value, "Object", nullptr);
}
bool createObject(System* system, Object*& value, TypeDescriptor* type)
{
    if (!createRuntimeObject(reinterpret_cast<RuntimeObject*&>(value), type)) return false;
    if (value->onCreate(system)) return true;
    DeletionStream deletion(false);
    deletion.streamObject(system, value, "Object", nullptr); // ignored result
    return false;
}
#if defined(_M_IX86)
static_assert(sizeof(Stream) == 4, "BFMeme stream prefix");
static_assert(sizeof(DeletionStream) == 8, "BFMeme deletion visitor");
#endif
} }
#endif
