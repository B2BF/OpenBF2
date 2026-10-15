// Owns D3D query wrappers and recreates supported queries across device reset.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <set>
#include <stddef.h>

namespace dice {
namespace hfe {
namespace dx9rend {

// dword_1023B970 - the base object. Query::create reaches the device through it rather than being
// handed one, which is why this file needs the global and not a parameter. See RendDX9Init.cpp:671.
extern void* g_rendererBase;

// dword_1023B98C. Renderer::initialize loads it directly as the `this` for its five calls.
void* g_queryManager = 0;


// Manager-owned query with getRef, no-op, and addRef slots. There is no release slot;
// QueryManager tears down the wrapper.

class Query
{
public:
    Query();

    // Called by createQuery for a supported type, and again by onDeviceReset.
    void create(int type, bool occlusionSupported);

    int  getRef() const { return m_refCount; }
    void addRef()       { ++m_refCount; }
    int  dropRef()      { return --m_refCount; }     // the holders' inline decrement (queryReleaseRef)

    // sub_10003FA0 - the destructor body, See the definition.
    void destroy();

    IDirect3DQuery9* d3dQuery() const { return m_d3dQuery; }
    int              type()     const { return m_type; }
    void             clearFence()     { m_field10 = 0; }
    void             releaseD3DQuery();
    // +0x19, the fence-issued flag (see queryIssueEnd / queryWaitDone).
    unsigned char&   issued()         { return m_field19; }

    // The native address of the field at a retail x86 byte offset; see queryFieldAt.
    unsigned char*   fieldAt(unsigned int retailOffset);

private:
    void*            m_vptr;        // +0x00  set by the constructor to the 3-slot table
    int              m_refCount;    // +0x04  ctor: 1
    IDirect3DQuery9* m_d3dQuery;    // +0x08  ctor: 0
    int              m_type;        // +0x0C  ctor: -1  (minus one, not zero)
    unsigned int     m_field10;     // +0x10  ctor: 0   [UNRESOLVED]; onDeviceReset zeroes it again
    unsigned int     m_field14;     // +0x14  ctor: 0   [UNRESOLVED]
    unsigned char    m_field18;     // +0x18  ctor: 0   [UNRESOLVED]
    unsigned char    m_field19;     // +0x19  ctor: 0   [UNRESOLVED]
    unsigned char    m_pad1A[2];    // +0x1A  the allocation is 0x1C
};

// Retail x86 evidence. createQuery allocates sizeof(Query); other TUs reach the fields through
// queryFieldAt, so the x64 object is native (issue #7).
#if defined(_M_IX86)
static_assert(sizeof(Query) == 0x1C, "sub_10004420 allocates 0x1C - see 0x10004428");
#endif

// The three slots. Free functions with an explicit ECX/EDX pair, because MSVC will not let a free
// function be __thiscall and the table has to hold real entries rather than a null.
namespace {

int  RD9_THUNK query_getRef(void* self RD9_THUNK_EDX)  { return static_cast<Query*>(self)->getRef(); }
void RD9_THUNK query_nullsub(void* RD9_THUNK_EDX)      { /* nullsub_53 - empty in the original too */ }
void RD9_THUNK query_addRef(void* self RD9_THUNK_EDX)  { static_cast<Query*>(self)->addRef(); }

void* const kQueryVtable[3] =
{
    reinterpret_cast<void*>(&query_getRef),    // +0x00
    reinterpret_cast<void*>(&query_nullsub),   // +0x04
    reinterpret_cast<void*>(&query_addRef)     // +0x08
};

} // anonymous namespace

// sub_10004170, read in full.
Query::Query()
    : m_vptr(const_cast<void**>(kQueryVtable))
    , m_refCount(1)
    , m_d3dQuery(0)
    , m_type(-1)
    , m_field10(0)
    , m_field14(0)
    , m_field18(0)
    , m_field19(0)
{
    m_pad1A[0] = 0;
    m_pad1A[1] = 0;
}

void Query::releaseD3DQuery()
{
    if (m_d3dQuery)
        m_d3dQuery->Release();
    m_d3dQuery = 0;
}


// Replaces the D3D query when supported; unsupported occlusion records only its type.
// Requires the published renderer and device.

void Query::create(int type, bool occlusionSupported)
{
    RD9_TRACE_AT("dice::hfe::dx9rend::Query::create", this, 0x00);

    if (occlusionSupported || type != D3DQUERYTYPE_OCCLUSION)
    {
        releaseD3DQuery();

        RendDX9Base* base = reinterpret_cast<RendDX9Base*>(g_rendererBase);
        if (base && base->device)
            base->device->CreateQuery(static_cast<D3DQUERYTYPE>(type), &m_d3dQuery);

        m_type = type;
    }
    else
    {
        m_type = D3DQUERYTYPE_OCCLUSION;
    }
}


// Owns live device queries and supported query types, and listens for device resets.
// Its only interface is DeviceLostResetListener; it is not an IBase singleton.

class QueryManager
{
public:
    explicit QueryManager(IDirect3DDevice9* device);

