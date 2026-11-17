// [TEMP] [CHOSEN, not recovered] SDK-shaped resource fixture, issue #36.
// Exercises ownership and ABI conversion against DXVK's real public declarations without a GPU.
#include "ForwardSdk.h"
#include "../ForwardRuntime.h"
#include <cstdio>

extern "C" void* bf2fwd_adopt_IDirect3DVertexBuffer9(void*);
extern "C" void* bf2fwd_real_IDirect3DVertexBuffer9(void*);

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

// Keep storage on the stack so the checks can inspect the final reference count.
struct Buffer final : ::IDirect3DVertexBuffer9 {
    ULONG refs = 1;
    unsigned releases = 0;
    unsigned char data[64]{};
    HRESULT QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_IDirect3DResource9 || iid == IID_IDirect3DVertexBuffer9) {
            *out = this; AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG AddRef() override { return ++refs; }
    ULONG Release() override { ++releases; CHECK(refs != 0); return --refs; }
    HRESULT GetDevice(::IDirect3DDevice9** out) override { if (out) *out = nullptr; return D3DERR_INVALIDCALL; }
    HRESULT SetPrivateData(REFGUID, const void*, DWORD, DWORD) override { return E_NOTIMPL; }
    HRESULT GetPrivateData(REFGUID, void*, DWORD*) override { return E_NOTIMPL; }
    HRESULT FreePrivateData(REFGUID) override { return E_NOTIMPL; }
    DWORD SetPriority(DWORD) override { std::abort(); }
    DWORD GetPriority() override { std::abort(); }
    void PreLoad() override { std::abort(); }
    ::D3DRESOURCETYPE GetType() override { return ::D3DRTYPE_VERTEXBUFFER; }
    HRESULT Lock(UINT offset, UINT size, void** out, DWORD flags) override {
        CHECK(offset == 4 && size == 8 && flags == sub::D3DLOCK_READONLY);
        if (!out) return D3DERR_INVALIDCALL;
        *out = data + offset; return S_OK;
    }
    HRESULT Unlock() override { return S_OK; }
    HRESULT GetDesc(::D3DVERTEXBUFFER_DESC* out) override {
        if (!out) return D3DERR_INVALIDCALL;
        *out = {}; out->Type = ::D3DRTYPE_VERTEXBUFFER; out->Size = sizeof(data);
        out->Pool = ::D3DPOOL_MANAGED; return S_OK;
    }
};

int main()
{
    Buffer native;
    auto* wrapper = static_cast<sub::IDirect3DVertexBuffer9*>(bf2fwd_adopt_IDirect3DVertexBuffer9(&native));
    CHECK(native.refs == 1 && bf2fwd_real_IDirect3DVertexBuffer9(wrapper) == &native);
    CHECK(wrapper->GetType() == sub::D3DRTYPE_VERTEXBUFFER);
    sub::D3DVERTEXBUFFER_DESC desc{};
    CHECK(wrapper->GetDesc(&desc) == S_OK && desc.Size == 64 && desc.Pool == sub::D3DPOOL_MANAGED);
    void* bytes = nullptr;
    CHECK(wrapper->Lock(4, 8, &bytes, sub::D3DLOCK_READONLY) == S_OK && bytes == native.data + 4);
    CHECK(wrapper->Unlock() == S_OK);

    for (const GUID& iid : {IID_IDirect3DVertexBuffer9, IID_IDirect3DResource9, IID_IUnknown}) {
        void* queried = nullptr;
        CHECK(wrapper->QueryInterface(iid, &queried) == S_OK);
        CHECK(queried == wrapper && native.refs == 2);
        CHECK(static_cast<IUnknown*>(queried)->Release() == 1);
    }
    CHECK(wrapper->QueryInterface(IID_IUnknown, nullptr) == E_POINTER && native.refs == 1);
    void* unsupported = wrapper;
    CHECK(wrapper->QueryInterface(IID_IDirect3DTexture9, &unsupported) == E_NOINTERFACE);
    CHECK(!unsupported && native.refs == 1);

    // The native API owns a reference independently of the renderer; re-adoption
    // after the last wrapper release must remain possible and balanced.
    native.AddRef();
    CHECK(wrapper->Release() == 1);
    CHECK(bf2fwd::Registry::Get().map.empty());
    wrapper = static_cast<sub::IDirect3DVertexBuffer9*>(bf2fwd_adopt_IDirect3DVertexBuffer9(&native));
    CHECK(wrapper->Release() == 0);
    CHECK(bf2fwd::Registry::Get().map.empty());

    Buffer unknown;
    void* out = &unknown;
    CHECK(bf2fwd::FinishIidOut(S_OK, IID_IUnknown, &unknown, &out) == E_NOINTERFACE);
    CHECK(out == nullptr && unknown.refs == 0 && unknown.releases == 1);
    std::fprintf(stderr, "DXVK SDK ownership checks: %s\n", failures ? "FAILED" : "passed");
    return failures ? 1 : 0;
}
