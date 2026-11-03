// DllStlAllocator.cpp is an original ELF FILE symbol. Function placement [INFERRED]
// from its position between DllModule.cpp and Random.cpp.
#include "DllStlAllocator.h"

namespace dice {
namespace hfe {

// ELF 0x48A7F0; engine 0x83C3420.
void* HeapAlloc(unsigned long size)
{
    return new char[size];
}

// ELF 0x48A820; engine 0x83C3430.
void HeapFree(void* storage)
{
    if (storage)
        delete[] static_cast<char*>(storage);
}

} // namespace hfe
} // namespace dice
