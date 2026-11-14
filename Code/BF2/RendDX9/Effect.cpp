// Effect techniques and semantic-keyed parameter handles.
// Handles can exist before the compiled effect supplies their reflected parameters.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "../../RendDX9Abi/EngineFile.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "RendDX9Base.h"
#include "ShaderParameter.h"
#include "../RendMisc/IRenderer.h"
#include "../GeomMisc/Debug.h"

#include <new>
#include <map>
#include <string>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <cstdlib>
#if defined(_WIN32)
#include <dxerr9.h>
#pragma comment(lib, "dxerr9.lib")
#endif

namespace dice {
namespace hfe {
namespace dx9rend {

extern void* g_shaderManager;   // ShaderManager.cpp
extern void* g_stateManager;    // StateManager.cpp - dword_1023B984
void shaderManagerSetCurrentEffect(void* effect);
unsigned char shaderManagerStateManagerDisabled(void* shaderManager);   // its +0xA4 byte
void vertexDeclarationBind(void* wrapper);          // BufferManager.cpp - sub_100147C0
void* vertexDeclarationCreate(const void* elements);
void vertexDeclarationRelease(void* wrapper);        // inline decrement + 10014790
int vertexDeclarationTypeSize(unsigned int type);    // RendDX9Types.cpp - 1000CC30
void stateManagerPush();
void stateManagerPop();
extern IBase* g_Renderer;       // BF2Render.cpp
static ID3DXEffect* compileEffectFile(const char* path, ID3DXBuffer** errors);


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

