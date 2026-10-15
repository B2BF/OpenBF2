// Path [INFERRED]. Full RuntimeObject/Object tables 0x9327A0/0x932B18,
// descriptor factories 0x82BC10/0x82ACC0, and lifetime helpers read in full.
#include "Object.h"
#if !DEDICATED
#include "TypeDescriptor.h"
namespace dice { namespace meme {
namespace {
class RuntimeTypeDescriptor : public TypeDescriptor
{
public:
    RuntimeTypeDescriptor() : TypeDescriptor("RuntimeObject") {}
    RuntimeObject* create() override // 82BC10 -> ordinary operator new via737D60
    {
        void* allocation = ::operator new(sizeof(RuntimeObject));
        return allocation ? new(allocation) RuntimeObject : nullptr;
    }
};
// PE 0x86FEC0 creates this descriptor at image initialization, not first lookup.
RuntimeTypeDescriptor runtimeType;
}

RuntimeObject::~RuntimeObject() {}
TypeDescriptor* RuntimeObject::getType() { return &runtimeType; }
TypeDescriptor* RuntimeObject::getClassType() { return &runtimeType; }
unsigned int RuntimeObject::getObjectSize() { return sizeof(RuntimeObject); }
void RuntimeObject::destroy() { if (this) delete this; }
bool RuntimeObject::createRuntime() { return true; }    // 0x5A5E20, real hook
bool RuntimeObject::destroyRuntime() { return true; }   // 0x5A5E20, real hook

Object::~Object() {}                                  // 0x835660 -> base destructor
TypeDescriptor* Object::getClassType()
{
    static ClassTypeDescriptor<Object, RuntimeObject> type("dice::meme::Object");
    return &type;
}
TypeDescriptor* Object::getType() { return getClassType(); }
unsigned int Object::getObjectSize() { return sizeof(Object); }
const char* Object::getName() { return nullptr; }
bool Object::setName(const char*) { return false; }
bool Object::slot_0x20() { return false; }
bool Object::slot_0x24() { return false; }
bool Object::onCreate(System*) { return true; }
bool Object::streamProperties(System*, Stream*) { return true; }
bool Object::visit0x2c(System* system, Stream* stream) { return streamProperties(system, stream); }
bool Object::visit0x34(System* system, Stream* stream) { return streamProperties(system, stream); }
bool Object::visit0x38(System* system, Stream* stream) { return streamProperties(system, stream); }
bool Object::visit0x3c(System* system, Stream* stream) { return streamProperties(system, stream); }

bool destroyRuntimeObject(RuntimeObject*& object)
{
    if (object)
    {
        if (!object->destroyRuntime())
        {
            object = nullptr;
            return false;
        }
        object->getType()->removeInstance();
        object->destroy();
        object = nullptr;
    }
    return true;
}
bool createRuntimeObject(RuntimeObject*& object, TypeDescriptor* type)
{
    if (object)
    {
        if (object->destroyRuntime())
        {
            object->getType()->removeInstance();
            object->destroy();
        }
        object = nullptr;
    }
    if (!type) return false;
    RuntimeObject* created = type->create();
    if (!created) return false;
    if (!created->createRuntime())
    {
        destroyRuntimeObject(created);
        return false;
    }
    object = created;
    created->getType()->addInstance();
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(RuntimeObject) == 4, "BFMeme RuntimeObject layout");
static_assert(sizeof(Object) == 4, "BFMeme Object layout");
#endif
} }
#endif
