// Effect techniques and semantic-keyed parameter handles.
// Handles can exist before the compiled effect supplies their reflected parameters.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "../../RendDX9Abi/EngineFile.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "RendDX9Base.h"
#include "ShaderParameter.h"
#include "../RendMisc/IRenderer.h"

#include <new>
#include <map>
#include <string>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <cstdlib>

namespace dice {
namespace hfe {
namespace dx9rend {

extern void* g_shaderManager;   // ShaderManager.cpp
extern void* g_stateManager;    // StateManager.cpp - dword_1023B984
void shaderManagerSetCurrentEffect(void* effect);
void vertexDeclarationBind(void* wrapper);          // BufferManager.cpp - sub_100147C0
void* vertexDeclarationCreate(const void* elements);
void vertexDeclarationRelease(void* wrapper);        // inline decrement + 10014790
int vertexDeclarationTypeSize(unsigned int type);    // RendDX9Types.cpp - 1000CC30
void stateManagerPush();
void stateManagerPop();
extern IBase* g_Renderer;       // BF2Render.cpp


// The Effect constructor's tail is a first-time-only block: if the global exists it is AddRef'd,
// otherwise an 8-byte {vptr, refCount=1} is allocated and stored. Every parameter handle then holds
// a counted reference to it. It is a process-wide singleton, not per-effect, and the constructor is
// its only creator.

// FXParameter.cpp
ParameterOwner* createFXParameter(void* effect, void* d3dxHandle);

// [INFERRED] source spelling. a native value type, not
// an ABI overlay. The retail 52-byte size includes its VC7 std::string.
// All outside consumers retain opaque pointers and return them to this TU.
struct EffectTechnique
{
    std::string name;
    void* handle;
    void* declaration;
    int restore;
    unsigned int passes;
    EffectTechnique* states;
    bool validated;

    EffectTechnique()
        : handle(0), declaration(0), restore(1), passes(0), states(0), validated(false) {}
    ~EffectTechnique()
    {
        if (declaration) vertexDeclarationRelease(declaration);
        delete states;
    }
};

namespace {

// [TEMP] [CHOSEN, not recovered] Exact GSMaxValidation failure arms only.
// [RETIRE-WHEN: RW-48] Recover path-name expansion / upstream DXErr9 diagnostics
// and both original MessageBox retry outcomes. Legacy RD9 continue mode cannot
// turn these missing bodies into an invented non-retry answer.
[[noreturn]] void techniqueDialogFrontier(const char* method, const void* self)
{
    std::fprintf(stderr, "[FRONTIER] %s reached\n", method);
    std::fflush(stderr);
    ::renddx9abi::frontier(method, self, 0);
    std::abort();
}

// the process-wide owner used before an Effect has a compiled D3DX parameter.
// The five no-op setter bodies are the shared `retn 4` / `retn 8` functions at 0x100E13B0 and
// The two queries are exact false/zero bodies at 0x10083AF0 and 0x100ED3B0.
class DefaultParameterOwner final : public ParameterOwner
{
public:
    DefaultParameterOwner() : m_refCount(1) {}

    int getRef() const { return m_refCount; }
    int addRef() { return ++m_refCount; }
    int release()
    {
        const int remaining = --m_refCount;
        if (!remaining)
            ::renddx9abi::engineFree(this);
        return remaining;
    }
    void* getD3DXParam() const { return 0; }
    void setD3DXParam(void*) {}
    bool isParameterUsed() const { return false; }
    void setValue(const void*, unsigned int) {}
    void setBool(unsigned char) {}
    void setInt(int) {}
    void setFloat(float) {}
    void setMatrixPointerArray(const void* const*, unsigned int) {}
    unsigned int getTypeCode() const { return 0; }

private:
    int m_refCount; // +0x04
};

#if defined(_M_IX86)
static_assert(sizeof(DefaultParameterOwner) == 0x08,
              "dword_1023C3B8 owns the shipped eight-byte default parameter owner");
#endif

ParameterOwner* g_parameterOwner = 0; // dword_1023C3B8

} // anonymous namespace

// The constructor deliberately has no owner-null guard;
// the destructor does.  Every remaining method is one typed virtual forward, matching the ten-byte
// 0x10021910..0x10021990 tail-dispatch family without source-level vtable indexing.
ShaderParameter::ShaderParameter(ParameterOwner* owner)
    : m_refCount(1)
    , m_owner(owner)
{
    m_owner->addRef();
}

ShaderParameter::~ShaderParameter()
{
    if (m_owner)
        m_owner->release();
    m_owner = 0;
}

int ShaderParameter::getRef() const { return m_refCount; }
int ShaderParameter::addRef() { return ++m_refCount; }

int ShaderParameter::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        this->~ShaderParameter();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

void* ShaderParameter::getD3DXParam() const { return m_owner->getD3DXParam(); }
void ShaderParameter::setD3DXParam(void* handle) { m_owner->setD3DXParam(handle); }
bool ShaderParameter::isParameterUsed() const { return m_owner->isParameterUsed(); }
void ShaderParameter::setValue(const void* data, unsigned int count)
{
    m_owner->setValue(data, count);
}
void ShaderParameter::setBool(unsigned char value) { m_owner->setBool(value); }
void ShaderParameter::setInt(int value) { m_owner->setInt(value); }
void ShaderParameter::setFloat(float value) { m_owner->setFloat(value); }
void ShaderParameter::setMatrixPointerArray(const void* const* matrices, unsigned int count)
{
    m_owner->setMatrixPointerArray(matrices, count);
}
unsigned int ShaderParameter::getTypeCode() const { return m_owner->getTypeCode(); }

// Reference-counted effect wrapper with a four-slot IBase interface.
// Caches validated techniques and named parameters. Values set before binding are copied
// into a pending queue, then applied and removed when the effect is bound.
// Native technique containers are private; their layout need not match the retail object.
class Effect
{
public:
    Effect();

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;

