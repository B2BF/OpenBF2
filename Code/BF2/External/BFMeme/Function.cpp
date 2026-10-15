// Full17-slot table94B370, descriptor94B3D0, lifetime and clone737DC0 read.
#include "Function.h"
#if !DEDICATED
#include "System.h"
#include "TypeDescriptor.h"
#include <cstring>
namespace dice { namespace meme {
namespace {
char* cloneFunctionName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* result = static_cast<char*>(::operator new(bytes));
    std::memcpy(result, name, bytes);
    return result;
}
}
Function::Function(const char* name) : m_name(cloneFunctionName(name)) {}
Function::~Function() { ::operator delete(m_name); m_name = nullptr; }
TypeDescriptor* Function::getClassType()
{
    static ClassTypeDescriptor<Function, Object> type("dice::meme::Function");
    return &type;
}
TypeDescriptor* Function::getType() { return getClassType(); }
unsigned int Function::getObjectSize() { return sizeof(Function); }
const char* Function::getName() { return m_name; }
bool Function::setName(const char* name)
{
    ::operator delete(m_name);
    m_name = nullptr;
    m_name = cloneFunctionName(name); // Retail frees before cloning; self-aliasing unsafe.
    return true;
}
bool Function::call(System* system)
{
    if (m_name) systemPrintf(system, "Meme: FunctionCall: %s\r\n", m_name);
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(Function) == 8, "BFMeme Function layout");
#endif
} }
#endif
