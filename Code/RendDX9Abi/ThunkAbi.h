// [CHOSEN, not recovered] Calling-convention spelling for hand-built vtable thunks (issue #7).
//
// Several recovered classes publish vtables of free functions instead of C++ virtual methods. On
// x86 a member function's __thiscall ABI (object in ECX, arguments on the stack, callee cleanup) is
// reproduced by a __fastcall free function whose unused second parameter occupies EDX. x64 has a
// single calling convention in which the object is simply the first argument, so the EDX slot must
// disappear there or every later argument would shift by one.
//
//   int RD9_THUNK getRef(void* self RD9_THUNK_EDX);                  // a thunk definition
//   int RD9_THUNK setValue(void* self RD9_THUNK_EDX, int value);
//   getRef(object RD9_THUNK_ARG);                                    // a direct call to one
//
// Code that calls a C++ virtual method or a thunk through a raw function pointer uses a
// `__thiscall` pointer type without the EDX slot, which is the same ABI on x86 and correct on x64.

#ifndef RENDDX9ABI_THUNKABI_H
#define RENDDX9ABI_THUNKABI_H

#if defined(_M_IX86)
#define RD9_THUNK       __fastcall
#define RD9_THUNK_EDX   , void*
#define RD9_THUNK_ARG   , 0
#else
#define RD9_THUNK
#define RD9_THUNK_EDX
#define RD9_THUNK_ARG
#endif

#endif // RENDDX9ABI_THUNKABI_H
