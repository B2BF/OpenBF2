// Sample-history graphs for frame timing and renderer counters.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "Debug.h"
#include "DebugTextWriter.h"
#include "../Core/StringUtil.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendMisc/IRenderView.h"
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h"
#endif
#include <cfloat>
#include <climits>
#include <cmath>
#include <cfenv>
#include <cstring>
#include <utility>
#include <vector>
#include <string>

#include <stdio.h>

namespace dice { namespace hfe { namespace dx9rend {
class Effect;
extern void* g_bufferManager;
extern void* g_shaderManager;
void* createVertexBuffer(void*, unsigned int, unsigned int, const std::string&, unsigned int);
void* shaderManagerGetEffect(void*, const char*);
void* effectGetShaderParameter(Effect*, const std::string&);
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect*, const std::string&);
int effectRelease(Effect*);
bool effectSetTechniqueByName(void*, const std::string&);
unsigned int effectBegin(void*);
void effectBeginPass(void*, unsigned int);
void effectEnd(void*);
int bufferManagerSwapPool(int);
void vertexBufferRelease(void*);
void* vertexBufferLock(void*, unsigned int, unsigned int, unsigned int);
bool vertexBufferUnlock(void*);
void vertexBufferBindStream(void*, unsigned int);
void rendererDrawPrimitive(unsigned int, unsigned int, unsigned int);
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
// Preserve the base/renderer layout and the 27-slot table, including deletion.
// [INFERRED] Base-class spelling. Its destructor runs after the derived shader-name
// string, then frees the sample ring before its own strings and legend vector.
class DebugLineGraphState
{
public:
    DebugLineGraphState();
    ~DebugLineGraphState();
protected:
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
    // Manual graph limits; history initialization resets both to zero.
    float         m_bounds0;         // +0x38
    float         m_bounds1;         // +0x3C
    unsigned char m_byte40;          // +0x40
    unsigned char m_byte41;          // +0x41   ctor: 1
    short         m_writeIndex;      // +0x42
    float         m_seenMin;         // +0x44
    float         m_seenMax;         // +0x48
    unsigned char m_seeded;          // +0x4C
    unsigned char m_pad4D[3];
    std::string  m_name;            // +0x50
    unsigned int  m_dword6C;         // +0x6C   not written by either constructor
    typedef std::pair<std::string, unsigned int> LegendEntry;
    std::vector<LegendEntry> m_legend; // +0x70 .. +0x78, allocator word +0x6C
    int           m_legendX;         // +0x7C
    int           m_legendY;         // +0x80
    unsigned char m_byte84;          // +0x84   ctor: 1
    unsigned char m_byte85;          // +0x85
    unsigned char m_pad86[2];
    float         m_lastSample;      // +0x88
    int           m_dword8C;         // +0x8C
    int           m_dword90;         // +0x90
    std::string  m_string94;        // +0x94
};

class DebugLineGraphD3D : public DebugLineGraphState
{
public:
    DebugLineGraphD3D();
    ~DebugLineGraphD3D();
    void* queryInterface(unsigned int iid) const;
    int addRef() { return ++m_refCount; } // PE 0x10096540
    int getRef() const { return m_refCount; } // PE 0x10096550
    int release();
    void* destroy(unsigned int flags);
    void setRect(const float* rect);
    void setName(const std::string& name);
    void setColors(unsigned a, unsigned b, unsigned c);
    void setHistorySize(int n);
    // PE 0x100A2610
    void setBounds(float lo, float hi) { m_bounds0 = lo; m_bounds1 = hi; }
    void setFlag05(unsigned char v) { m_flag05 = v; }
    unsigned char getFlag05() const { return m_flag05; } // PE 0x10096490
    int addSample(float v);
    bool setRange(float lo, float hi);
    void setEnabled(unsigned char v) { m_byte40 = v; }
    void setColour(unsigned int value) { m_colors[0] = value; } // PE 0x100A25D0
    void resetExtrema();
    void draw();
    void clearLegend();
    void addLegend(const std::string& name, unsigned int colour);
    void setLegendPosition(int x, int y) { m_legendX = x; m_legendY = y; } // PE 0x100964A0
    void setAutoScale(unsigned char v) { m_byte24 = v; } // PE 0x100A2600
    bool setByte41(unsigned char v)
    { if (v > 10) return false; m_byte41 = v; return true; } // PE 0x100A2670
    void setDrawValue(unsigned char v) { m_byte85 = v; } // PE 0x100964C0
    unsigned char getDrawValue() const { return m_byte85; } // PE 0x100964D0
    void setValuePosition(int x, int y) { m_dword8C = x; m_dword90 = y; } // PE 0x100964E0
    void setValueSuffix(const std::string& value) { m_string94 = value; } // PE 0x10096500
private:
    void initializeHistory(short n);
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

int   RD9_THUNK dlg_addRef(void* s RD9_THUNK_EDX) { return static_cast<DebugLineGraphD3D*>(s)->addRef(); }
int   RD9_THUNK dlg_getRef(void* s RD9_THUNK_EDX) { return static_cast<DebugLineGraphD3D*>(s)->getRef(); }
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

// +0x1C, sub_10095C40 -> sub_100A2610. A thunk onto a two-field setter: the two stack dwords go to
// +0x38 and +0x3C, which the sample clamp reads as floats. Every caller passes floats: Renderer::initialize
// (0, 100.0f / 60.0f / 584.0f for the three graphs), BF2.exe 0x815AE0..0x815C00 and the Linux bf2
// 0x842E050..0x842E4F0. Declared float so x86-64 receives them in the float argument registers.
void RD9_THUNK dlg_setBounds(void* s RD9_THUNK_EDX, float lo, float hi)
{
    RD9_TRACE_AT("dice::hfe::geom::DebugLineGraphD3D::setBounds", s, 0x1C);
    static_cast<DebugLineGraphD3D*>(s)->setBounds(lo, hi);
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

// PE addresses are retained on the corresponding member definitions/declarations.
void RD9_THUNK dlg_slot14(void* s RD9_THUNK_EDX, unsigned v) { static_cast<DebugLineGraphD3D*>(s)->setColour(v); }
void RD9_THUNK dlg_slot20(void* s RD9_THUNK_EDX) { static_cast<DebugLineGraphD3D*>(s)->resetExtrema(); }
void RD9_THUNK dlg_slot24(void* s RD9_THUNK_EDX) { static_cast<DebugLineGraphD3D*>(s)->draw(); }
unsigned char RD9_THUNK dlg_slot2C(void* s RD9_THUNK_EDX) { return static_cast<DebugLineGraphD3D*>(s)->getFlag05(); }
void RD9_THUNK dlg_slot30(void* s RD9_THUNK_EDX) { static_cast<DebugLineGraphD3D*>(s)->clearLegend(); }
void RD9_THUNK dlg_slot34(void* s RD9_THUNK_EDX, const std::string& n, unsigned c) { static_cast<DebugLineGraphD3D*>(s)->addLegend(n,c); }
void RD9_THUNK dlg_slot38(void* s RD9_THUNK_EDX, int x, int y) { static_cast<DebugLineGraphD3D*>(s)->setLegendPosition(x,y); }
void RD9_THUNK dlg_slot48(void* s RD9_THUNK_EDX, unsigned char v) { static_cast<DebugLineGraphD3D*>(s)->setAutoScale(v); }
bool RD9_THUNK dlg_slot54(void* s RD9_THUNK_EDX, unsigned char v) { return static_cast<DebugLineGraphD3D*>(s)->setByte41(v); }
void RD9_THUNK dlg_slot58(void* s RD9_THUNK_EDX, unsigned char v) { static_cast<DebugLineGraphD3D*>(s)->setDrawValue(v); }
unsigned char RD9_THUNK dlg_slot5C(void* s RD9_THUNK_EDX) { return static_cast<DebugLineGraphD3D*>(s)->getDrawValue(); }
void RD9_THUNK dlg_slot60(void* s RD9_THUNK_EDX, int x, int y) { static_cast<DebugLineGraphD3D*>(s)->setValuePosition(x,y); }
void RD9_THUNK dlg_slot64(void* s RD9_THUNK_EDX, const std::string& v) { static_cast<DebugLineGraphD3D*>(s)->setValueSuffix(v); }
void* RD9_THUNK dlg_destroy(void* s RD9_THUNK_EDX, unsigned flags) { return static_cast<DebugLineGraphD3D*>(s)->destroy(flags); }

// PE table 0x1020C7E8; the deleting destructor follows the public graph slots.
void* const kVtable[27] =
{
    reinterpret_cast<void*>(&dlg_addRef),           // +0x00
    reinterpret_cast<void*>(&dlg_getRef),           // +0x04  PE 0x10096550
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
    reinterpret_cast<void*>(&dlg_setHistorySize),   // +0x3C  0x10095CB0
    reinterpret_cast<void*>(&dlg_addSample),        // +0x40  0x100A2520  read
    reinterpret_cast<void*>(&dlg_setColors),        // +0x44  0x100A25E0  read
    reinterpret_cast<void*>(&dlg_slot48),           // +0x48
    reinterpret_cast<void*>(&dlg_setRange),         // +0x4C  0x100A2630  read
    reinterpret_cast<void*>(&dlg_setEnabled),       // +0x50  0x100A2660  read
    reinterpret_cast<void*>(&dlg_slot54),           // +0x54
    reinterpret_cast<void*>(&dlg_slot58),           // +0x58
    reinterpret_cast<void*>(&dlg_slot5C),           // +0x5C
    reinterpret_cast<void*>(&dlg_slot60),           // +0x60
    reinterpret_cast<void*>(&dlg_slot64),           // +0x64
    reinterpret_cast<void*>(&dlg_destroy)           // +0x68  PE 0x10096580
};

} // anonymous namespace


// Initializes base and derived graph state in order.

// PE 0x100A26E0. Unwritten colours, ranges, extrema and sample value remain unwritten.
DebugLineGraphState::DebugLineGraphState()
    : m_byte04(0)
    , m_flag05(0)
    , m_rect{0.0f, 0.0f, 0.0f, 0.0f}
    , m_byte24(0)
    , m_samples(0)
    , m_capacity(0)
    , m_byte40(0)
    , m_byte41(1)
    , m_writeIndex(0)
    , m_legendX(0)
    , m_legendY(0)
    , m_byte84(1)
    , m_byte85(0)
    , m_dword8C(0)
    , m_dword90(0)
{
}

// PE 0x100A2760. The string/vector destructors follow this ring release.
DebugLineGraphState::~DebugLineGraphState()
{
    if (m_samples) ::renddx9abi::engineFree(m_samples);
}

// PE 0x10095C50.
DebugLineGraphD3D::DebugLineGraphD3D()
    : m_refCount(1)          // +0xB0
    , m_stride(0)            // +0xB4, ctor: 0; setHistorySize writes 12
    , m_vertexCount(801)     // +0xB8
    , m_dwordC0(0)
    , m_className("DebugLineGraph")
    , m_vertexBuffer(0)
    , m_effect(0)
    , m_paramGraphSize(0)
    , m_paramViewPort(0)
{
    m_vptr = const_cast<void**>(kVtable);
}

// Restores the graph table, then releases the vertex buffer, both shader parameters and the effect;
// class-name destruction and the base destructor 0x100A2760 follow.
// PE 0x10095EF0, 0x10095F60: the resource release and the complete destructor around it.
// The complete destructor 0x10095F60 is unreachable in 1.50: no code or data references to that
// out-of-line copy; the deleting destructor 0x10096580 inlines the same sequence.
DebugLineGraphD3D::~DebugLineGraphD3D()
{
    m_vptr = const_cast<void**>(kVtable);
    if (m_vertexBuffer) dx9rend::vertexBufferRelease(m_vertexBuffer);
    if (m_paramViewPort) static_cast<dx9rend::ParameterOwner*>(m_paramViewPort)->release();
    if (m_paramGraphSize) static_cast<dx9rend::ParameterOwner*>(m_paramGraphSize)->release();
    if (m_effect) dx9rend::effectRelease(static_cast<dx9rend::Effect*>(m_effect));
}

// PE 0x10096580.
void* DebugLineGraphD3D::destroy(unsigned int flags)
{
    this->~DebugLineGraphD3D();
    if (flags & 1) ::renddx9abi::engineFree(this);
    return this;
}

int DebugLineGraphD3D::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        typedef void* (__thiscall* Destroy)(void*, unsigned int);
        reinterpret_cast<Destroy>(static_cast<void* const*>(m_vptr)[0x68 / 4])(this, 1);
    }
    return remaining;
}

void* DebugLineGraphD3D::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IDebugLineGraph || iid == 80041u) // PE 0x10096510
        return const_cast<DebugLineGraphD3D*>(this);
    return 0;
}


