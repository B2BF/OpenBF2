// Sample-history graphs for frame timing and renderer counters.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>

#include <stdio.h>

namespace dice { namespace hfe { namespace dx9rend {
class Effect;
extern void* g_bufferManager;
extern void* g_shaderManager;
void* createVertexBuffer(void*, unsigned int, unsigned int, const std::string&, unsigned int);
void* shaderManagerGetEffect(void*, const char*);
void* effectGetShaderParameter(Effect*, const std::string&);
} } }

#include <new>

namespace dice {
namespace hfe {
namespace geom {

// The classId is read as data - dword_1020AF0C - exactly like GeometryTemplateManager's, not as an
// immediate. The iid is the one Renderer::initialize's createInstanceAs passes.
extern const unsigned int CID_DebugLineGraph = 81038u;
extern const unsigned int IID_IDebugLineGraph = 80039u;

// D3D graph backed by a ring of samples and a dynamically sized vertex buffer.
// SetHistorySize allocates the sample and rendering resources; addSample maintains extrema.
// Preserve the base/renderer layout and 26-slot interface.
class DebugLineGraphD3D
{
public:
    DebugLineGraphD3D();

    void* queryInterface(unsigned int iid) const;
    void  addRef() { ++m_refCount; }
    int   release();

    void setRect(const float* rect);
    void setName(const std::string& name);
    void setColors(unsigned a, unsigned b, unsigned c);
    void setHistorySize(int n);
    void setBounds(unsigned a, unsigned b) { m_bounds0 = a; m_bounds1 = b; }
    void setFlag05(unsigned char v) { m_flag05 = v; }
    int  addSample(float v);
    bool setRange(float lo, float hi);
    void setEnabled(unsigned char v) { m_byte40 = v; }

private:
    void*         m_vptr;            // +0x00
    unsigned char m_byte04;          // +0x04
    // [UNRESOLVED] Flag set by setHistorySize and cleared by renderer initialization.
    unsigned char m_flag05;          // +0x05
    unsigned char m_pad06[2];
    float         m_rect[4];         // +0x08 .. +0x14
    unsigned int  m_colors[3];       // +0x18 .. +0x20
    unsigned char m_byte24;          // +0x24
    unsigned char m_pad25[3];
    float*        m_samples;         // +0x28   the ring buffer
    short         m_capacity;        // +0x2C
    unsigned char m_pad2E[2];
    float         m_rangeMin;        // +0x30
    float         m_rangeMax;        // +0x34
    // +0x38 and +0x3C are written only by slot +0x1C - neither constructor touches them, which is
    // why they looked like padding until that slot was read.
    unsigned int  m_bounds0;         // +0x38
    unsigned int  m_bounds1;         // +0x3C
    unsigned char m_byte40;          // +0x40
    unsigned char m_byte41;          // +0x41   ctor: 1
    short         m_writeIndex;      // +0x42
    float         m_seenMin;         // +0x44
    float         m_seenMax;         // +0x48
    unsigned char m_seeded;          // +0x4C
    unsigned char m_pad4D[3];
    std::string  m_name;            // +0x50
    unsigned int  m_dword6C;         // +0x6C   not written by either constructor
    unsigned int  m_block70[5];      // +0x70 .. +0x80
    unsigned char m_byte84;          // +0x84   ctor: 1
    unsigned char m_byte85;          // +0x85
    unsigned char m_pad86[2];
    float         m_lastSample;      // +0x88
    unsigned int  m_dword8C;         // +0x8C
    unsigned int  m_dword90;         // +0x90
    std::string  m_string94;        // +0x94
    int           m_refCount;        // +0xB0   ctor: 1
    int           m_stride;          // +0xB4
    int           m_vertexCount;     // +0xB8   ctor: 801
    int           m_bufferBytes;     // +0xBC   not written by the constructor
    int           m_dwordC0;         // +0xC0
    std::string  m_className;       // +0xC4   "DebugLineGraph"
    void*         m_vertexBuffer;    // +0xE0
    void*         m_effect;          // +0xE4
    void*         m_paramGraphSize;  // +0xE8
    void*         m_paramViewPort;   // +0xEC
};

namespace {

void  RD9_THUNK dlg_addRef(void* s RD9_THUNK_EDX) { static_cast<DebugLineGraphD3D*>(s)->addRef(); }
int   RD9_THUNK dlg_release(void* s RD9_THUNK_EDX) { return static_cast<DebugLineGraphD3D*>(s)->release(); }
void* RD9_THUNK dlg_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
{
    return static_cast<DebugLineGraphD3D*>(s)->queryInterface(iid);
}

// +0x10, sub_100A26B0. Four floats to +0x08..+0x14, then return 1. The original writes index 3
// before index 2 - compiler scheduling, same net effect.
char RD9_THUNK dlg_setRect(void* s RD9_THUNK_EDX, const float* rect)
{
    RD9_TRACE_AT("dice::hfe::geom::DebugLineGraphD3D::setRect", s, 0x10);
    static_cast<DebugLineGraphD3D*>(s)->setRect(rect);
    return 1;
}

// +0x18, sub_10096470. One statement: `this->name(+0x50) = a2`.
void RD9_THUNK dlg_setName(void* s RD9_THUNK_EDX, const std::string& n)
{
    RD9_TRACE_AT("dice::hfe::geom::DebugLineGraphD3D::setName", s, 0x18);
    static_cast<DebugLineGraphD3D*>(s)->setName(n);
}

// +0x28, sub_10096480. One byte store at +0x05 - see the field's note for why it is not "built".
char RD9_THUNK dlg_setFlag05(void* s RD9_THUNK_EDX, char v)
{
    static_cast<DebugLineGraphD3D*>(s)->setFlag05(static_cast<unsigned char>(v));
    return v;
}

// Adds a sample through the void plugin slot. Returning a floating-point value would leave an unused x87 result.
void RD9_THUNK dlg_addSample(void* s RD9_THUNK_EDX, float v)
{
    static_cast<DebugLineGraphD3D*>(s)->addSample(v);
}

// +0x1C, sub_10095C40 -> sub_100A2610. A thunk onto a two-field setter: `this[14] = a; this[15] = b`
// at +0x38 and +0x3C. Renderer::initialize passes (0, <float>) - 100.0f, 60.0f and 584.0f for the
// three graphs.
void RD9_THUNK dlg_setBounds(void* s RD9_THUNK_EDX, unsigned a, unsigned b)
{
    RD9_TRACE_AT("dice::hfe::geom::DebugLineGraphD3D::setBounds", s, 0x1C);
    static_cast<DebugLineGraphD3D*>(s)->setBounds(a, b);
}

// +0x44, sub_100A25E0. Three dwords to +0x18/+0x1C/+0x20. Neither constructor writes them, so this
// is their only initialiser.
void RD9_THUNK dlg_setColors(void* s RD9_THUNK_EDX, unsigned a, unsigned b, unsigned c)
{
    RD9_TRACE_AT("dice::hfe::geom::DebugLineGraphD3D::setColors", s, 0x44);
    static_cast<DebugLineGraphD3D*>(s)->setColors(a, b, c);
}

// +0x4C, sub_100A2630. guarded, and the guard is the whole of it: `if (lo >= hi) return 0;`. An
// inverted range is rejected outright rather than clamped.
char RD9_THUNK dlg_setRange(void* s RD9_THUNK_EDX, float lo, float hi)
{
    RD9_TRACE_AT("dice::hfe::geom::DebugLineGraphD3D::setRange", s, 0x4C);
    return static_cast<DebugLineGraphD3D*>(s)->setRange(lo, hi) ? 1 : 0;
}

// +0x50, sub_100A2660. One byte store at +0x40.
char RD9_THUNK dlg_setEnabled(void* s RD9_THUNK_EDX, char v)
{
    RD9_TRACE_AT("dice::hfe::geom::DebugLineGraphD3D::setEnabled", s, 0x50);
    static_cast<DebugLineGraphD3D*>(s)->setEnabled(static_cast<unsigned char>(v));
    return v;
}


// Initializes sample history, vertex storage and graph shader parameters.
// Must complete before addSample, which requires an allocated ring buffer.

void RD9_THUNK dlg_setHistorySize(void* s RD9_THUNK_EDX, int n)
{
    RD9_TRACE_AT("dice::hfe::geom::DebugLineGraphD3D::setHistorySize", s, 0x3C);

    DebugLineGraphD3D* self = static_cast<DebugLineGraphD3D*>(s);
    self->setHistorySize(n);
}

#define RD9_DLG_SLOT(name, slotOffset, addr)                                        \
    void RD9_THUNK name(void* s RD9_THUNK_EDX)                                            \
    {                                                                               \
        RD9_FRONTIER_AT("dice::hfe::geom::DebugLineGraphD3D::slot_" #slotOffset      \
                        " (" #addr ")", s, 0x00);                                   \
    }

RD9_DLG_SLOT(dlg_slot14, 0x14, 0x100A25D0)
RD9_DLG_SLOT(dlg_slot20, 0x20, 0x100A2690)
RD9_DLG_SLOT(dlg_slot24, 0x24, 0x10095F90)
RD9_DLG_SLOT(dlg_slot2C, 0x2C, 0x10096490)
RD9_DLG_SLOT(dlg_slot30, 0x30, 0x100965C0)
RD9_DLG_SLOT(dlg_slot34, 0x34, 0x100965D0)
RD9_DLG_SLOT(dlg_slot38, 0x38, 0x100964A0)
RD9_DLG_SLOT(dlg_slot48, 0x48, 0x100A2600)
RD9_DLG_SLOT(dlg_slot54, 0x54, 0x100A2670)
RD9_DLG_SLOT(dlg_slot58, 0x58, 0x100964C0)
RD9_DLG_SLOT(dlg_slot5C, 0x5C, 0x100964D0)
RD9_DLG_SLOT(dlg_slot60, 0x60, 0x100964E0)
RD9_DLG_SLOT(dlg_slot64, 0x64, 0x10096500)

#undef RD9_DLG_SLOT

// +0x00, 0x10096540 - not read; the class's own first slot rather than addRef, which is at +0x04's
// neighbour set. The IBase quartet here is +0x00..+0x0C as usual for this image.
void* const kVtable[26] =
{
    reinterpret_cast<void*>(&dlg_addRef),           // +0x00
    reinterpret_cast<void*>(&dlg_release),          // +0x04
    reinterpret_cast<void*>(&dlg_release),          // +0x08
    reinterpret_cast<void*>(&dlg_queryInterface),   // +0x0C
    reinterpret_cast<void*>(&dlg_setRect),          // +0x10  0x100A26B0  read
    reinterpret_cast<void*>(&dlg_slot14),           // +0x14
    reinterpret_cast<void*>(&dlg_setName),          // +0x18  0x10096470  read
    reinterpret_cast<void*>(&dlg_setBounds),        // +0x1C  0x10095C40  read
    reinterpret_cast<void*>(&dlg_slot20),           // +0x20
    reinterpret_cast<void*>(&dlg_slot24),           // +0x24
    reinterpret_cast<void*>(&dlg_setFlag05),        // +0x28  0x10096480  read
    reinterpret_cast<void*>(&dlg_slot2C),           // +0x2C
    reinterpret_cast<void*>(&dlg_slot30),           // +0x30
    reinterpret_cast<void*>(&dlg_slot34),           // +0x34
    reinterpret_cast<void*>(&dlg_slot38),           // +0x38
    reinterpret_cast<void*>(&dlg_setHistorySize),   // +0x3C  0x10095CB0  read, frontier
    reinterpret_cast<void*>(&dlg_addSample),        // +0x40  0x100A2520  read
    reinterpret_cast<void*>(&dlg_setColors),        // +0x44  0x100A25E0  read
    reinterpret_cast<void*>(&dlg_slot48),           // +0x48
    reinterpret_cast<void*>(&dlg_setRange),         // +0x4C  0x100A2630  read
    reinterpret_cast<void*>(&dlg_setEnabled),       // +0x50  0x100A2660  read
    reinterpret_cast<void*>(&dlg_slot54),           // +0x54
    reinterpret_cast<void*>(&dlg_slot58),           // +0x58
    reinterpret_cast<void*>(&dlg_slot5C),           // +0x5C
    reinterpret_cast<void*>(&dlg_slot60),           // +0x60
    reinterpret_cast<void*>(&dlg_slot64)            // +0x64
};

} // anonymous namespace


// Initializes base and derived graph state in order.

DebugLineGraphD3D::DebugLineGraphD3D()
    : m_vptr(const_cast<void**>(kVtable))
    , m_byte04(0)
    , m_flag05(0)
    , m_byte24(0)
    , m_samples(0)
    , m_capacity(0)
    , m_rangeMin(0.0f)
    , m_rangeMax(0.0f)
    , m_byte40(0)
    , m_byte41(1)
    , m_writeIndex(0)
    , m_seenMin(0.0f)
    , m_seenMax(0.0f)
    , m_seeded(0)
    , m_dword6C(0)
    , m_byte84(1)
    , m_byte85(0)
    , m_lastSample(0.0f)
    , m_dword8C(0)
    , m_dword90(0)
    , m_refCount(1)          // +0xB0
    , m_stride(0)            // +0xB4, ctor: 0; setHistorySize writes 12
    , m_vertexCount(801)     // +0xB8
    , m_bufferBytes(0)
    , m_dwordC0(0)
    , m_vertexBuffer(0)
    , m_effect(0)
    , m_paramGraphSize(0)
    , m_paramViewPort(0)
{
    m_pad06[0] = 0; m_pad06[1] = 0;
    for (int i = 0; i < 4; ++i) m_rect[i] = 0.0f;
    // The three colours at +0x18..+0x20 are not written by either constructor - setColors is their
    // only initialiser. Zeroed here so they are not allocator garbage before that call.
    for (int i = 0; i < 3; ++i) m_colors[i] = 0;
    m_pad25[0] = 0; m_pad25[1] = 0; m_pad25[2] = 0;
    m_pad2E[0] = 0; m_pad2E[1] = 0;
    m_bounds0 = 0; m_bounds1 = 0;
    m_pad4D[0] = 0; m_pad4D[1] = 0; m_pad4D[2] = 0;
    for (int i = 0; i < 5; ++i) m_block70[i] = 0;
    m_pad86[0] = 0; m_pad86[1] = 0;

    m_className = "DebugLineGraph";
}

int DebugLineGraphD3D::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~DebugLineGraphD3D();          // the three name strings
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

void* DebugLineGraphD3D::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IDebugLineGraph)
        return const_cast<DebugLineGraphD3D*>(this);
    return 0;
}


// Allocates the sample ring and its graph vertex buffer. Call before adding samples.

void DebugLineGraphD3D::setHistorySize(int n)
{
    if (n <= 0)
        return;

    m_capacity = static_cast<short>(n);
    m_samples  = static_cast<float*>(::renddx9abi::engineAlloc(
        static_cast<unsigned int>(n) * sizeof(float)));
    if (m_samples)
        for (int i = 0; i < n; ++i)
            m_samples[i] = 0.0f;

    m_vertexCount = n + 6;
    m_stride      = 12;
    m_bufferBytes = m_vertexCount * m_stride;

    m_vertexBuffer = ::dice::hfe::dx9rend::createVertexBuffer(
        ::dice::hfe::dx9rend::g_bufferManager,
        static_cast<unsigned int>(m_vertexCount), static_cast<unsigned int>(m_stride),
        std::string("DebugLineGraphD3D"), 520u);

    // Use the class name for the shared shader; the per-graph name may not yet be set.
    char shaderPath[128];
    ::_snprintf_s(shaderPath, sizeof(shaderPath), _TRUNCATE, "Shaders/%s", m_className.c_str());
    m_effect = ::dice::hfe::dx9rend::shaderManagerGetEffect(
        ::dice::hfe::dx9rend::g_shaderManager, shaderPath);

    if (m_effect)
    {
        m_paramGraphSize = ::dice::hfe::dx9rend::effectGetShaderParameter(
            static_cast<::dice::hfe::dx9rend::Effect*>(m_effect), std::string("GRAPHSIZE"));
        if (!m_paramGraphSize)
            ::renddx9abi::logf("DebugLineGraphD3d.cpp:95 Could not load m_paramGraphSize parameter "
                               "from shader");

        m_paramViewPort = ::dice::hfe::dx9rend::effectGetShaderParameter(
            static_cast<::dice::hfe::dx9rend::Effect*>(m_effect), std::string("VIEWPORTSIZE"));
        if (!m_paramViewPort)
            ::renddx9abi::logf("DebugLineGraphD3d.cpp:103 Could not load m_paramViewPort parameter "
                               "from shader");
    }

    m_flag05 = 1;

    ::renddx9abi::logf("DebugLineGraphD3D::setHistorySize(%d) \"%s\": ring=0x%p vb=0x%p "
                       "effect=0x%p graphSize=0x%p viewPort=0x%p", n, m_className.c_str(),
                       static_cast<const void*>(m_samples), m_vertexBuffer, m_effect,
                       m_paramGraphSize, m_paramViewPort);
}


// Adds a sample and updates the observed range. The first out-of-range sample seeds the entire ring.
// Returns 1 when the write cursor wraps, otherwise 0.

int DebugLineGraphD3D::addSample(float v)
{
    if (!m_samples || m_capacity <= 0)
        return 0;

    if (v >= m_seenMin)
    {
        if (v > m_seenMax)
        {
            const bool unseeded = (m_seeded == 0);
            m_seenMax = v;
            if (unseeded)
            {
                for (short i = 0; i < m_capacity; ++i)
                    m_samples[i] = v;
                m_seeded = 1;
            }
        }
    }
    else
    {
        const bool unseeded = (m_seeded == 0);
        m_seenMin = v;
        if (unseeded)
        {
            for (short i = 0; i < m_capacity; ++i)
                m_samples[i] = v;
            m_seeded = 1;
        }
    }

    m_samples[m_writeIndex] = v;
    const int wrapped = static_cast<short>(m_writeIndex + 1) / m_capacity;
    m_writeIndex = static_cast<short>((m_writeIndex + 1) % m_capacity);
    m_lastSample = v;
    return wrapped;
}

void DebugLineGraphD3D::setRect(const float* rect)
{
    m_rect[1] = rect[1];
    m_rect[0] = rect[0];
    m_rect[2] = rect[2];
    m_rect[3] = rect[3];
}

void DebugLineGraphD3D::setName(const std::string& name) { m_name = name; }

void DebugLineGraphD3D::setColors(unsigned a, unsigned b, unsigned c)
{
    m_colors[0] = a;
    m_colors[1] = b;
    m_colors[2] = c;
}

bool DebugLineGraphD3D::setRange(float lo, float hi)
{
    if (lo >= hi)
        return false;
    m_rangeMin = lo;
    m_rangeMax = hi;
    return true;
}

// RefFactory_geom_DebugLineGraph, 0x1007CC20.
IBase* __fastcall createDebugLineGraph(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugLineGraphD3D));
    if (!storage)
        return 0;

    DebugLineGraphD3D* self = new (storage) DebugLineGraphD3D();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
