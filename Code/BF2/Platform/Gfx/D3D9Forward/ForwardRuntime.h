// [CHOSEN, not recovered] Runtime support for the generated D3D9 forwarding wrappers (G2w).
// Included by Generated/ForwardWrappers.cpp after ForwardSdk.h, which provides the namespaces
// `sub` (the renderer's subset headers) and `real` (the SDK, or the test fixture's stand-in).
//
// Reference counting model: every reference the renderer holds on a wrapper is also exactly one
// reference on the real object. AddRef/Release forward to the real object (so the counts the
// renderer sees are the real counts); the wrapper only keeps a private tally to know when the
// renderer's last reference is gone, then unregisters and deletes itself. The real object may
// outlive its wrapper (D3D holds internal references); asking for it again makes a new wrapper.
// There is at most one live wrapper per real object (the registry below).
#ifndef BF2_GFX_D3D9FORWARD_RUNTIME_H
#define BF2_GFX_D3D9FORWARD_RUNTIME_H

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace bf2fwd {

[[noreturn]] inline void Fatal(const char* what, const char* detail) {
    std::fprintf(stderr, "[FRONTIER] bf2fwd (BF2_GFX_SEAM): %s %s\n", what, detail);
    std::fflush(stderr);
    std::abort();
}

inline bool SameIid(REFIID a, const GUID& b) { return std::memcmp(&a, &b, sizeof(GUID)) == 0; }

// State shared by every wrapper regardless of interface: the registry entry.
struct Common {
    Common(::IUnknown* s, void* k) : self(s), key(k), refs(1) {}
    ::IUnknown* self;   // the wrapper, as its IUnknown
    void* key;          // the real object, as its IUnknown (never dereferenced after Release)
    long refs;          // references the renderer holds on the wrapper
};

class Registry {
public:
    static Registry& Get() { static Registry r; return r; }
    std::recursive_mutex mutex;   // recursive: real Release can re-enter through the state manager adapter
    std::unordered_map<void*, Common*> map;
};

typedef std::lock_guard<std::recursive_mutex> Lock;

// Implemented in the generated file: wraps `real` as the interface named by `iid` (adopting one
// reference), or returns nullptr if the subset does not know that interface.
void* WrapByIid(REFIID iid, void* real);

// The out-pointer of QueryInterface / GetContainer: wrap on success, release on an unknown IID.
inline HRESULT FinishIidOut(HRESULT hr, REFIID iid, void* obj, void** out) {
    if (out) *out = nullptr;
    if (FAILED(hr) || !obj) return hr;
    void* w = out ? WrapByIid(iid, obj) : nullptr;
    if (!w) {
        static_cast<::IUnknown*>(obj)->Release();
        return out ? E_NOINTERFACE : hr;
    }
    *out = w;
    return hr;
}

// The common base of every generated wrapper. SubI is the subset interface (what the renderer
// calls), RealI the SDK interface; Derived deletes itself.
template <class Derived, class SubI, class RealI>
class FwdBase : public SubI, public Common {
public:
    explicit FwdBase(RealI* r)
        : Common(static_cast<::IUnknown*>(static_cast<SubI*>(this)), static_cast<void*>(static_cast<::IUnknown*>(r))),
          real_(r) {
        Registry::Get().map[key] = this;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
#if defined(BF2_GFX_DXVK)
        // Preserve the native SDK's invalid-output result without acquiring a reference.
        if (!out) return real_->QueryInterface(iid, nullptr);
#endif
        void* obj = nullptr;
        HRESULT hr = real_->QueryInterface(iid, &obj);
        Lock g(Registry::Get().mutex);
        return FinishIidOut(hr, iid, obj, out);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        Lock g(Registry::Get().mutex);
        ++refs;
        return real_->AddRef();
    }
    ULONG STDMETHODCALLTYPE Release() override {
        Lock g(Registry::Get().mutex);
        void* k = key;
        ULONG r = real_->Release();
        if (--refs == 0) {
            Registry::Get().map.erase(k);
            delete static_cast<Derived*>(this);
        }
        return r;
    }
    RealI* real_;
};

// Take over one reference on `r`. If a wrapper for `r` is alive, that reference becomes one more
// renderer reference on it; otherwise a new wrapper is made.
template <class Wrapper, class SubI, class RealI>
SubI* AdoptT(RealI* r) {
    if (!r) return nullptr;
    Lock g(Registry::Get().mutex);
    auto it = Registry::Get().map.find(static_cast<void*>(static_cast<::IUnknown*>(r)));
    if (it != Registry::Get().map.end()) {
        ++it->second->refs;
        return static_cast<SubI*>(it->second->self);
    }
    return new Wrapper(r);
}

// IID_IUnknown: the wrapper registered for this real object, if any.
inline void* AdoptUnknown(::IUnknown* r) {
    Lock g(Registry::Get().mutex);
    auto it = Registry::Get().map.find(static_cast<void*>(r));
    if (it == Registry::Get().map.end()) {
#if !defined(BF2_GFX_DXVK)
        r->Release();
#endif
        // The native caller, FinishIidOut, releases the unwrapped reference once.
        return nullptr;
    }
    ++it->second->refs;
    return it->second->self;
}

// For callbacks: real D3DX hands the state manager a raw real pointer it does not give away.
// Take a reference for the duration of the call so the wrapper the renderer sees is balanced.
template <class RealI>
inline RealI* AddRefd(RealI* p) {
    if (p) p->AddRef();
    return p;
}
template <class T>
class Borrowed {
public:
    explicit Borrowed(T* p) : p_(p) {}
    ~Borrowed() { if (p_) p_->Release(); }
    T* get() const { return p_; }
private:
    Borrowed(const Borrowed&);
    Borrowed& operator=(const Borrowed&);
    T* p_;
};

} // namespace bf2fwd

#endif