// PE 0x100A24A0. Reinitialization overwrites the ring without freeing it.
void DebugLineGraphD3D::initializeHistory(short n)
{
    m_capacity = n;
    m_colors[1] = m_colors[2] = 0xFF000000u;
    m_byte40 = 0;
    m_colors[0] = 0xFFFFFFFFu;
    m_byte24 = 0;
    m_writeIndex = 0;
    m_byte41 = 1;
    m_byte04 = 0;
    for (int i = 0; i < 4; ++i) m_rect[i] = 0.0f;
    m_flag05 = 1;
    // Shipped defect: the previous ring/buffer/effect references are lost on repeated calls.
    m_samples  = static_cast<float*>(::renddx9abi::engineAlloc(
        static_cast<unsigned int>(static_cast<int>(m_capacity)) * 4u));
    typedef void (__thiscall* Reset)(void*);
    reinterpret_cast<Reset>(static_cast<void* const*>(m_vptr)[0x20 / 4])(this);
    m_bounds0 = m_bounds1 = 0.0f;
    m_rangeMin = m_rangeMax = 0.0f;
}

// PE 0x10095CB0. Reinitialization overwrites owned resources without freeing them.
void DebugLineGraphD3D::setHistorySize(int n)
{
    initializeHistory(static_cast<short>(n));
    m_vertexCount = n + 6;
    m_stride      = 12;
    const int oldPool = dx9rend::bufferManagerSwapPool(0);
    m_bufferBytes = m_vertexCount * m_stride;
    m_vertexBuffer = ::dice::hfe::dx9rend::createVertexBuffer(
        ::dice::hfe::dx9rend::g_bufferManager,
        static_cast<unsigned int>(m_vertexCount), static_cast<unsigned int>(m_stride),
        std::string("DebugLineGraphD3D"), 520u);
    dx9rend::bufferManagerSwapPool(oldPool);

    // Use the class name for the shared shader; the per-graph name may not yet be set.
    m_effect = ::dice::hfe::dx9rend::shaderManagerGetEffect(
        ::dice::hfe::dx9rend::g_shaderManager, (std::string("Shaders/") + m_className).c_str());
    m_paramGraphSize = dx9rend::effectGetShaderParameterBorrowedTyped(
        static_cast<dx9rend::Effect*>(m_effect), std::string("GRAPHSIZE"));
    if (m_paramGraphSize)
        static_cast<dx9rend::ParameterOwner*>(m_paramGraphSize)->addRef();
    else
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugLineGraphD3d.cpp", 95, "")
            << "Could not load m_paramGraphSize parameter from shader";
    m_paramViewPort = dx9rend::effectGetShaderParameterBorrowedTyped(
        static_cast<dx9rend::Effect*>(m_effect), std::string("VIEWPORTSIZE"));
    if (m_paramViewPort)
        static_cast<dx9rend::ParameterOwner*>(m_paramViewPort)->addRef();
    else
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugLineGraphD3d.cpp", 103, "")
            << "Could not load m_paramViewPort parameter from shader";
    m_flag05 = 1;
}