    // sub_1001E3A0, read in full. See the body.
    void* getShaderParameter(const vc71::string& name);

    bool         bind(ID3DXEffect* d3dxEffect);
    unsigned int populateParameters();
    void         flushPendingValues();
    int          validateTechniques();
    int          validateTechnique(const char* n);
    int          linkStateTechnique(const char* n);
    bool         readDeclaration(D3DXHANDLE technique, void** out);
    bool         isParameterUsed(void* handle) const;

    bool         setTechniqueByName(const vc71::string& name);
    void         setTechniqueRecord(unsigned char* record);
    unsigned int begin();
    void         beginPass(unsigned int index);
    void         end();

    ID3DXEffect* d3dxEffect() const { return m_d3dxEffect; }
    typedef std::map<std::string, EffectTechnique> TechniqueMap;
    TechniqueMap& techniqueMap() { return m_techniques; }
    unsigned int currentTechniquePassCount() const
    {
        return m_currentTechnique ? m_currentTechnique->passes : 0u;
    }

    // sub_1001DB60 / sub_1001DB70 read +0x34 - the current technique record - directly; this is
    // that read for the two seams below.
    unsigned char* currentTechniqueRecord() const
    { return reinterpret_cast<unsigned char*>(m_currentTechnique); }

    // The constructor cannot reach the table (it is defined below), so the factory installs it -
    // exactly as the original's factory does after calling the constructor.
    void  installVtable(void** vtable) { m_vptr = vtable; }

    // sub_1001DB00 - the cached int parameter "ShaderVersion": re-read from D3DX while +0x74 is
    // set (the constructor sets it), answered from +0x70. Terrain::drawMain selects its light pass
    // on it. No shipped shader defines the parameter, so it reads 0 on retail content.
    int*  shaderVersion();