    // PE 0x1001D740.
    EffectTechnique()
        : handle(0), declaration(0), restore(1), passes(0), states(0), validated(false) {}
    // PE 0x1001F1B0.
    ~EffectTechnique()
    {
        if (declaration) vertexDeclarationRelease(declaration);
        delete states;
    }
};

namespace {

extern void* const kEffectVtable[4];

int effectRetryDialog(const char* method, const void* self,
                      const char* text, const char* caption)
{
#if defined(_WIN32)
    (void)method; (void)self;
    return ::MessageBoxA(0, text, caption, 0x50015);
#else
    // [CHOSEN, not recovered] No Win32 modal UI: choose IDCANCEL (2). PE 0x1001E863,
    // 0x1001E94D and 0x1001EF8D continue on Cancel; 0x1001E0C6 exits with code 99.
    // All four dialogs use MB_RETRYCANCEL (0x50015), never an invented success.
    (void)self;
    std::fprintf(stderr, "%s: %s: %s (Win32 retry dialog unavailable; Cancel)\n",
                 method, caption, text);
    std::fflush(stderr);
    return 2;
#endif
}

// PE 0x1001D820; path expansion is sub_10029D00 and is conditional on the renderer.
std::string effectSourcePath(const std::string& name)
{
    std::string path(name);
    if (g_Renderer && !::strstr(path.c_str(), "mods/"))
    {
        static std::string expanded; // dword_1023D488
        expanded = static_cast<IRenderer*>(g_Renderer)->isExpansionPack()
            ? "mods/xpack/" : "mods/bf2/";
        expanded += path;
        path = expanded;
    }
    return path;
}

const char* effectErrorDescription(long hr)
{
#if defined(_WIN32)
    return ::DXGetErrorDescription9A(hr); // PE 0x1007102E, upstream DXErr9
#else
    // [CHOSEN, not recovered] DXErr9 is a Windows SDK library. Keep the HRESULT in
    // the native diagnostic accompanying the unsupported Win32 retry dialog.
    static char description[32];
    std::snprintf(description, sizeof(description), "HRESULT 0x%08X", static_cast<unsigned int>(hr));
    return description;
#endif
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

// PE 0x100219E0 (listing read). The constructor deliberately has no owner-null guard;
// the destructor does.  Every remaining method is one typed virtual forward, matching the ten-byte
// 0x10021910..0x10021990 tail-dispatch family without source-level vtable indexing.
ShaderParameter::ShaderParameter(ParameterOwner* owner)
    : m_refCount(1)
    , m_owner(owner)
{
    m_owner->addRef();
}

// PE 0x100218C0.
ShaderParameter::~ShaderParameter()
{
    if (m_owner)
        m_owner->release();
    m_owner = 0;
}

int ShaderParameter::getRef() const { return m_refCount; }
int ShaderParameter::addRef() { return ++m_refCount; }

// PE 0x100222B0.
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
// PE 0x10021920.
void ShaderParameter::setD3DXParam(void* handle) { m_owner->setD3DXParam(handle); }
bool ShaderParameter::isParameterUsed() const { return m_owner->isParameterUsed(); }
// PE 0x10021940.
void ShaderParameter::setValue(const void* data, unsigned int count)
{
    m_owner->setValue(data, count);
}
// PE 0x10021950.
void ShaderParameter::setBool(unsigned char value) { m_owner->setBool(value); }
// PE 0x10021960.
void ShaderParameter::setInt(int value) { m_owner->setInt(value); }
// PE 0x10021970.
void ShaderParameter::setFloat(float value) { m_owner->setFloat(value); }
// PE 0x10021980.
void ShaderParameter::setMatrixPointerArray(const void* const* matrices, unsigned int count)
{
    m_owner->setMatrixPointerArray(matrices, count);
}
unsigned int ShaderParameter::getTypeCode() const { return m_owner->getTypeCode(); }

void ShaderParameter::setOwner(ParameterOwner* owner)
{
    if (m_owner)
        m_owner->release();
    m_owner = owner;
    m_owner->addRef();
}

// One queued value of the write queue at Effect +0x60 (0x830-byte node: the 28-byte semantic key,
// then 0x800 value bytes and their size at +0x800, as sub_1001E570 writes and sub_1001E2E0 reads).
struct EffectPendingValue
{
    unsigned char data[0x800];   // GetValue's buffer
    unsigned int  bytes;         // +0x800, the parameter's D3DXPARAMETER_DESC::Bytes
};

// Reference-counted effect wrapper with a four-slot IBase interface.
// Caches validated techniques and named parameters. Values set before binding are copied
// into a pending queue, then applied and removed when the effect is bound.
// Native technique containers are private; their layout need not match the retail object.
class Effect
{
public:
    Effect();
    ~Effect();

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;

    // sub_1001E3A0, read in full. See the body.
    void* getShaderParameter(const std::string& name);

    bool         bind(ID3DXEffect* d3dxEffect);
    bool         reload(const std::string& path);
    void         populateParameters();
    void         rebindParameters();
    void         snapshotParameterValues();
    void         resetTechniqueRecords();
    void         flushPendingValues();
    int          validateTechniques();
    int          validateTechnique(const std::string& name);
    int          linkStateTechnique(const std::string& n);
    bool         readDeclaration(D3DXHANDLE technique, void** out);
    bool         isParameterUsed(void* handle) const;

    bool         setTechniqueByName(const std::string& name);
    bool         setTechniqueRecord(unsigned char* record);
    unsigned int begin();
    void         beginPass(unsigned int index);
    void         end();

    ID3DXEffect* d3dxEffect() const { return m_d3dxEffect; }
    const std::string& name() const { return m_string0C; }
    typedef std::map<std::string, EffectTechnique> TechniqueMap;
    TechniqueMap& techniqueMap() { return m_techniques; }
    const std::string& currentTechniqueName() const { return m_string38; }
    unsigned int currentTechniquePassCount() const
    {
        return m_currentTechnique ? m_currentTechnique->passes : 0u;
    }

    // sub_1001DB60 / sub_1001DB70 read +0x34 - the current technique record - directly; this is
    // that read for the two seams below.
    unsigned char* currentTechniqueRecord() const
    { return reinterpret_cast<unsigned char*>(m_currentTechnique); }

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
    std::string     m_string0C;      // +0x0C  the effect's name
    // Native private state from here on: subsequent comments denote retail offsets only.
    // No external reader uses these offsets; opaque technique pointers round-trip here.
    TechniqueMap    m_techniques;    // retail +0x28
    EffectTechnique* m_currentTechnique;   // retail +0x34
    std::string     m_string38;      // +0x38  the current technique's name, copied on selection
    // +0x54: semantic -> its ShaderParameter handle (strcmp order; null caches a failed allocation).
    std::map<std::string, ShaderParameter*>   m_parameters;
    std::map<std::string, EffectPendingValue> m_pendingValues;   // +0x60  the write queue
    unsigned char    m_byte6C;        // +0x6C
    unsigned char    m_pad6D[3];
    unsigned int     m_dword70;       // +0x70
    unsigned char    m_byte74;        // +0x74  ctor: 1
    unsigned char    m_pad75[3];
    std::vector<std::string> m_stringVec; // retail +0x78, state-technique names
};


// PE 0x1001ED90.
Effect::Effect()
    : m_vptr(const_cast<void**>(kEffectVtable))
    , m_refCount(1)
    , m_d3dxEffect(0)
    , m_currentTechnique(0)
    , m_byte6C(0)
    , m_dword70(0)
    , m_byte74(1)
{
    // The parameter map and the write queue are constructed empty with the members.

    // The shared owner. First Effect creates it; the rest AddRef it.
    if (g_parameterOwner)
        g_parameterOwner->addRef();
    else
    {
        void* const storage = ::renddx9abi::engineAlloc(sizeof(DefaultParameterOwner));
        g_parameterOwner = storage ? new (storage) DefaultParameterOwner() : 0;
    }
}

// PE 0x1001EBE0 (also 0x1001DF70). The parameter and compiled-effect cleanup is the separate 0x1001DF70 body, inlined here.
Effect::~Effect()
{
    m_vptr = const_cast<void**>(kEffectVtable);
    if (g_parameterOwner && !g_parameterOwner->release())
        g_parameterOwner = 0;
    // PE 0x1001DF70, inlined parameter and compiled-effect cleanup.
    for (std::map<std::string, ShaderParameter*>::iterator node = m_parameters.begin();
         node != m_parameters.end(); ++node)
        node->second->release();
    m_parameters.clear();
    if (m_d3dxEffect)
    {
        m_d3dxEffect->Release();
        m_d3dxEffect = 0;
    }
}

// PE 0x10021160
int Effect::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~Effect();
        ::renddx9abi::engineFree(this);
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

void* Effect::getShaderParameter(const std::string& name)
{
    // The find, and then the create-and-cache.
    const std::map<std::string, ShaderParameter*>::const_iterator node = m_parameters.find(name);
    if (node != m_parameters.end())
        return node->second;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(ShaderParameter));
    ShaderParameter* const p = storage ? new (storage) ShaderParameter(g_parameterOwner) : 0;

    // Cache allocation failures as null entries; later lookups do not retry.
    m_parameters.insert(std::make_pair(name, p));

    return p;
}


// Binds the supplied compiled effect, replacing its previous device state and parameter bindings.
// Returns false for a null wrapper.

bool effectBind(Effect* self, void* d3dxEffect)
{
    RD9_TRACE_AT("dice::hfe::dx9rend::Effect::bind", self, 0x00);
    return self ? self->bind(static_cast<ID3DXEffect*>(d3dxEffect)) : false;
}

// PE 0x1001ECB0 (listing read).
bool Effect::bind(ID3DXEffect* d3dxEffect)
{
    // A rebind (the ShaderManager reload sweep) first keeps every parameter's current value
    // (sub_1001E570), then lets the outgoing effect go: OnLostDevice (+0x114) and Release.
    if (m_d3dxEffect)
    {
        snapshotParameterValues();
        m_d3dxEffect->OnLostDevice();
        m_d3dxEffect->Release();
        m_d3dxEffect = 0;
    }

    // sub_1001DE30 - every cached technique record (and its state-technique chain) is reset;
    // the records stay in the map, so selected-record pointers remain valid.
    resetTechniqueRecords();

    if (!d3dxEffect)
        return false;

    d3dxEffect->AddRef();
    m_d3dxEffect = d3dxEffect;

    // when the ShaderManager's +0xA4 byte is clear -
    // and its constructor leaves it clear - every freshly bound effect is handed the StateManager
    // through ID3DXEffect::SetStateManager (fx vtbl +0x11C). From here on, every state a pass sets
    // routes through dword_1023B984 instead of the device.
    if (g_shaderManager
        && !shaderManagerStateManagerDisabled(g_shaderManager)
        && g_stateManager)
    {
        d3dxEffect->SetStateManager(static_cast<ID3DXEffectStateManager*>(g_stateManager));
    }

    // 0x1001ED0F: handles already in the registry are rebound by semantic (sub_1001DD50);
    // an empty registry is populated from the effect (sub_1001E440).
    if (!m_parameters.empty())
        rebindParameters();
    else
        populateParameters();

    flushPendingValues();

    const int verdict = validateTechniques();

    if (verdict == 3)
        return false;

    // 0x1001ED3E..0x1001ED7E: reselect the technique selected before (by its name at +0x38), and
    // select the first record when nothing is selected.
    if (!m_string38.empty())
    {
        const TechniqueMap::iterator node = m_techniques.find(m_string38);
        if (node != m_techniques.end())
            setTechniqueRecord(reinterpret_cast<unsigned char*>(&node->second));
    }
    if (!m_currentTechnique && !m_techniques.empty())
        setTechniqueRecord(reinterpret_cast<unsigned char*>(&m_techniques.begin()->second));
    return true;
}

// sub_1001E570 - for every registered parameter, its current value (up to 0x800 bytes) and its
// byte size go into the write queue under the parameter's semantic.
// PE 0x1001E570.
void Effect::snapshotParameterValues()
{
    for (std::map<std::string, ShaderParameter*>::iterator node = m_parameters.begin();
         node != m_parameters.end(); ++node)
    {
        D3DXHANDLE handle = ::renddx9abi::d3dxHandle(node->second->getD3DXParam());
        // [CHOSEN, not recovered] zeroed: retail reads Bytes from an uninitialised description
        // when GetParameterDesc fails (an unbound handle).
        D3DXPARAMETER_DESC desc;
        ::memset(&desc, 0, sizeof(desc));
        m_d3dxEffect->GetParameterDesc(handle, &desc);                  // +0x10
        EffectPendingValue& value = m_pendingValues[node->first];      // sub_10020D40
        value.bytes = desc.Bytes;                                       // +0x800
        unsigned char* const data = value.data;
        handle = ::renddx9abi::d3dxHandle(node->second->getD3DXParam());
        m_d3dxEffect->GetValue(handle, data, 0x800);                    // +0x54
    }
}

// sub_1001DE30 / sub_1001F200 - per record: handle, restore, pass count and the validated flag
// cleared, the declaration released; then the same for its state-technique link.
// PE 0x1001F200: the per-record reset, inlined into the loop below.
void Effect::resetTechniqueRecords()
{
    for (TechniqueMap::iterator node = m_techniques.begin(); node != m_techniques.end(); ++node)
    {
        for (EffectTechnique* record = &node->second; record; record = record->states)
        {
            record->handle = 0;                                         // +0x1C
            record->restore = 0;                                        // +0x24
            record->passes = 0;                                         // +0x28
            record->validated = false;                                  // +0x30
            if (record->declaration)
                vertexDeclarationRelease(record->declaration);          // +0x20
            record->declaration = 0;
        }
    }
}

// sub_1001DD50 - every registered handle gets a new owner for its semantic in the bound effect:
// an FX parameter when the effect has one, the shared unbound owner otherwise. Then the cached
// "ShaderVersion" is marked for re-reading.
void Effect::rebindParameters()
{
    for (std::map<std::string, ShaderParameter*>::iterator node = m_parameters.begin();
         node != m_parameters.end(); ++node)
    {
        ShaderParameter* const handle = node->second;
        const D3DXHANDLE h = m_d3dxEffect->GetParameterBySemantic(0, node->first.c_str());   // +0x28
        ParameterOwner* owner;
        if (h)
        {
            owner = createFXParameter(this, ::renddx9abi::d3dxHandleValue(h));
        }
        else
        {
            owner = g_parameterOwner;
            owner->addRef();
        }
        handle->setOwner(owner);                                        // sub_100218E0
        owner->release();
    }
    m_byte74 = 1;
}


// Adds or rebinds parameters keyed by semantic, skipping parameters without one.
// Each registry handle owns its parameter implementation.
// PE 0x1001E440.
void Effect::populateParameters()
{
    for (unsigned int i = 0; ; ++i)
    {
        D3DXHANDLE h = m_d3dxEffect->GetParameter(0, i);
        if (!h)
            break;

        D3DXPARAMETER_DESC desc;
        // Shipped defect: failure leaves the descriptor uninitialized.
        m_d3dxEffect->GetParameterDesc(h, &desc);

        if (!desc.Semantic)
            continue;                       // no semantic - not registered at all

        const std::map<std::string, ShaderParameter*>::const_iterator node =
            m_parameters.find(desc.Semantic);
        if (node != m_parameters.end())
        {
            ShaderParameter* const existing = node->second;
            existing->setD3DXParam(::renddx9abi::d3dxHandleValue(h));
            continue;
        }

        ParameterOwner* const owner = createFXParameter(this, ::renddx9abi::d3dxHandleValue(h));

        void* const storage = ::renddx9abi::engineAlloc(sizeof(ShaderParameter));
        ShaderParameter* const handle = storage ? new (storage) ShaderParameter(owner) : 0;
        // PE 0x1001E4DB releases unconditionally. The ShaderParameter constructor first took its
        // own reference when allocation succeeded, leaving that handle as the sole owner.
        owner->release();

        m_parameters[std::string(desc.Semantic)] = handle;
    }
}


// Applies queued parameter values through their registered handlers, then clears the queue.
// PE 0x1001E2E0.
void Effect::flushPendingValues()
{
    // sub_1001E2E0 - each queued value goes to the registered parameter of the same semantic
    // (SetValue, +0x50, with the stored byte size); values without a parameter are dropped.
    for (std::map<std::string, EffectPendingValue>::iterator node = m_pendingValues.begin();
         node != m_pendingValues.end(); ++node)
    {
        const std::map<std::string, ShaderParameter*>::iterator parameter =
            m_parameters.find(node->first);
        if (parameter != m_parameters.end())
        {
            const unsigned int bytes = node->second.bytes;
            const unsigned char* const data = node->second.data;
            const D3DXHANDLE handle =
                ::renddx9abi::d3dxHandle(parameter->second->getD3DXParam());
            m_d3dxEffect->SetValue(handle, data, bytes);
        }
    }
    m_pendingValues.clear();
}


// Validates every technique and queued technique name. Returns 3 when the user requests
// Retry after a failure; returns 2 on completion. These are verdict codes, not Booleans.
// PE 0x1001EB40 (listing read).
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
        linkStateTechnique(m_stringVec[i]);
    return 2;
}


// PE 0x1001E630
// GSMaxValidation is cached once. Native platforms log and choose Cancel for its dialogs.
// Name and validated flag are written before lookup; non-retry failures retain
// validated=true, and success means HRESULT == 0 (not merely succeeded(hr)).
int Effect::validateTechnique(const std::string& name)
{
    // It is the first strstr match, not a generic suffix comparison.
    const char* suffix = ::strstr(name.c_str(), "_States");
    if (suffix && suffix + 7 == name.c_str() + ::strlen(name.c_str()))
        m_stringVec.push_back(name);

    EffectTechnique& record = m_techniques[name];
    record.name = name;
    record.validated = true;
    static const bool maxValidation = readBoolSetting("GSMaxValidation", false);

    D3DXHANDLE h = m_d3dxEffect->GetTechniqueByName(name.c_str());
    if (!h)
    {
        if (maxValidation)
        {
            const std::string caption = std::string("Technique not found: ") + name;
            const std::string text = std::string("Effect: ") + effectSourcePath(m_string0C);
            if (effectRetryDialog("dice::hfe::dx9rend::Effect::validateTechnique (1001E863 missing-technique retry dialog)",
                                  this, text.c_str(), caption.c_str()) == 4)
                return 3;
        }
        return 0;
    }

    D3DXTECHNIQUE_DESC desc; // Ignored HRESULT; retail does not initialize this output.
    m_d3dxEffect->GetTechniqueDesc(h, &desc);
    const long hr = m_d3dxEffect->ValidateTechnique(h);
    if (hr != 0)
    {
        if (maxValidation)
        {
            const std::string text = std::string("Error: ") + effectErrorDescription(hr);
            const std::string caption = std::string("Failed to validate technique: ") + desc.Name
                + ", in effect: " + effectSourcePath(m_string0C);
            if (effectRetryDialog("dice::hfe::dx9rend::Effect::validateTechnique (1001E94D validation retry dialog)",
                                  this, text.c_str(), caption.c_str()) == 4)
            {
                record.validated = false;
                return 3;
            }
        }
        return 1;
    }

    record.handle = ::renddx9abi::d3dxHandleValue(h);
    record.passes = desc.Passes;
    readDeclaration(h, &record.declaration);
    return 2;
}

// PE 0x1001E9D0 (listing read: the retail argument is a std::string reference, and the validation pass
// hands it the vector element itself). The base name ends at the first "_States". A missing or invalid
// base is validated again, with no end/null guard after that lookup. The linked
// record keeps constructor defaults for its pass count and declaration.
int Effect::linkStateTechnique(const std::string& name)
{
    D3DXHANDLE h = m_d3dxEffect->GetTechniqueByName(name.c_str());
    if (!h) return 0;
    D3DXTECHNIQUE_DESC desc;
    m_d3dxEffect->GetTechniqueDesc(h, &desc);
    std::string baseName(name);
    baseName.resize(::strstr(name.c_str(), "_States") - name.c_str());
    TechniqueMap::iterator base = m_techniques.find(baseName);
    if (base == m_techniques.end() || !base->second.validated)
    {
        if (validateTechnique(baseName) == 3) return 3;
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

// PE 0x1001D870 (listing read).
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

// The effect's +0x0C name, for ShaderManager's nesting diagnostic. No null test.
const std::string& effectName(void* effect)
{
    return static_cast<Effect*>(effect)->name();
}

// PE 0x1001D9B0 (listing read).
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

bool effectSetTechniqueByName(void* effect, const std::string& name)
{
    return effect ? static_cast<Effect*>(effect)->setTechniqueByName(name) : false;
}

// The current technique's name (+0x38), copied there by every technique selection; FSQuadDrawer's
// stencil passes (PE 0x10085D30, 0x10085E70, 0x10086030) copy it to reselect it afterwards.
const std::string& effectCurrentTechniqueName(void* effect)
{
    return static_cast<Effect*>(effect)->currentTechniqueName();
}

// sub_1001DC20 - the constructor-side probe FSQuadDrawer uses before selecting "Blit" or its
// "Blit_1_4" fallback.  A cached record is usable only when its D3DX technique handle is non-null.
bool effectHasTechnique(void* effect, const std::string& name)
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

// PE 0x1001DC60.
bool Effect::setTechniqueByName(const std::string& name)
{
    TechniqueMap::iterator node = m_techniques.find(name);
    if (node == m_techniques.end())
    {
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\Effect.cpp",
            299, "") << "Technique not found " << name << " in effect " << m_string0C;
        return false;
    }

    return setTechniqueRecord(reinterpret_cast<unsigned char*>(&node->second));
}


// Begins the effect and returns its pass count, using StateManager-owned state preservation.
// beginPass closes any previous pass; end closes a pending pass and clears the current effect.

// PE 0x1001D780
unsigned int Effect::begin()
{
    unsigned int passes; // Retail ignores Begin's HRESULT and does not initialize the output.
    const bool smDisabled = shaderManagerStateManagerDisabled(g_shaderManager) != 0;
#if !defined(_WIN32)
    // Retail crash path, not missing code. The body above and below is complete; PE
    // 0x1001D793..0x1001D796 loads the D3DX effect and calls through it without a null check, so a
    // null effect faults inside retail's Effect::begin. Under the memory-safety exception
    // (docs/macos-port.md) that null dereference is not reproduced: no retail pass
    // count exists to return, so the native build reports and stops where retail faults.
    // [CHOSEN, not recovered] the report text; it fires only when native effect compilation
    // failed and left the effect unbound.
    if (!m_d3dxEffect)
    {
        std::fprintf(stderr, "dice::hfe::dx9rend::Effect::begin: fatal unbound D3DX effect "
                             "(retail null dereference at PE 0x1001D796); inspect compiler diagnostics\n");
        std::fflush(stderr);
        std::abort();
    }
#endif
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


// PE 0x1001DBF0 (listing read) - find a technique record by name: the map lookup, then null on a miss or the
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
    static_cast<Effect*>(effect)->setTechniqueRecord(static_cast<unsigned char*>(record));
}

// PE 0x1001DB80.
bool Effect::setTechniqueRecord(unsigned char* record)
{
    EffectTechnique* technique = reinterpret_cast<EffectTechnique*>(record);
    m_currentTechnique = technique;
    m_string38 = technique->name;
    m_d3dxEffect->SetTechnique(::renddx9abi::d3dxHandle(m_currentTechnique->handle));
    return true;
}

// The record's +44 link points to the state technique.
// Names are [INFERRED]. Opaque pointers now resolve to native private records.
// D3DX calls and their ignored HRESULTs follow the PE exactly.
// PE 0x1001DA00.
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
// PE 0x1001DA80.
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
// PE 0x1001DE90.
void effectBeginTechniqueStates(void* effect, const std::string& name)
{
    Effect* self = static_cast<Effect*>(effect); // no null-this guard in retail
    Effect::TechniqueMap::iterator found = self->techniqueMap().find(name);
    if (found != self->techniqueMap().end())
        effectBeginTechniqueStateRecord(self, reinterpret_cast<unsigned char*>(&found->second));
}
// PE 0x1001DEC0.
void effectEndTechniqueStates(void* effect, const std::string& name)
{
    Effect* self = static_cast<Effect*>(effect);
    Effect::TechniqueMap::iterator found = self->techniqueMap().find(name);
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

void RD9_THUNK effect_addRef(void* s RD9_THUNK_EDX) { static_cast<Effect*>(s)->addRef(); }
int  RD9_THUNK effect_getRef(void* s RD9_THUNK_EDX) { return static_cast<Effect*>(s)->getRef(); }
int  RD9_THUNK effect_release(void* s RD9_THUNK_EDX) { return static_cast<Effect*>(s)->release(); }
void* RD9_THUNK effect_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
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

// PE 0x1001EE90
// Recompiles in place; compile failures and GSMaxValidation messages enter the retry dialog.
bool Effect::reload(const std::string& path)
{
    m_string0C = path;                                                  // +0x0C assign
    static const bool maxValidation = readBoolSetting("GSMaxValidation", false);   // byte_1023C6C4
    ID3DXBuffer* errors = 0;
    for (;;)
    {
        ID3DXEffect* const fx = compileEffectFile(m_string0C.c_str(), &errors);
        if (!fx || (maxValidation && errors))
        {
            const std::string caption = std::string("Errors in effect: ") + m_string0C;
            // shipped defect: compilation failure dereferences even a null error buffer.
            const char* text = static_cast<const char*>(errors->GetBufferPointer());
            const bool retry = effectRetryDialog("dice::hfe::dx9rend::Effect::reload (1001EF8D compiler retry dialog)",
                                                 this, text, caption.c_str()) == 4;
            errors->Release();
            if (retry)
            {
                if (fx) fx->Release();
                continue;
            }
        }
        if (!fx)
            return false;
        const bool bound = bind(fx);                                    // sub_1001ECB0
        fx->Release();
        if (bound)
            return true;
    }
}

bool effectReload(Effect* effect, const std::string& path)
{
    return effect->reload(path);
}

Effect* createEffectObject()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(Effect));
    if (!storage)
        return 0;
    Effect* self = new (storage) Effect();
    return self;
}


// Returns an owned parameter reference; the caller must release it.
// Requires a valid effect and an existing semantic. Neither pointer is null-checked.

ShaderParameter* effectGetShaderParameterTyped(Effect* self, const std::string& name)
{
    ShaderParameter* const handle = static_cast<ShaderParameter*>(self->getShaderParameter(name));
    handle->addRef();
    return handle;
}

void* effectGetShaderParameter(Effect* self, const std::string& name)
{
    return effectGetShaderParameterTyped(self, name);
}

// sub_1001E3A0 itself, exposed for the one FSQuadDrawer constructor member (stencilpass) that
// deliberately retains the Effect map's existing reference rather than taking the caller-facing
// extra reference sub_1001E420 adds for every other semantic.
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect* self,
                                                       const std::string& name)
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
// PE 0x1001DEF0.
void Effect::clearTextureParameters()
{
    for (std::map<std::string, ShaderParameter*>::const_iterator node = m_parameters.begin();
         node != m_parameters.end(); ++node)
    {
        ShaderParameter* const parameter = node->second;
        if (parameter->getTypeCode() & 0x800u)                                    // vtbl +0x2C
        {
            void* const handle = node->second->getD3DXParam();                    // +0x0C
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

// PE 0x10032F90 (listing read). Unreachable in 1.50: no code or data references. The std::string
// form of the same decode: the magic length is a function-local static seeded with 10, the string
// must be at least that long and start with the magic, and the decode then runs over every byte.
void deobfuscateShader(std::string& text)
{
    static const unsigned int magicLength = 10;
    if (text.size() < magicLength)
        return;
    deobfuscateShader(reinterpret_cast<unsigned char*>(&text[0]),
                      static_cast<unsigned int>(text.size()));
}

namespace {


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
// Adapter for callers that do not consume compiler messages.
void* compileEffectFromFile(const char* rawPath)
{
    ID3DXBuffer* errors = 0;
    ID3DXEffect* effect = compileEffectFile(rawPath, &errors);
    if (errors) errors->Release();
    return effect;
}

// PE 0x1001E010; the original member takes the error-buffer out parameter.
static ID3DXEffect* compileEffectFile(const char* rawPath, ID3DXBuffer** errors)
{
    RD9_TRACE_AT("dice::hfe::dx9rend::compileEffectFromFile", 0, 0x00);

    *errors = 0;
    const std::string sourcePath = effectSourcePath(rawPath);
    const char* const path = sourcePath.c_str();

    ::renddx9abi::EngineFileData file = ::renddx9abi::readEngineFile(path);
    while (!file.ok())
    {
        const std::string text = sourcePath
            + " not found!!! _DO_ check your working directory, _AND_ sync your shaders folder "
              "before calling upon your local rendering programmer/GP. (really!)";
        if (effectRetryDialog("dice::hfe::dx9rend::Effect::compile (1001E0C6 missing-file retry dialog)",
                              0, text.c_str(), "Error!") == 2)
            std::exit(99);
        file = ::renddx9abi::readEngineFile(path);
    }

    // GSLoadObfuscated, read once and cached by the original. Its default is true.
    static int s_loadObfuscated = -1;
    if (s_loadObfuscated < 0)
        s_loadObfuscated = readBoolSetting("GSLoadObfuscated", true) ? 1 : 0;
    if (s_loadObfuscated)
        deobfuscateShader(file.data, file.size);

    // The macro array, flattened from the ShaderManager's vector of 28-byte strings.
    std::vector<const char*> macros;
    if (g_shaderManager)
    {
        ShaderManagerDefines defines = shaderManagerDefines(g_shaderManager);
        for (unsigned int i = 0; i < defines.count; ++i)
            macros.push_back(defines.at(i));
    }
    const unsigned int macroCount = static_cast<unsigned int>(macros.size());
    macros.push_back(0); // Retail appends only a null Name, not a complete D3DXMACRO.

    // Save the source size before freeEngineFile clears the structure.
    const unsigned int sourceBytes = file.size;

    ID3DXEffect* effect = 0;

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
                                                 reinterpret_cast<const D3DXMACRO*>(macros.data()),
                                                 static_cast<ID3DXInclude*>(include), flags, 0,
                                                 &effect, &extendedErrors);
        if (extendedHr >= 0 && effect)
        {
            *errors = extendedErrors;
            ::renddx9abi::freeEngineFile(file);
            ::renddx9abi::logf("compileEffectFromFile(\"%s\"): COMPILED - effect=0x%08X from %u bytes with %u macro pairs, "
                               "the editor's lightmap-generation include added", path, ::renddx9abi::addr(effect),
                               sourceBytes, macroCount / 2);
            return effect;
        }
        if (extendedErrors) extendedErrors->Release();
        effect = 0;
        ::renddx9abi::logf("compileEffectFromFile(\"%s\"): the editor's lightmap-generation include did not compile "
                           "(hr=0x%08X); the shipped source is compiled", path, static_cast<unsigned>(extendedHr));
    }
#endif

    const long hr = D3DXCreateEffect(static_cast<IDirect3DDevice9*>(device), file.data, file.size,
                                     reinterpret_cast<const D3DXMACRO*>(macros.data()),
                                     static_cast<ID3DXInclude*>(include), flags, 0, &effect, errors);

    ::renddx9abi::freeEngineFile(file);

    if (hr < 0 || !effect)
    {
        ::renddx9abi::logf("compileEffectFromFile(\"%s\"): D3DXCreateEffect FAILED hr=0x%08X "
                           "(%u bytes, %u defines = %u macro pairs, include=0x%08X flags=0x%X)",
                           path, static_cast<unsigned>(hr), sourceBytes, macroCount, macroCount / 2,
                           ::renddx9abi::addr(include), flags);
        return effect; // PE ignores D3DXCreateEffect's HRESULT and returns its output pointer.
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