// Adds a sample and updates the observed range. The first out-of-range sample seeds the entire ring.
// Returns 1 when the write cursor wraps, otherwise 0.

// PE 0x100A2520. No ring/capacity guard; NaNs do not change either extremum.
int DebugLineGraphD3D::addSample(float v)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const bool below = bf2x87::less(bf2x87::wide(v), bf2x87::wide(m_seenMin));
#else
    const bool below = v < m_seenMin;
#endif
    if (below)
    {
        const bool unseeded = m_seeded == 0;
        m_seenMin = v;
        if (unseeded)
        {
            for (int i = 0; i < m_capacity; ++i) m_samples[i] = v;
            m_seeded = 1;
        }
    }
    else
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const bool above = bf2x87::greater(bf2x87::wide(v), bf2x87::wide(m_seenMax));
#else
        const bool above = v > m_seenMax;
#endif
        if (above)
        {
            const bool unseeded = m_seeded == 0;
            m_seenMax = v;
            if (unseeded)
            {
                for (int i = 0; i < m_capacity; ++i) m_samples[i] = v;
                m_seeded = 1;
            }
        }
    }

    m_samples[m_writeIndex] = v;
    const short next = static_cast<short>(m_writeIndex + 1);
    const int wrapped = next / m_capacity;
    m_writeIndex = static_cast<short>(next % m_capacity);
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
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    if (bf2x87::greaterEqual(bf2x87::wide(lo), bf2x87::wide(hi)))
#else
    if (lo >= hi)