    // Clears texture parameters held by the effect.
    void  clearTextureParameters();

private:
    void*            m_vptr;          // +0x00
    int              m_refCount;      // +0x04
    ID3DXEffect*     m_d3dxEffect;    // +0x08
    vc71::string     m_string0C;      // +0x0C  the effect's name
    // Native private state from here on: subsequent comments denote retail offsets only.
    // No external reader uses these offsets; opaque technique pointers round-trip here.
    TechniqueMap    m_techniques;    // retail +0x28
    EffectTechnique* m_currentTechnique;   // retail +0x34
    vc71::string     m_string38;      // +0x38  the current technique's name, copied on selection
    vc71::map_layout m_parameters;    // +0x54  node 48
    vc71::map_layout m_pendingValues; // +0x60  node 2096 - the write queue
    unsigned char    m_byte6C;        // +0x6C
    unsigned char    m_pad6D[3];
    unsigned int     m_dword70;       // +0x70
    unsigned char    m_byte74;        // +0x74  ctor: 1
    unsigned char    m_pad75[3];
    std::vector<std::string> m_stringVec; // retail +0x78, state-technique names
};


// sub_1001ED90, read in full.

Effect::Effect()
    : m_vptr(0)          // installed by createEffectObject - see installVtable
    , m_refCount(1)
    , m_d3dxEffect(0)
    , m_currentTechnique(0)
    , m_byte6C(0)
    , m_dword70(0)
    , m_byte74(1)
{
    m_pad6D[0] = 0; m_pad6D[1] = 0; m_pad6D[2] = 0;
    m_pad75[0] = 0; m_pad75[1] = 0; m_pad75[2] = 0;

    // The remaining two legacy private maps are tracked separately; this change
    // does not claim their raw nodes are recovered source-level containers.
    m_parameters.construct(0x30);
    m_pendingValues.construct(0x830);

    // The shared owner. First Effect creates it; the rest AddRef it.
    if (g_parameterOwner)
        g_parameterOwner->addRef();
    else
    {
        void* const storage = ::renddx9abi::engineAlloc(sizeof(DefaultParameterOwner));
        g_parameterOwner = storage ? new (storage) DefaultParameterOwner() : 0;
    }
}

int Effect::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        // [FRONTIER] dice::hfe::dx9rend::Effect::~Effect - the three maps' contents, the
        // ID3DXEffect release and the owner's counted reference are all unwound by the shipped
        // destructor (0x10021160), which is unread. Freeing the object without them would leak
        // rather than crash, so the frontier is here rather than a partial teardown.
        RD9_FRONTIER("dice::hfe::dx9rend::Effect::~Effect (0x10021160)", 0x08);
    }
    return remaining;
}

// the folded body that accepts iid 1 and nothing else.
void* Effect::queryInterface(unsigned int iid) const
{
    return iid == RD9_IID_IBASE ? const_cast<Effect*>(this) : 0;
}


// Returns a borrowed semantic handle, creating and caching it on a miss.
// Allocation failure is cached as null, so later lookups do not retry.

void* Effect::getShaderParameter(const vc71::string& name)
{
    // The find, and then the create-and-cache. Both go through the shared helpers in Vc71Map.h,
    // which build a real BST with VC7.1's header convention - see the warning there about why the
    // shape matters to bind's traversals rather than to these lookups.
    vc71::map_node* node = vc71::mapFind(m_parameters, name.c_str());
    if (node)
        return vc71::mapNodeValueRef(node);

    void* const storage = ::renddx9abi::engineAlloc(sizeof(ShaderParameter));
    ShaderParameter* const p = storage ? new (storage) ShaderParameter(g_parameterOwner) : 0;

    // Cache allocation failures as null entries; later lookups do not retry.
    node = vc71::mapInsert(m_parameters, 0x30, name.c_str());
    if (node)
        vc71::mapNodeValueRef(node) = p;

    return p;
}


// Binds the supplied compiled effect, replacing its previous device state and parameter bindings.
// Returns false for a null wrapper.

bool effectBind(Effect* self, void* d3dxEffect)
{
    RD9_TRACE_AT("dice::hfe::dx9rend::Effect::bind", self, 0x00);
    return self ? self->bind(static_cast<ID3DXEffect*>(d3dxEffect)) : false;
}

bool Effect::bind(ID3DXEffect* d3dxEffect)
{
    if (m_d3dxEffect)
    {
        // [FRONTIER] dice::hfe::dx9rend::Effect::bind - the rebind path. sub_1001E570 flushes,
        // then OnLostDevice and Release run on the outgoing effect. Unreachable until an effect is
        // recompiled over a live one.
        RD9_FRONTIER("dice::hfe::dx9rend::Effect::bind (an effect is already bound - the "
                     "flush/OnLostDevice/Release path is unwritten)", 0x00);
        return false;
    }

    // sub_1001DE30 - reset every cached technique record. Empty here, and the original's own
    // `*head != head` guard is what makes that a no-op rather than a special case.
    if (!m_techniques.empty())
    {
        RD9_FRONTIER("dice::hfe::dx9rend::Effect::bind (the technique cache is non-empty; "
                     "sub_1001F200 per entry is unwritten)", 0x00);
        return false;
    }

    if (!d3dxEffect)
        return false;

    d3dxEffect->AddRef();
    m_d3dxEffect = d3dxEffect;

    // when the ShaderManager's +0xA4 byte is clear -
    // and its constructor leaves it clear - every freshly bound effect is handed the StateManager
    // through ID3DXEffect::SetStateManager (fx vtbl +0x11C). From here on, every state a pass sets
    // routes through dword_1023B984 instead of the device.
    if (g_shaderManager
        && !*(static_cast<unsigned char*>(g_shaderManager) + 0xA4)
        && g_stateManager)
    {
        d3dxEffect->SetStateManager(static_cast<ID3DXEffectStateManager*>(g_stateManager));
    }

    const unsigned int populated = m_parameters.size ? 0u : populateParameters();

    flushPendingValues();

    const int verdict = validateTechniques();

    ::renddx9abi::logf("Effect::bind: effect=0x%08X, %u parameters registered, %u techniques "
                       "validated (verdict %d)", ::renddx9abi::addr(d3dxEffect), populated,
                       static_cast<unsigned int>(m_techniques.size()), verdict);

    return verdict != 3;
}


// Adds or rebinds parameters keyed by semantic, skipping parameters without one.
// Each registry handle owns its parameter implementation; returns the number added.

unsigned int Effect::populateParameters()
{
    unsigned int added = 0;

    for (unsigned int i = 0; ; ++i)
    {
        D3DXHANDLE h = m_d3dxEffect->GetParameter(0, i);
        if (!h)
            break;

        D3DXPARAMETER_DESC desc;
        for (unsigned int k = 0; k < sizeof(desc) / 4; ++k)
            reinterpret_cast<unsigned int*>(&desc)[k] = 0;
        m_d3dxEffect->GetParameterDesc(h, &desc);

        if (!desc.Semantic)
            continue;                       // no semantic - not registered at all

        vc71::map_node* node = vc71::mapFind(m_parameters, desc.Semantic);
        if (node)
        {
            ShaderParameter* const existing =
                static_cast<ShaderParameter*>(vc71::mapNodeValueRef(node));
            if (existing)
                existing->setD3DXParam(::renddx9abi::d3dxHandleValue(h));
            continue;
        }

        ParameterOwner* const owner = createFXParameter(this, ::renddx9abi::d3dxHandleValue(h));

        void* const storage = ::renddx9abi::engineAlloc(sizeof(ShaderParameter));
        ShaderParameter* const handle = storage ? new (storage) ShaderParameter(owner) : 0;
        // PE 0x1001E4DB releases unconditionally. The ShaderParameter constructor first took its
        // own reference when allocation succeeded, leaving that handle as the sole owner.
        owner->release();

        node = vc71::mapInsert(m_parameters, 0x30, desc.Semantic);
        if (node)
            vc71::mapNodeValueRef(node) = handle;
        ++added;
    }

    return added;
}


// Applies queued parameter values through their registered handlers, then clears the queue.

void Effect::flushPendingValues()
{
    if (!m_pendingValues.empty())
    {
        // [FRONTIER] dice::hfe::dx9rend::Effect::flushPendingValues - the per-entry apply
        RD9_FRONTIER("dice::hfe::dx9rend::Effect::flushPendingValues (the queue is non-empty; the "
                     "per-entry apply through vslot +0x0C is unwritten)", 0x00);
        return;
    }

    // The tail runs unconditionally: relink the sentinel and zero the count.
    if (m_pendingValues.head)
    {
        m_pendingValues.head->parent = m_pendingValues.head;
        m_pendingValues.head->left   = m_pendingValues.head;
        m_pendingValues.head->right  = m_pendingValues.head;
    }
    m_pendingValues.size = 0;
}


// Validates every technique and queued technique name. Returns 3 when the user requests
// Retry after a failure; returns 2 on completion. These are verdict codes, not Booleans.

int Effect::validateTechniques()
{
    for (unsigned int i = 0; ; ++i)
    {
        D3DXHANDLE h = m_d3dxEffect->GetTechnique(i);
        if (!h)
            break;

        D3DXTECHNIQUE_DESC desc;
        m_d3dxEffect->GetTechniqueDesc(h, &desc);

        if (validateTechnique(desc.Name) == 3)
            return 3;
    }

    // Retail deliberately ignores even the Retry verdict returned by this postpass.
    for (std::size_t i = 0; i < m_stringVec.size(); ++i)
        linkStateTechnique(m_stringVec[i].c_str());
    return 2;
}


// sub_1001E630 - validate ONE technique and cache the result.

// GSMaxValidation is cached once. Its two diagnostic dialogs still require the
// original path-name helper / upstream DXErr9 text; those exact arms stop visibly.
// Name and validated flag are written before lookup; non-retry failures retain
// validated=true, and success means HRESULT == 0 (not merely succeeded(hr)).

int Effect::validateTechnique(const char* name)
{
    // It is the first strstr match, not a generic suffix comparison.
    const char* suffix = ::strstr(name, "_States");
    if (suffix && suffix + 7 == name + ::strlen(name))
        m_stringVec.push_back(name);

    EffectTechnique& record = m_techniques[name];
    record.name = name;
    record.validated = true;
    static const bool maxValidation = readBoolSetting("GSMaxValidation", false);

    D3DXHANDLE h = m_d3dxEffect->GetTechniqueByName(name);
    if (!h)
    {
        if (maxValidation)
            techniqueDialogFrontier("dice::hfe::dx9rend::Effect::validateTechnique (1001E7DD missing-technique retry dialog)", this);
        return 0;
    }

    D3DXTECHNIQUE_DESC desc; // Ignored HRESULT; retail does not initialize this output.
    m_d3dxEffect->GetTechniqueDesc(h, &desc);
    const long hr = m_d3dxEffect->ValidateTechnique(h);
    if (hr != 0)
    {
        if (maxValidation)
            techniqueDialogFrontier("dice::hfe::dx9rend::Effect::validateTechnique (1001E8C4 validation retry dialog / DXErr9)", this);
        return 1;
    }

    record.handle = ::renddx9abi::d3dxHandleValue(h);
    record.passes = desc.Passes;
    readDeclaration(h, &record.declaration);
    return 2;
}

// The base name ends at the first "_States". A missing or invalid
// base is validated again, with no end/null guard after that lookup. The linked
// record keeps constructor defaults for its pass count and declaration.
int Effect::linkStateTechnique(const char* name)
{
    D3DXHANDLE h = m_d3dxEffect->GetTechniqueByName(name);
    if (!h) return 0;
    D3DXTECHNIQUE_DESC desc;
    m_d3dxEffect->GetTechniqueDesc(h, &desc);
    std::string baseName(name);
    baseName.resize(::strstr(name, "_States") - name);
    TechniqueMap::iterator base = m_techniques.find(baseName);
    if (base == m_techniques.end() || !base->second.validated)
    {
        if (validateTechnique(baseName.c_str()) == 3) return 3;
        base = m_techniques.find(baseName);
    }
    if (!base->second.states)
        base->second.states = new (std::nothrow) EffectTechnique;
    int restore = 0;
    D3DXHANDLE annotation = m_d3dxEffect->GetAnnotationByName(h, "Restore");
    if (annotation)
        m_d3dxEffect->GetBool(annotation, &restore);
    EffectTechnique& state = *base->second.states; // Retail has no allocation-failure guard.
    state.name = name;
    state.handle = ::renddx9abi::d3dxHandleValue(h);
    state.restore = restore;
    state.validated = true;
    return 2;
}

// Declaration consists of int quadruples {stream,type,usage,index}.
// Preserve full-width stream comparisons before byte truncation, offset reset
// on stream changes, zero offset for the 255 terminator, and ignored HRESULTs.
bool Effect::readDeclaration(D3DXHANDLE technique, void** out)
{
    *out = 0; // Does not release the previous declaration.
    D3DXHANDLE annotation = m_d3dxEffect->GetAnnotationByName(technique, "Declaration");
    if (!annotation) return false;
    D3DXPARAMETER_DESC desc;
    m_d3dxEffect->GetParameterDesc(annotation, &desc);
    int* values = new int[desc.Elements];
    m_d3dxEffect->GetIntArray(annotation, values, desc.Elements);
    int stream = values[0]; // Retail reads this even for zero elements.
    D3DVERTEXELEMENT9* elements = new D3DVERTEXELEMENT9[desc.Elements >> 2];
    unsigned int offset = 0;
    for (unsigned int i = 0; i < (desc.Elements >> 2); ++i)
    {
        const int* source = values + 4 * i;
        if (source[0] != stream) { offset = 0; stream = source[0]; }
        D3DVERTEXELEMENT9& element = elements[i];
        element.Stream = static_cast<unsigned char>(source[0]);
        element.Type = static_cast<unsigned char>(source[1]);
        element.Usage = static_cast<unsigned char>(source[2]);
        element.UsageIndex = static_cast<unsigned char>(source[3]);
        element.Method = 0;
        element.Offset = element.Stream == 255 ? 0 : static_cast<unsigned short>(offset);
        offset += vertexDeclarationTypeSize(element.Type);
    }
    *out = vertexDeclarationCreate(elements);
    delete[] values;
    delete[] elements;
    return true;
}


// What FXParameterImpl::setD3DXParam calls back for. Null until bind has stored one.
void* effectD3dxHandle(void* effect)
{
    return effect ? static_cast<Effect*>(effect)->d3dxEffect() : 0;
}

// With no selected technique every parameter is considered used. Otherwise the
// compiled effect answers ID3DXEffect::IsParameterUsed(handle, currentTechnique->handle).
bool Effect::isParameterUsed(void* handle) const
{
    if (!m_currentTechnique)
        return true;

    return m_d3dxEffect->IsParameterUsed(::renddx9abi::d3dxHandle(handle),
                                         ::renddx9abi::d3dxHandle(m_currentTechnique->handle)) != 0;
}

bool effectIsParameterUsed(void* effect, void* parameter)
{
    return static_cast<Effect*>(effect)->isParameterUsed(parameter);
}


// Effect_setTechniqueByName, select a validated technique by name. A miss in the
// technique map logs the shipped Effect.cpp:299 line (severity 6) and changes nothing; a hit
// stores the record pointer, copies its name string into +0x38, and calls the D3DX SetTechnique
// (fx vtbl +0xE8) with the cached handle at record+28.

// Technique records are native values; every reader uses their typed members.

bool effectSetTechniqueByName(void* effect, const vc71::string& name)
{
    return effect ? static_cast<Effect*>(effect)->setTechniqueByName(name) : false;
}

// sub_1001DC20 - the constructor-side probe FSQuadDrawer uses before selecting "Blit" or its
// "Blit_1_4" fallback.  A cached record is usable only when its D3DX technique handle is non-null.
bool effectHasTechnique(void* effect, const vc71::string& name)
{
    Effect* self = static_cast<Effect*>(effect);
    Effect::TechniqueMap::iterator node = self->techniqueMap().find(name.c_str());
    return node != self->techniqueMap().end() && node->second.handle != 0;
}

// sub_1001DB00, read in full. A failed GetInt leaves 0 and clears the re-read flag the same way a
// success does.
int* Effect::shaderVersion()
{
    if (m_byte74)
    {
        const D3DXHANDLE handle = m_d3dxEffect->GetParameterByName(0, "ShaderVersion");
        int value = 0;
        int read = 0;
        if (handle)
        {
            if (m_d3dxEffect->GetInt(handle, &read) < 0)
            {
                m_dword70 = 0;
                m_byte74 = 0;
                return reinterpret_cast<int*>(&m_dword70);
            }
            value = read;
        }
        m_dword70 = static_cast<unsigned int>(value);
        m_byte74 = 0;
    }
    return reinterpret_cast<int*>(&m_dword70);
}

int* effectShaderVersion(void* effect)
{
    return static_cast<Effect*>(effect)->shaderVersion();
}

// sub_1001D9F0 - number of passes in the currently selected technique, or zero when none is set.
unsigned int effectCurrentPassCount(void* effect)
{
    Effect* self = static_cast<Effect*>(effect);
    // Keep this tiny accessor in the owning TU: EffectTechnique is deliberately opaque outside it.
    // The PE likewise performs no null check on `effect` itself.
    return self->currentTechniquePassCount();
}

bool Effect::setTechniqueByName(const vc71::string& name)
{
    TechniqueMap::iterator node = m_techniques.find(name.c_str());
    if (node == m_techniques.end())
    {
        ::renddx9abi::logf("Effect.cpp:299 Technique not found %s in effect %s", name.c_str(),
                           m_string0C.c_str());
        return false;
    }

    EffectTechnique* record = &node->second;
    m_currentTechnique = record;
    m_string38 = record->name.c_str();
    m_d3dxEffect->SetTechnique(::renddx9abi::d3dxHandle(record->handle));
    return true;
}


// Begins the effect and returns its pass count, using StateManager-owned state preservation.
// beginPass closes any previous pass; end closes a pending pass and clears the current effect.

unsigned int Effect::begin()
{
    unsigned int passes = 0;
    const bool smDisabled =
        g_shaderManager && *(static_cast<unsigned char*>(g_shaderManager) + 0xA4) != 0;
    m_d3dxEffect->Begin(&passes, smDisabled ? 0u : 7u);

    if (m_currentTechnique)
    {
        void* declWrapper = m_currentTechnique->declaration;
        if (declWrapper)
        {
            // sub_100147C0 - the decl wrapper's bind: StateManager SetVertexDeclaration with the
            // D3D declaration at wrapper+8. The annotation reader above creates it.
            vertexDeclarationBind(declWrapper);
        }
    }

    shaderManagerSetCurrentEffect(this);
    return passes;
}

void Effect::beginPass(unsigned int index)
{
    if (m_byte6C)
        m_d3dxEffect->EndPass();
    m_d3dxEffect->BeginPass(index);
    m_byte6C = 1;
}

void Effect::end()
{
    if (m_byte6C)
        m_d3dxEffect->EndPass();
    m_d3dxEffect->End();
    m_byte6C = 0;
    shaderManagerSetCurrentEffect(0);
}


// sub_1001DBF0 - find a technique record by name: the map lookup, then null on a miss or the
// record on a hit. SwiffRender caches these pointers and re-finds them every beginDisplay and on
// device reset. Native std::map keeps each value pointer stable across insertions.

void* effectFindTechniqueRecord(void* effect, const char* name)
{
    if (!effect)
        return 0;
    Effect* self = static_cast<Effect*>(effect);
    Effect::TechniqueMap::iterator node = self->techniqueMap().find(name);
    return node == self->techniqueMap().end() ? 0 : &node->second;
}


// sub_1001DB80 - select a technique BY record: store it, copy its name string into +0x38, and call
// the D3DX SetTechnique with the cached handle at record+28. The fast path setTechniqueByName
// takes after its own lookup, exposed because SwiffRender's style applier dispatches on records.

void effectSetTechniqueRecord(void* effect, void* record)
{
    if (!effect || !record)
        return;
    static_cast<Effect*>(effect)->setTechniqueRecord(static_cast<unsigned char*>(record));
}

void Effect::setTechniqueRecord(unsigned char* record)
{
    EffectTechnique* technique = reinterpret_cast<EffectTechnique*>(record);
    m_currentTechnique = technique;
    m_string38 = technique->name.c_str();
    m_d3dxEffect->SetTechnique(::renddx9abi::d3dxHandle(technique->handle));
}

// The record's +44 link points to the state technique.
// Names are [INFERRED]. Opaque pointers now resolve to native private records.
// D3DX calls and their ignored HRESULTs follow the PE exactly.
void effectBeginTechniqueStateRecord(void* effect, unsigned char* record)
{
    EffectTechnique* technique = reinterpret_cast<EffectTechnique*>(record);
    EffectTechnique* state = technique->states;
    if (!state) return;
    Effect* self = static_cast<Effect*>(effect);
    ID3DXEffect* fx = self->d3dxEffect();
    fx->SetTechnique(::renddx9abi::d3dxHandle(state->handle));
    unsigned int passes; // Retail ignores failure and does not initialize the output.
    fx = self->d3dxEffect();
    fx->Begin(&passes, 7);
    if (passes != 0)
    {
        stateManagerPush();
        fx = self->d3dxEffect();
        fx->BeginPass(0);
        fx = self->d3dxEffect();
        fx->EndPass();
    }
    fx = self->d3dxEffect();
    fx->End();
    fx = self->d3dxEffect();
    fx->SetTechnique(::renddx9abi::d3dxHandle(technique->handle));
}
void effectEndTechniqueStateRecord(void* effect, unsigned char* record)
{
    EffectTechnique* technique = reinterpret_cast<EffectTechnique*>(record);
    EffectTechnique* state = technique->states;
    if (!state) return;
    Effect* self = static_cast<Effect*>(effect);
    ID3DXEffect* fx = self->d3dxEffect();
    fx->SetTechnique(::renddx9abi::d3dxHandle(state->handle));
    unsigned int passes;
    fx = self->d3dxEffect();
    fx->Begin(&passes, 7);
    if (passes > 1)
    {
        fx = self->d3dxEffect();
        fx->BeginPass(1);
        fx = self->d3dxEffect();
        fx->EndPass();
        stateManagerPop();
    }
    fx = self->d3dxEffect();
    fx->End();
    fx = self->d3dxEffect();
    fx->SetTechnique(::renddx9abi::d3dxHandle(technique->handle));
}
void effectBeginTechniqueStates(void* effect, const vc71::string& name)
{
    Effect* self = static_cast<Effect*>(effect); // no null-this guard in retail
    Effect::TechniqueMap::iterator found = self->techniqueMap().find(name.c_str());
    if (found != self->techniqueMap().end())
        effectBeginTechniqueStateRecord(self, reinterpret_cast<unsigned char*>(&found->second));
}
void effectEndTechniqueStates(void* effect, const vc71::string& name)
{
    Effect* self = static_cast<Effect*>(effect);
    Effect::TechniqueMap::iterator found = self->techniqueMap().find(name.c_str());
    if (found != self->techniqueMap().end())
        effectEndTechniqueStateRecord(self, reinterpret_cast<unsigned char*>(&found->second));
}

// Applies/closes the current technique's optional _States companion.
// A failed technique-name lookup leaves that companion unchanged.
void effectBeginCurrentTechniqueStates(void* effect)
{ Effect* const self = static_cast<Effect*>(effect); unsigned char* const record =
  self->currentTechniqueRecord(); if (record) effectBeginTechniqueStateRecord(self, record); }
void effectEndCurrentTechniqueStates(void* effect)
{ Effect* const self = static_cast<Effect*>(effect); unsigned char* const record =
  self->currentTechniqueRecord(); if (record) effectEndTechniqueStateRecord(self, record); }

// The seams the QuadDrawer flush calls.
unsigned int effectBegin(void* effect)
{
    return effect ? static_cast<Effect*>(effect)->begin() : 0;
}
void effectBeginPass(void* effect, unsigned int index)
{
    if (effect)
        static_cast<Effect*>(effect)->beginPass(index);
}
void effectEnd(void* effect)
{
    if (effect)
        static_cast<Effect*>(effect)->end();
}

namespace {


// Effect's reference slots are addRef then getRef, opposite to ParameterOwner/ShaderParameter.

void __fastcall effect_addRef(void* s, void*) { static_cast<Effect*>(s)->addRef(); }
int  __fastcall effect_getRef(void* s, void*) { return static_cast<Effect*>(s)->getRef(); }
int  __fastcall effect_release(void* s, void*) { return static_cast<Effect*>(s)->release(); }
void* __fastcall effect_queryInterface(void* s, void*, unsigned int iid)
{
    return static_cast<Effect*>(s)->queryInterface(iid);
}

void* const kEffectVtable[4] =
{
    reinterpret_cast<void*>(&effect_addRef),          // +0x00
    reinterpret_cast<void*>(&effect_getRef),          // +0x04
    reinterpret_cast<void*>(&effect_release),         // +0x08
    reinterpret_cast<void*>(&effect_queryInterface)   // +0x0C
};

} // anonymous namespace

Effect* createEffectObject()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(Effect));
    if (!storage)
        return 0;
    Effect* self = new (storage) Effect();
    self->installVtable(const_cast<void**>(kEffectVtable));
    return self;
}


