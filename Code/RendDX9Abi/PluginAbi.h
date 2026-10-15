// [CHOSEN, not recovered] Rebuilt plugins share the host's current STL/CRT.
// Retail plugin loading is retired under https://github.com/B2BF/OpenBF2/issues/7: every image that
// includes this header (the plugin DLLs, the rebuilt client and the editor) is built with one MSVC
// dynamic CRT and one STL layout, so host and plugins pass std containers to each other directly.

#ifndef RENDDX9ABI_PLUGINABI_H
#define RENDDX9ABI_PLUGINABI_H

#include <string>

#include <list>
#include <string>
#include <vector>
#include "../BF2/Core/ClassManager.h"

#if !defined(_MSC_VER) || !defined(_MT) || !defined(_DLL)
#error The rebuilt plugin ABI requires the supported MSVC dynamic-CRT (/MD) build
#endif
#if !defined(_ITERATOR_DEBUG_LEVEL) || _ITERATOR_DEBUG_LEVEL != 0
#error The rebuilt plugin ABI requires _ITERATOR_DEBUG_LEVEL=0 in both host and plugin images
#endif

namespace dice { namespace hfe { class IBase; } }

namespace renddx9abi {

typedef std::string HostString;
typedef std::list<HostString> HostStringList;

template <typename T>
class HostVectorWriter
{
public:
    explicit HostVectorWriter(std::vector<T>& hostVector) : m_vector(hostVector) {}
    void push_back(const T& value) { m_vector.push_back(value); }
    // Returns the current registration count for accurate diagnostics.
    unsigned int size() const { return static_cast<unsigned int>(m_vector.size()); }

private:
    std::vector<T>& m_vector;
};

// Host-callable slots use HostString, converting once at this boundary (RW-129).
inline HostString toHostString(const std::string& value)
{
    return HostString(value.c_str(), value.size());
}

inline std::string fromHostString(const HostString& value)
{
    return std::string(value.c_str(), static_cast<unsigned int>(value.size()));
}

inline unsigned int hostStringListSize(const HostStringList* values)
{
    return values ? static_cast<unsigned int>(values->size()) : 0;
}

template <typename Visitor>
inline void visitHostStringList(const HostStringList* values, Visitor visitor)
{
    if (!values)
        return;
    for (HostStringList::const_iterator it = values->begin(); it != values->end(); ++it)
        visitor(*it);
}

inline HostString makeHostString(const std::string& value)
{
    return HostString(value.c_str());
}

inline void assignFromHostString(std::string& destination, const HostString& value)
{
    destination = value.c_str();
}

// The recovered PE factories remain explicitly __fastcall.  Core/ClassManager.h's native record
// holds an ordinary free-function pointer (__cdecl under the supported build), so each recovered
// factory is published through one stateless thunk instead of changing the internal functions.
typedef dice::hfe::IBase* (__fastcall *RetailFastcallFactory)(unsigned int,
                                                               dice::hfe::IBase*);

template <RetailFastcallFactory Factory>
struct NativeFactoryThunk
{
    static dice::hfe::IBase* __cdecl invoke(unsigned int iid, dice::hfe::IBase* outer)
    {
        return Factory(iid, outer);
    }
};

} // namespace renddx9abi

#define RD9_HOST_VECTOR_PARAMETER(Type, name) std::vector<Type>& name
#define RD9_CLASS_FACTORY(factory) \
    (&::renddx9abi::NativeFactoryThunk<factory>::invoke)

#endif // RENDDX9ABI_PLUGINABI_H