#endif
        return false;
    m_rangeMin = lo;
    m_rangeMax = hi;
    return true;
}

// PE 0x100A2690. Shipped defect: negative-only samples retain positive FLT_MIN as the maximum.
void DebugLineGraphD3D::resetExtrema()
{
    m_seeded = 0;
    m_seenMin = FLT_MAX;
    m_seenMax = FLT_MIN;
}

// PE 0x100965C0 -> 0x1001D610: destroy the legend entries and release their backing allocation.
void DebugLineGraphD3D::clearLegend()
{
    std::vector<LegendEntry>().swap(m_legend);
}

// PE 0x100965D0. The string is copied into a pair, then appended by value.
void DebugLineGraphD3D::addLegend(const std::string& name, unsigned int colour)
{
    const LegendEntry entry = std::make_pair(name, colour);
    m_legend.push_back(entry);
}

namespace {

// [CHOSEN, not recovered] Factoring of the draw body's repeated FISTP-chop sequences.
int graphCoordinate(float value)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved, chop;
    int result;
    __asm {
        fld value
        fnstcw saved
        mov ax, saved
        or ax, 0C00h
        mov chop, ax
        fldcw chop
        fistp result
        fldcw saved
    }
    return result;
#elif (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return bf2x87::integer<int>(bf2x87::wide(value));
#else
    if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0)
    { std::feraiseexcept(FE_INVALID); return INT_MIN; }
    return static_cast<int>(value);