// Returns an owned parameter reference; the caller must release it.
// Requires a valid effect and an existing semantic. Neither pointer is null-checked.

ShaderParameter* effectGetShaderParameterTyped(Effect* self, const vc71::string& name)
{
    ShaderParameter* const handle = static_cast<ShaderParameter*>(self->getShaderParameter(name));
    handle->addRef();
    return handle;
}

void* effectGetShaderParameter(Effect* self, const vc71::string& name)
{
    return effectGetShaderParameterTyped(self, name);
}

// sub_1001E3A0 itself, exposed for the one FSQuadDrawer constructor member (stencilpass) that
// deliberately retains the Effect map's existing reference rather than taking the caller-facing
// extra reference sub_1001E420 adds for every other semantic.
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect* self,
                                                       const vc71::string& name)
{
    return static_cast<ShaderParameter*>(self->getShaderParameter(name));
}

void effectAddRef(Effect* effect)
{
    effect->addRef();
}

int effectRelease(Effect* effect)
{
    return effect->release();
}


// Clears bound texture values so the effect releases level texture references.
// Leaves the parameter registry intact and ignores SetTexture failures.

void Effect::clearTextureParameters()
{
    for (vc71::map_node* node = vc71::mapFirst(m_parameters); node;
         node = vc71::mapNext(m_parameters, node))
    {
        ShaderParameter* const parameter = static_cast<ShaderParameter*>(vc71::mapNodeValue(node));
        if (parameter->getTypeCode() & 0x800u)                                    // vtbl +0x2C
        {
            void* const handle =
                static_cast<ShaderParameter*>(vc71::mapNodeValue(node))->getD3DXParam();  // +0x0C
            m_d3dxEffect->SetTexture(::renddx9abi::d3dxHandle(handle), 0);
        }
    }
}

