// Path [INFERRED]. Full PE instructions 0x82BB60/0x82BAC0/0x82B9A0;
// default virtual table 0x92FF18 and scalar deleting destructor 0x7D1D70.
#include "TypeDescriptor.h"
#if !DEDICATED
#include <cstring>
namespace dice { namespace meme {
namespace { TypeDescriptor* firstType = nullptr; }

TypeDescriptor::TypeDescriptor(const char* name)
    : m_next(nullptr), m_name(name), m_shortNameOffset(0)
{
    const char* separator = std::strrchr(name, ':');
    if (separator) m_shortNameOffset = unsigned(separator - name + 1);
    TypeDescriptor* previous = nullptr;
    TypeDescriptor* current = firstType;
    while (current)
    {
        if (std::strcmp(getShortName(), current->getShortName()) < 0) break;
        previous = current;
        current = current->m_next;
    }
    if (previous)
    {
        m_next = current;
        previous->m_next = this;
    }
    else
    {
        m_next = firstType;
        firstType = this;
    }
}
// The retail destructor neither removes itself from the chain nor frees name.
TypeDescriptor::~TypeDescriptor() {}
TypeDescriptor* TypeDescriptor::getBaseType() { return nullptr; }
RuntimeObject* TypeDescriptor::create() { return nullptr; }
const char* TypeDescriptor::getDescription() { return nullptr; }
void TypeDescriptor::addInstance() {}
void TypeDescriptor::removeInstance() {}
int TypeDescriptor::getInstanceCount() { return 0; }
unsigned int TypeDescriptor::getInstanceSize() { return 0; }

TypeDescriptor* TypeDescriptor::find(const char* name)
{
    for (TypeDescriptor* type = firstType; type; type = type->m_next)
        if (!std::strcmp(type->m_name, name)) return type;
    for (TypeDescriptor* type = firstType; type; type = type->m_next)
        if (!std::strcmp(type->getShortName(), name)) return type;
    return nullptr;
}
bool TypeDescriptor::isDerivedFrom(TypeDescriptor* base)
{
    if (base)
    {
        TypeDescriptor* type = this;
        while (type != base)
        {
            if (!type->getBaseType()) return false;
            type = type->getBaseType(); // intentional repeated virtual access
        }
    }
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(TypeDescriptor) == 16, "BFMeme descriptor layout");
#endif
} }
#endif