#endif
}

int graphRightCoordinate(float x, float width)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const float gap = 10.0f;
    unsigned short saved, chop;
    int result;
    __asm {
        fld x
        fadd width
        fadd gap
        fnstcw saved
        mov ax, saved
        or ax, 0C00h
        mov chop, ax
        fldcw chop
        fistp result
        fldcw saved
    }
    return result;
#elif (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return bf2x87::integer<int>(bf2x87::add(
        bf2x87::add(bf2x87::wide(x), bf2x87::wide(width)), bf2x87::wide(10.0f)));
#else
    return graphCoordinate(x + width + 10.0f);
#endif
}

struct GraphVertex { float x, y; unsigned int colour; };
static_assert(sizeof(GraphVertex) == 12, "graph vertex stride");

} // namespace

// PE 0x10095F90, slot +0x24. Lock failure retains the graph's buffer position and text state.
void DebugLineGraphD3D::draw()
{
    if (!m_seeded) return;
    const unsigned int sampleBytes = static_cast<unsigned int>(m_stride * static_cast<int>(m_capacity));
    if (static_cast<unsigned int>(m_dwordC0) > static_cast<unsigned int>(m_bufferBytes) - sampleBytes)
        m_dwordC0 = 0;
    GraphVertex* const vertices = static_cast<GraphVertex*>(dx9rend::vertexBufferLock(
        m_vertexBuffer, 0, static_cast<unsigned int>(m_capacity + 5), 0x2000));
    if (!vertices) return;
    int count = 0;
    for (; count < m_capacity; ++count)
    {
        const int index = (static_cast<int>(m_writeIndex) + m_capacity + count - 1) % m_capacity;
        const float value = m_samples[index];
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] Retain x87 intermediates through the final y store.
        using namespace bf2x87;
        vertices[count].x = store(wide(count));
        Real level;
        if (m_byte24)
            level = mul(div(wide(m_rect[3]), sub(wide(m_seenMax), wide(m_seenMin))),
                        sub(wide(value), wide(m_seenMin)));
        else if (less(wide(value), wide(m_bounds0))) level = wide(0);
        else if (greater(wide(value), wide(m_bounds1))) level = wide(m_rect[3]);
        else
        {
            level = mul(div(wide(m_rect[3]), sub(wide(m_bounds1), wide(m_bounds0))),
                        sub(wide(value), wide(m_bounds0)));
            if (greater(level, wide(m_rect[3]))) level = wide(m_rect[3]);
        }
        vertices[count].y = store(sub(wide(m_rect[3]), level));
        if (m_byte40 && less(wide(value), wide(m_rangeMin))) vertices[count].colour = m_colors[1];
        else if (m_byte40 && greater(wide(value), wide(m_rangeMax))) vertices[count].colour = m_colors[2];
        else vertices[count].colour = m_colors[0];
#else
        vertices[count].x = static_cast<float>(count);
        float level;
        if (m_byte24)
            level = m_rect[3] / (m_seenMax - m_seenMin) * (value - m_seenMin);
        else if (value < m_bounds0) level = 0.0f;
        else if (value > m_bounds1) level = m_rect[3];
        else
        {
            level = m_rect[3] / (m_bounds1 - m_bounds0) * (value - m_bounds0);
            if (level > m_rect[3]) level = m_rect[3];
        }
        vertices[count].y = m_rect[3] - level;
        if (m_byte40 && value < m_rangeMin) vertices[count].colour = m_colors[1];
        else if (m_byte40 && value > m_rangeMax) vertices[count].colour = m_colors[2];
        else vertices[count].colour = m_colors[0];
#endif
    }
    vertices[count++] = GraphVertex{0.0f, 0.0f, 0xFFFFFFFFu};
    vertices[count++] = GraphVertex{m_rect[2], 0.0f, 0xFFFFFFFFu};
    vertices[count++] = GraphVertex{m_rect[2], m_rect[3], 0xFFFFFFFFu};
    vertices[count++] = GraphVertex{0.0f, m_rect[3], 0xFFFFFFFFu};
    vertices[count++] = GraphVertex{0.0f, 0.0f, 0xFFFFFFFFu};
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
    dx9rend::effectSetTechniqueByName(m_effect, std::string("Graph"));
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();
    const float graphSize[4] = {m_rect[0], m_rect[1], 0.0f, 0.0f};
    static_cast<dx9rend::ParameterOwner*>(m_paramGraphSize)->setValue(graphSize, 0);
    const float* const firstViewport = view->getViewport();
    const float width = view->getViewport()[2];
    const float viewportSize[4] = {width, firstViewport[3], 0.0f, 0.0f};
    static_cast<dx9rend::ParameterOwner*>(m_paramViewPort)->setValue(viewportSize, 0);
    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, 0);
    dx9rend::rendererDrawPrimitive(static_cast<unsigned int>(count - 6), 3, 0);
    dx9rend::rendererDrawPrimitive(4, 3, static_cast<unsigned int>(count - 5));
    dx9rend::effectEnd(m_effect);

    const unsigned int previousColour = debugTextWriterGetColour(g_DebugTextWriter);
    debugTextWriterSetColour(g_DebugTextWriter, m_colors[0]);
    const int titleY = graphCoordinate(m_rect[1]) - 10;
    const int titleX = graphCoordinate(m_rect[0]) + 1;
    debugTextWriterQueueText(g_DebugTextWriter, titleX, titleY, m_name, 0);
    if (m_byte85)
    {
        const std::string value = formatString("%f", static_cast<double>(m_lastSample)) + m_string94;
        const int y = graphCoordinate(m_rect[1]) + m_dword90;
        const int x = graphCoordinate(m_rect[0]) + m_dword8C;
        debugTextWriterQueueText(g_DebugTextWriter, x, y, value, 0);
    }
    if (m_byte84)
    {
        const int x = graphRightCoordinate(m_rect[0], m_rect[2]) + m_legendX;
        int y = graphCoordinate(m_rect[1]) + m_legendY;
        for (unsigned int i = 0; i < m_legend.size(); ++i)
        {
            debugTextWriterSetColour(g_DebugTextWriter, m_legend[i].second);
            debugTextWriterQueueText(g_DebugTextWriter, x, y, m_legend[i].first, 0);
            y += 10;
        }
    }
    debugTextWriterSetColour(g_DebugTextWriter, previousColour);
    m_dwordC0 = static_cast<int>(static_cast<unsigned int>(m_dwordC0) + sampleBytes);
}

// RefFactory_geom_DebugLineGraph, 0x1007CC20.
IBase* __fastcall createDebugLineGraph(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugLineGraphD3D));
    DebugLineGraphD3D* self = storage ? new (storage) DebugLineGraphD3D() : 0;

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