void effectClearTextureParameters(Effect* effect)
{
    effect->clearTextureParameters();
}


// THE compile - sub_1001E010, read in full and written.

namespace {


// Shader decoding is conditional on the magic prefix; plaintext remains unchanged.
// The byte position participates in the cipher in addition to the repeating key.

const unsigned char kObfuscationMagic[10] =
{
    0x23, 0x5B, 0x07, 0x02, 0x63, 0x67, 0x02, 0x58, 0x2D, 0x27
};

// byte_101E0A60 - thirty-two ASCII characters, and they are a hex digest rather than random bytes.
const char kObfuscationKey[33] = "ca1a450de56a8c4d317d13ebb616d492";

} // anonymous namespace

// Decodes shader sources and included fragments in place.
void deobfuscateShader(unsigned char* buf, unsigned int size)
{
    if (size < 10)
        return;
    for (int i = 0; i < 10; ++i)
        if (buf[i] != kObfuscationMagic[i])
            return;   // plaintext - the strncmp mismatch path

    for (unsigned int i = 0; i < size; ++i)
        buf[i] = static_cast<unsigned char>(
            ((static_cast<unsigned char>(i) ^ buf[i] ^ kObfuscationKey[i & 0x1F]) - 17) & 0xFF);
}

namespace {


// Prefixes paths lacking "mods/" with the active base/expansion mod directory.

void applyModPrefix(char* out, unsigned int outSize, const char* path)
{
    if (::strstr(path, "mods/"))
    {
        ::_snprintf_s(out, outSize, _TRUNCATE, "%s", path);
        return;
    }

    bool xpack = false;
    if (::dice::hfe::classManager)
    {
        // The original reads g_Renderer, which BF2 publishes into our own global.
        if (g_Renderer)
        {
            xpack = static_cast<IRenderer*>(g_Renderer)->isExpansionPack();
        }
    }
    ::_snprintf_s(out, outSize, _TRUNCATE, "%s%s", xpack ? "mods/xpack/" : "mods/bf2/", path);
}

#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
// The editor's renderer only [CHOSEN, not recovered]: the game's 1.50 StaticMesh.fx comments out its include of
// StaticMesh_lightmapgen.fx, the lightmapGeneration techniques the lightmap generator draws static meshes with as
// occluders, so terrain lightmaps would carry no object shadows; the retail editor's own StaticMesh.fx included it.
// True when `path` names StaticMesh.fx and its source has no include of that file outside a comment.
bool needsLightmapGenerationInclude(const char* path, const char* source, unsigned int size)
{
    const char* const slash = ::strrchr(path, '/');
    if (::_stricmp(slash ? slash + 1 : path, "StaticMesh.fx") != 0)
        return false;
    const char needle[] = "StaticMesh_lightmapgen.fx";
    bool block = false, line = false;
    for (unsigned int i = 0; i < size; ++i)
    {
        const char c = source[i];
        const char next = i + 1 < size ? source[i + 1] : '\0';
        if (block)
        {
            if (c == '*' && next == '/')
            {
                block = false;
                ++i;
            }
        }
        else if (line)
        {
            if (c == '\n')
                line = false;
        }
        else if (c == '/' && next == '*')
        {
            block = true;
            ++i;
        }
        else if (c == '/' && next == '/')
            line = true;
        else if (c == 'S' && size - i >= sizeof(needle) - 1 && ::_strnicmp(source + i, needle, sizeof(needle) - 1) == 0)
            return false;
    }
    return true;
}
#endif

} // anonymous namespace