    // Renderer::initialize calls it five times with D3DQUERYTYPE_EVENT.
    Query* createQuery(int type);

    // sub_10004490 - take a query out of the live set (Query::destroy's last step).
    void removeQuery(Query* query);

    void onDeviceLost();     // +0x00
    void onDeviceReset();    // +0x04
    // +0x08 is nullsub_21 - empty in the original; see kQueryManagerVtable.

private:
    void*                  m_vptr;              // +0x00
    std::set<Query*>       m_liveQueries;       // +0x04
    // Retail std::set<int>; the keys are compared unsigned, as the recovered lookups do.
    std::set<unsigned int> m_supportedTypes;    // +0x10
    unsigned char          m_occlusionOk;       // +0x1C  ctor: 1
    unsigned char          m_flag1D;            // +0x1D  ctor: 1  [UNRESOLVED] what it gates
    unsigned char          m_pad1E[2];          // +0x1E
};

// RendDX9Init.cpp:249 allocates sizeof(QueryManager) (retail: 32, the bf_malloc at 0x1000B41E).

namespace {

void RD9_THUNK qm_onDeviceLost(void* self RD9_THUNK_EDX)  { static_cast<QueryManager*>(self)->onDeviceLost(); }
void RD9_THUNK qm_onDeviceReset(void* self RD9_THUNK_EDX) { static_cast<QueryManager*>(self)->onDeviceReset(); }
void RD9_THUNK qm_nullsub(void* RD9_THUNK_EDX)            { /* nullsub_21 - empty in the original too */ }

void* const kQueryManagerVtable[3] =
{
    reinterpret_cast<void*>(&qm_onDeviceLost),   // +0x00
    reinterpret_cast<void*>(&qm_onDeviceReset),  // +0x04
    reinterpret_cast<void*>(&qm_nullsub)         // +0x08
};

// The capability probe the constructor runs twice, at QueryManager.cpp:37 and :51. CreateQuery with
// a real out-pointer both tests support and creates a query, so the original immediately releases
// the probe - the failure test is the HRESULT, and a non-zero one means unsupported.
bool probeQueryType(IDirect3DDevice9* device, D3DQUERYTYPE type)
{
    IDirect3DQuery9* probe = 0;
    if (!device || device->CreateQuery(type, &probe) != D3D_OK)
        return false;
    if (probe)
        probe->Release();
    return true;
}

} // anonymous namespace


// QueryManager_ctor, 0x10004550, read in full.

// Occlusion-query failure clears both support flags; event-query failure leaves them unchanged.
// Only supported query types are inserted into the set.

QueryManager::QueryManager(IDirect3DDevice9* device)
    : m_vptr(const_cast<void**>(kQueryManagerVtable))
    , m_occlusionOk(1)
    , m_flag1D(1)
{
    m_pad1E[0] = 0;
    m_pad1E[1] = 0;

    // The two sets (sub_100047E0 + the self-linking, on +0x04 and +0x10) are constructed as members.

    if (!probeQueryType(device, D3DQUERYTYPE_OCCLUSION))
    {
        ::renddx9abi::logf("QueryManager.cpp:37 TL: HW does NOT support D3DQUERYTYPE_OCCLUSION. "
                           "(Tell me, then press ignore!)");
        m_occlusionOk = 0;
        m_flag1D      = 0;
    }
    else
    {
        m_supportedTypes.insert(D3DQUERYTYPE_OCCLUSION);
    }

    if (!probeQueryType(device, D3DQUERYTYPE_EVENT))
    {
        ::renddx9abi::logf("QueryManager.cpp:51 TL: HW does NOT support D3DQUERYTYPE_EVENT. "
                           "(Tell me, then press ignore!)");
    }
    else
    {
        m_supportedTypes.insert(D3DQUERYTYPE_EVENT);
    }

    ::renddx9abi::logf("QueryManager: built at 0x%08X - occlusion=%s event=%s, %u supported "
                       "type(s), device-lost listener registered at priority 3",
                       ::renddx9abi::addr(this),
                       m_occlusionOk ? "yes" : "NO",
                       m_supportedTypes.find(D3DQUERYTYPE_EVENT) != m_supportedTypes.end()
                           ? "yes" : "NO",
                       static_cast<unsigned int>(m_supportedTypes.size()));

    // the constructor's last act, 0x100046F4..0x100046FD:
    //     mov ecx, dword_1023B970; push 3; push edi (= this); call sub_10009D60
    // i.e. RendDX9_registerDeviceLostResetListener(this, 3) - the object's only vptr IS the
    // listener table (0x101DC4DC). Registered after the two probes, which is why no probe query
    // is ever tracked. The matching detach is the destructor's first call (0x100044E6, see below).
    registerDeviceLostResetListener(static_cast<void*>(this), 3);
}


// Allocates and tracks a query even for unsupported types, which retain type -1 and no D3D object.

Query* QueryManager::createQuery(int type)
{
    RD9_TRACE_AT("dice::hfe::dx9rend::QueryManager::createQuery", this, 0x00);

    void*  storage = ::renddx9abi::engineAlloc(sizeof(Query));
    Query* query   = storage ? new (storage) Query() : 0;

    if (query && m_supportedTypes.find(static_cast<unsigned int>(type)) != m_supportedTypes.end())
        query->create(type, m_occlusionOk != 0);

    m_liveQueries.insert(query);
    return query;
}


// Releases D3D queries while retaining wrappers for reset. Live-set entries must be nonnull.

void QueryManager::onDeviceLost()
{
    RD9_TRACE_AT("dice::hfe::dx9rend::QueryManager::onDeviceLost", this, 0x00);

    for (std::set<Query*>::const_iterator n = m_liveQueries.begin();
         n != m_liveQueries.end();
         ++n)
    {
        Query* q = *n;
        q->releaseD3DQuery();
    }
}


// Recreates queries whose current type remains supported; type -1 wrappers are untouched.
// Live-set entries must be nonnull.

void QueryManager::onDeviceReset()
{
    RD9_TRACE_AT("dice::hfe::dx9rend::QueryManager::onDeviceReset", this, 0x04);

    for (std::set<Query*>::const_iterator n = m_liveQueries.begin();
         n != m_liveQueries.end();
         ++n)
    {
        Query* q = *n;

        if (m_supportedTypes.find(static_cast<unsigned int>(q->type())) != m_supportedTypes.end())
        {
            q->create(q->type(), m_occlusionOk != 0);
            q->clearFence();
        }
    }
}


// The two entry points RendDX9Init.cpp and BF2Render.cpp use. The original has no such functions -
// it inlines the allocation at RendDX9Init.cpp:249 and loads dword_1023B98C directly - so these are
// [CHOSEN, not recovered] seams, the same shape as createBufferManager next door.

bool createQueryManager(IDirect3DDevice9* device)
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(QueryManager));
    if (!storage)
    {
        g_queryManager = 0;
        ::renddx9abi::logf("RendDX9Init.cpp:249 Failed to create querymanager.");
        return false;
    }

