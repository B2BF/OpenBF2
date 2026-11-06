// Allocation pair shared by the engine and side DLLs. Header placement [INFERRED].
#ifndef CORE_DLLSTLALLOCATOR_H
#define CORE_DLLSTLALLOCATOR_H

namespace dice {
namespace hfe {

// Allocates uninitialized array storage; allocation failure follows operator new[].
void* HeapAlloc(unsigned long size);
// Releases storage from HeapAlloc; a null pointer has no effect.
void HeapFree(void* storage);

} // namespace hfe
} // namespace dice

#endif