// Loads and compiles an effect using ShaderManager definitions and its include resolver.
// The macro array ends with a null Name only. File-open failure logs and returns null;
// this build does not show the compatibility runtime's retry dialog.

void* compileEffectFromFile(const char* rawPath)
{
    RD9_TRACE_AT("dice::hfe::dx9rend::compileEffectFromFile", 0, 0x00);

    char path[512];
    applyModPrefix(path, sizeof(path), rawPath);

    ::renddx9abi::EngineFileData file = ::renddx9abi::readEngineFile(path);
    if (!file.ok())
    {
        ::renddx9abi::logf("compileEffectFromFile(\"%s\"): NOT FOUND through the FileManager. The "
                           "original raises a modal retry box here and exit(99)s on cancel; this "
                           "build logs and fails closed.", path);
        return 0;
    }

    // GSLoadObfuscated, read once and cached by the original. Its default is true.
    static int s_loadObfuscated = -1;
    if (s_loadObfuscated < 0)
        s_loadObfuscated = readBoolSetting("GSLoadObfuscated", true) ? 1 : 0;
    if (s_loadObfuscated)
        deobfuscateShader(file.data, file.size);

    // The macro array, flattened from the ShaderManager's vector of 28-byte strings.
    const char* macros[300];
    unsigned int macroCount = 0;
    if (g_shaderManager)
    {
        ShaderManagerDefines defines = shaderManagerDefines(g_shaderManager);
        for (unsigned int i = 0; i < defines.count && macroCount < 299; ++i)
            macros[macroCount++] = defines.at(i);
    }
    macros[macroCount] = 0;      // the single terminating dword - see the note above

    // Save the source size before freeEngineFile clears the structure.
    const unsigned int sourceBytes = file.size;

    ID3DXEffect* effect = 0;
    ID3DXBuffer* errors = 0;

    void*        device = renderDevice();
    void*        include = shaderManagerInclude(g_shaderManager);
    unsigned long flags = shaderManagerEffectFlags(g_shaderManager);

#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    if (device && needsLightmapGenerationInclude(path, reinterpret_cast<const char*>(file.data), file.size))
    {
        std::string extended(reinterpret_cast<const char*>(file.data), file.size);
        extended += "\n#include \"mods/xpack/Shaders/StaticMesh_lightmapgen.fx\"\n";
        ID3DXBuffer* extendedErrors = 0;
        const long extendedHr = D3DXCreateEffect(static_cast<IDirect3DDevice9*>(device), extended.data(),
                                                 static_cast<unsigned int>(extended.size()),
                                                 reinterpret_cast<const D3DXMACRO*>(macros),
                                                 static_cast<ID3DXInclude*>(include), flags, 0,
                                                 &effect, &extendedErrors);
        if (extendedHr >= 0 && effect)
        {
            ::renddx9abi::freeEngineFile(file);
            ::renddx9abi::logf("compileEffectFromFile(\"%s\"): COMPILED - effect=0x%08X from %u bytes with %u macro pairs, "
                               "the editor's lightmap-generation include added", path, ::renddx9abi::addr(effect),
                               sourceBytes, macroCount / 2);
            return effect;
        }
        effect = 0;
        ::renddx9abi::logf("compileEffectFromFile(\"%s\"): the editor's lightmap-generation include did not compile "
                           "(hr=0x%08X); the shipped source is compiled", path, static_cast<unsigned>(extendedHr));
    }
#endif

    const long hr = device
        ? D3DXCreateEffect(static_cast<IDirect3DDevice9*>(device), file.data, file.size,
                           reinterpret_cast<const D3DXMACRO*>(macros),
                           static_cast<ID3DXInclude*>(include), flags, 0, &effect, &errors)
        : -1;

    ::renddx9abi::freeEngineFile(file);

    if (hr < 0 || !effect)
    {
        ::renddx9abi::logf("compileEffectFromFile(\"%s\"): D3DXCreateEffect FAILED hr=0x%08X "
                           "(%u bytes, %u defines = %u macro pairs, include=0x%08X flags=0x%X)",
                           path, static_cast<unsigned>(hr), sourceBytes, macroCount, macroCount / 2,
                           ::renddx9abi::addr(include), flags);
        return 0;
    }

    ::renddx9abi::logf("compileEffectFromFile(\"%s\"): COMPILED - effect=0x%08X from %u bytes with "
                       "%u macro pairs", path, ::renddx9abi::addr(effect), sourceBytes,
                       macroCount / 2);
    return effect;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