    g_queryManager = new (storage) QueryManager(device);
    return true;
}

// The five calls in Renderer::initialize, reached through the global exactly as the original does.
void* queryManagerCreateQuery(int type)
{
    if (!g_queryManager)
        return 0;
    return static_cast<QueryManager*>(g_queryManager)->createQuery(type);
}


// The event-fence pair renderFrame's throttle ring uses - and they resolve
// +0x19: it is the fence-issued flag.

// sub_10004030 - issue: if a D3D query exists, Issue(D3DISSUE_END) and mark issued.
// sub_100040F0 - wait: only if issued, spin GetData(0, 0, D3DGETDATA_FLUSH) while it returns
// S_FALSE (1), then clear the flag. The spin is the shipped GPU throttle - two frames of latency
// through the two-fence ring at Renderer+0x5A4.

void queryIssueEnd(void* query)
{
    Query* q = static_cast<Query*>(query);
    unsigned char* issued = &q->issued();                  // +0x19
    *issued = 0;
    if (q->d3dQuery())
    {
        q->d3dQuery()->Issue(D3DISSUE_END);
        *issued = 1;
    }
}

void queryWaitDone(void* query)
{
    Query* q = static_cast<Query*>(query);
    unsigned char* issued = &q->issued();                  // +0x19
    if (*issued)
    {
        while (q->d3dQuery()->GetData(0, 0, D3DGETDATA_FLUSH) == S_FALSE)
        {
        }
        *issued = 0;
    }
}


// The teardown - for the terrain culler node's query.


// sub_10004490, `retn 4`, ECX = the manager - erase BY KEY: equal_range (sub_10004930) then
// erase(first, last) (sub_10004CD0). The live set's keys are unique, so the range is the query's
// node or empty, and removing that one node is the whole effect.
void QueryManager::removeQuery(Query* query)
{
    const std::set<Query*>::iterator node = m_liveQueries.find(query);
    if (node != m_liveQueries.end())
        m_liveQueries.erase(node);
}

// sub_10003FA0 - the Query destructor body: the shared three-slot table republished (the
// compiler's vptr reset; 0x101DC468 is the only table a Query ever has), the D3D query Released
// through IUnknown +0x08 when there is one - the pointer is NOT nulled - and the query removed
// from the manager's live set (sub_10004490 with ECX = dword_1023B98C, the global untested).
void Query::destroy()
{
    m_vptr = const_cast<void**>(kQueryVtable);
    if (m_d3dQuery)
        m_d3dQuery->Release();
    static_cast<QueryManager*>(g_queryManager)->removeQuery(this);
}

// The owner-side release every Query holder open-codes (the terrain culler node's destructor
// 0x100E3CC0 and its inlined copy in sub_100E75B0, SkyDome's destructor at 0x10138578):
//     if (!--q->refCount) { sub_10003FA0(q); sub_1005F360(q); }
// Each caller tests the pointer for null first; this does not.
void queryReleaseRef(void* query)
{
    Query* const q = static_cast<Query*>(query);
    if (!q->dropRef())
    {
        q->destroy();
        ::renddx9abi::engineFree(q);
    }
}

// [CHOSEN, not recovered] (issue #7): the native address of the Query field at `retailOffset` in the
// retail x86 0x1C-byte layout, for the recovered bodies outside this file that read a Query by its
// retail offsets (SkyDome's occlusion read and begin/end toggle). It is the identity on x86. An
// offset that names no field stops with a [FRONTIER] diagnostic instead of reading a guess.
unsigned char* Query::fieldAt(unsigned int retailOffset)
{
#if defined(_M_IX86)
    static_assert(offsetof(Query, m_refCount) == 0x04 && offsetof(Query, m_d3dQuery) == 0x08 &&
                  offsetof(Query, m_type) == 0x0C && offsetof(Query, m_field10) == 0x10 &&
                  offsetof(Query, m_field14) == 0x14 && offsetof(Query, m_field18) == 0x18 &&
                  offsetof(Query, m_field19) == 0x19, "the retail Query field offsets");
#endif
    switch (retailOffset)
    {
    case 0x04: return reinterpret_cast<unsigned char*>(&m_refCount);
    case 0x08: return reinterpret_cast<unsigned char*>(&m_d3dQuery);
    case 0x0C: return reinterpret_cast<unsigned char*>(&m_type);
    case 0x10: return reinterpret_cast<unsigned char*>(&m_field10);
    case 0x14: return reinterpret_cast<unsigned char*>(&m_field14);
    case 0x18: return &m_field18;
    case 0x19: return &m_field19;
    default:   break;
    }
    std::fprintf(stderr, "[FRONTIER] dice::hfe::dx9rend::Query::fieldAt - retail offset 0x%X "
                         "names no Query field\n", retailOffset);
    std::fflush(stderr);
    ::renddx9abi::logf("[FRONTIER] dice::hfe::dx9rend::Query::fieldAt - retail offset 0x%X names "
                       "no Query field", retailOffset);
    std::abort();
}

unsigned char* queryFieldAt(void* query, unsigned int retailOffset)
{
    return static_cast<Query*>(query)->fieldAt(retailOffset);
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
