// [CHOSEN, not recovered] Native saved-table dispatch for the macOS port.
#ifndef BFMEME_NATIVE_SAVED_DISPATCH_H
#define BFMEME_NATIVE_SAVED_DISPATCH_H
#if defined(__APPLE__) && defined(__LP64__) && !defined(__arm64e__)
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>

namespace dice { namespace meme { namespace native_dispatch {

/// Itanium virtual-member encoding; a non-virtual member is a programming error and asserts. x86-64 marks a virtual
/// member by ptr = vtable byte offset + 1; the ARM variant of the C++ ABI (Apple arm64) keeps
/// ptr = vtable byte offset and marks it in adj = 2 * this-adjustment + 1.
struct VirtualMember
{
    std::ptrdiff_t offset;
    std::ptrdiff_t adjustment;

    template<class Member>
    static VirtualMember decode(Member member)
    {
        static_assert(std::is_member_function_pointer<Member>::value, "virtual member required");
        struct Representation { std::ptrdiff_t pointer; std::ptrdiff_t adjustment; };
        static_assert(sizeof(Member) == sizeof(Representation), "standard Itanium member encoding required");
        static_assert(sizeof(void (*)()) == sizeof(std::ptrdiff_t), "native function pointer width required");
        Representation bits;
        std::memcpy(&bits, &member, sizeof(bits));
#if defined(__aarch64__) || defined(__arm64__)
        const bool supported = (bits.adjustment & 1) && bits.pointer >= 0 &&
                               bits.pointer % sizeof(void (*)()) == 0;
        const VirtualMember decoded = {bits.pointer, bits.adjustment >> 1};
#else
        const bool supported = bits.pointer > 0 && (bits.pointer & 1) &&
                               (bits.pointer - 1) % sizeof(void (*)()) == 0;
        const VirtualMember decoded = {bits.pointer - 1, bits.adjustment};
#endif
        if (!supported)
        {
            // An invariant of this platform layer, not missing retail code: every caller passes a
            // virtual member, so only a programming error (null or non-virtual member) lands here.
            // bf2_meme_dispatch_check's death mode pins it.
            std::fprintf(stderr, "[ASSERT] dice::meme::native_dispatch::VirtualMember::decode: not a virtual member\n");
            std::fflush(stderr);
            std::abort();
        }
        return decoded;
    }

    std::size_t slot() const { return static_cast<std::size_t>(offset) / sizeof(void (*)()); }
};

template<class Member> class SavedVirtual;

/// Captures the adjusted receiver's table, then calls it on a later compatible receiver.
/// Both receivers must have live, layout-compatible subobjects for the selected method.
/// The stored table and its adjustment thunks must remain alive through the call.
template<class R, class C, class... Args>
class SavedVirtual<R (C::*)(Args...)>
{
public:
    using Member = R (C::*)(Args...);

    SavedVirtual(C* receiver, Member member) : m_member(VirtualMember::decode(member))
    {
        const char* adjusted = reinterpret_cast<const char*>(receiver) + m_member.adjustment;
        std::memcpy(&m_table, adjusted, sizeof(m_table));
    }

    R operator()(C* receiver, Args... args) const
    {
        // Keep the captured address point (including secondary-base thunks). Only
        // this is reloaded/adjusted; reading receiver's current vptr would change
        // the recovered capture-before-source-getter semantics. See the port guide.
        using Function = R (*)(void*, Args...);
        static_assert(sizeof(Function) == sizeof(void*), "native table entry width required");
        Function function;
        std::memcpy(&function, m_table + m_member.offset, sizeof(function));
        void* adjusted = reinterpret_cast<char*>(receiver) + m_member.adjustment;
        return function(adjusted, std::forward<Args>(args)...);
    }

private:
    VirtualMember m_member;
    const char* m_table;
};

} } }
#endif
#endif
