// Menu culling node. [INFERRED] Source placement.
// Inherited unimplemented Node operations retain their diagnostic stops.
#include "Bf2CullNode.h"
#if !DEDICATED
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] concrete descriptor name. A233F4 is independent loader-zeroed
// storage; object construction/destruction does not adjust this counter.
int bf2CullNodeInstances;
class Bf2CullNodeTypeDescriptor final : public TypeDescriptor
{
public:
    Bf2CullNodeTypeDescriptor() : TypeDescriptor("dice::meme::Bf2CullNode") {}
    TypeDescriptor* getBaseType() override { return CullNode::getClassType(); }
    RuntimeObject* create() override
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Allocation and construction are real; publication retains Node33.
        void* allocation = ::operator new(sizeof(Bf2CullNode));
        return allocation ? new(allocation) Bf2CullNode(nullptr, nullptr) : nullptr;
    }
    // Inherited destructor/description are the recovered no-unlink/null bodies.
    void addInstance() override { ++bf2CullNodeInstances; }
    void removeInstance() override { --bf2CullNodeInstances; } // 7CE1C0, no zero guard
    int getInstanceCount() override { return bf2CullNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(Bf2CullNode); }
};

char* cloneCullNodeName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes); // no allocation-null guard in the original
    return copy;
}

// 7CE04A..7CE063 /7CE08F..7CE0A8: store the rounded value to the actual
// field before comparing the still-extended x87 result and possibly clamping.
// Names are [INFERRED]. Typed float storage avoids any cross-build field offset.
void decreaseCullFade(float& current, float delta, float duration)
{
#if defined(_M_IX86)
    const float zero = 0.0f;
    __asm {
        mov ecx, current
        fld delta
        fdiv duration
        fsubr dword ptr [ecx]
        fst dword ptr [ecx]
        fcomp zero
        fnstsw ax
        test ah, 5
        jp decreased
        mov dword ptr [ecx], 0
    decreased:
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // The rounded value is stored first; only an ordered negative result then becomes +0.
    const double unrounded = static_cast<double>(current) - static_cast<double>(delta) / duration;
    current = static_cast<float>(unrounded);
    if (unrounded < 0.0) current = 0.0f;
#endif
}
void increaseCullFade(float& current, float delta, float duration)
{
#if defined(_M_IX86)
    const float one = 1.0f;
    __asm {
        mov ecx, current
        fld delta
        fdiv duration
        fadd dword ptr [ecx]
        fst dword ptr [ecx]
        fcomp one
        fnstsw ax
        test ah, 41h
        jnz increased
        mov dword ptr [ecx], 3F800000h
    increased:
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // The rounded value is stored first; only an ordered result above one is then clamped.
    const double unrounded = static_cast<double>(delta) / duration + current;
    current = static_cast<float>(unrounded);
    if (unrounded > 1.0) current = 1.0f;
#endif
}
}

Bf2CullNode::Bf2CullNode(const char* name, Data* data) : m_name(nullptr)
{
    ::operator delete(m_name); // 737EE0, clear even the initially null name
    m_name = nullptr;
    m_name = cloneCullNodeName(name);
    m_data = data;
    m_previousVisibility = 0;
}
Bf2CullNode::~Bf2CullNode()
{
    ::operator delete(m_name);
    m_name = nullptr;
    // Successor and Data remain borrowed, without counter changes or unlinking.
}
TypeDescriptor* Bf2CullNode::getClassType()
{
    static Bf2CullNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* Bf2CullNode::getType() { return getClassType(); }
namespace {
// CRT96DAC4 ->86C5F0 eagerly constructs descriptorA233F8 and storesA2340C.
// The original eager body unconditionally sets its guard; native C++ shares
// the descriptor's function-local lifetime, registration and teardown.
TypeDescriptor* const registeredBf2CullNodeType = Bf2CullNode::getClassType();
}
unsigned int Bf2CullNode::getObjectSize() { return sizeof(Bf2CullNode); }
const char* Bf2CullNode::getName() { return m_name; }
bool Bf2CullNode::setName(const char* name)
{
    ::operator delete(m_name);
    m_name = nullptr;
    m_name = cloneCullNodeName(name); // retain the shipped free-before-copy aliasing defect
    return true;
}
bool Bf2CullNode::slot_0x24() { return true; }
Data* Bf2CullNode::getDataForHud() { return m_data; }
bool Bf2CullNode::setDataForHud(Data* data)
{ m_data = data; return true; }
bool Bf2CullNode::setOutTimeForHud(float value)
{ m_outTime = value; return true; }
bool Bf2CullNode::setInTimeForHud(float value)
{ m_inTime = value; return true; }

bool Bf2CullNode::update(System* system, const NodeState* state,
                        const Rect* bounds, float delta)
{
    if (!m_next) return true; // precedes even the optional Data sample
    int visible;
    if (m_data && m_data->getFloat(system) == 0.0f)
    {
        if (m_fade > 0.0f)
        {
            if (m_outTime == 0.0f) m_fade = 0.0f;
            else decreaseCullFade(m_fade, delta, m_outTime);
        }
        visible = 0;
    }
    else
    {
        if (m_fade < 1.0f)
        {
            if (m_inTime == 0.0f) m_fade = 1.0f;
            else increaseCullFade(m_fade, delta, m_inTime);
        }
        visible = 1;
    }

    if (m_previousVisibility != visible && visible == 0)
    {
        // original successor virtual+C0. Preserve the known Node
        // caller boundary; do not invent or index past its native33 prefix.
        if (!dispatchNodeEvent20(m_next, system, bounds, state))
        {
            if (m_next->getChild(0))
                dispatchNodeEvent20(m_next->getChild(0), system, bounds, state);
        }
    }
    if (m_fade != 0.0f)
    {
        if (m_previousVisibility != visible && visible == 1)
        {
            // original successor virtual+BC, the event21 caller stop.
            if (!dispatchNodeEvent21(m_next, system, bounds, state))
            {
                if (m_next->getChild(0))
                    dispatchNodeEvent21(m_next->getChild(0), system, bounds, state);
            }
        }
        m_previousVisibility = visible;
        return m_next->update(system, state, bounds, delta);
    }
    m_previousVisibility = visible;
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(Bf2CullNodeTypeDescriptor) == 16, "client Bf2CullNode metadata");
static_assert(sizeof(Bf2CullNode) == 32, "client Bf2CullNode storage");
#endif
} }
#endif
