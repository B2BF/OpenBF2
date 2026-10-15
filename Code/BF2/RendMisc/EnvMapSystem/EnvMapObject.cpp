// Path attested by RendDX9 1.50 __FILE__ at10222978. Full instruction bodies read
// for tables10222938/102229F0, constructors101C4150/101C4380, update101C4180.
#ifdef BF2_IMAGE_RENDDX9
#include "EnvMapObject.h"
#include "EnvMapManager.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include <cfloat>
#include <cmath>
#include <new>
namespace dice { namespace hfe { namespace Rend {

EnvMapObjectTemplate::EnvMapObjectTemplate(const std::string& name) : m_name(name)
{
    setSize(1.0f);
    setIsStatic(true);
}
const std::string& EnvMapObjectTemplate::getName() const { return m_name; }
void EnvMapObjectTemplate::setSize(float value) { m_size=value; }
float EnvMapObjectTemplate::getSize() const { return m_size; }
void EnvMapObjectTemplate::setIsStatic(bool flag) { m_isStatic=flag; }
bool EnvMapObjectTemplate::getIsStatic() const { return m_isStatic; }
EnvMapObjectTemplate::~EnvMapObjectTemplate() {}

EnvMapObject::EnvMapObject(EnvMapObjectTemplate* objectTemplate)
    : m_enabled(true),m_template(objectTemplate),m_entry(0),m_value10(1),m_count(0),m_limit(20)
{}
void EnvMapObject::setEnabled(bool enabled) { m_enabled=enabled; }
bool EnvMapObject::getEnabled() const { return m_enabled; }
unsigned int EnvMapObject::getCount() const { return m_count; }
EnvMapObject::~EnvMapObject() {} // Retail destructor does not clear/free the entry.

bool EnvMapObject::update(const Mat4& transform,EnvMapObjectTemplate* objectTemplate,void*)
{
    // Shipped rounding order is Y, X, Z. The third transform argument is unused.
    const float y=static_cast<float>(std::floor(static_cast<double>(transform.m[13])));
    const Vec3 position={static_cast<float>(std::floor(static_cast<double>(transform.m[12]))),
        y,static_cast<float>(std::floor(static_cast<double>(transform.m[14])))};
    if(m_entry)
    {
        // 10037DC0 componentwise epsilon equality, including unordered refusal.
        if(!(std::fabs(m_entry->position.x-position.x)<=FLT_EPSILON &&
             std::fabs(m_entry->position.y-position.y)<=FLT_EPSILON &&
             std::fabs(m_entry->position.z-position.z)<=FLT_EPSILON))
            m_entry->position=position;
    }
    else if(m_count>=m_limit)
    {
        // [CHOSEN] Renderer diagnostic sink in place of the original Debug(type6)
        // stream at EnvMapObject.cpp:77; retain the shipped text and return branch.
        ::renddx9abi::logf("RendMisc/EnvMapSystem/EnvMapObject.cpp:77 Exceeding maximum amout of envMaps %u",m_limit);
    }
    else
    {
        void* const storage=::renddx9abi::engineAlloc(sizeof(EnvMapEntry));
        m_entry=storage ? new(storage) EnvMapEntry : 0;
        if(m_entry)
        {
            m_count=1;
            m_entry->flag=objectTemplate->getIsStatic();
            m_entry->position=position;
        }
    }
    return m_entry!=0;
}
void EnvMapObject::clear()
{
    static_cast<EnvMapManager*>(g_EnvMapManager)->removeEntry(m_entry);
    m_entry=0;
    m_count=0;
}
void EnvMapObject::release()
{
    // 101C4120 repeats the clear operations directly, then makes the +18 call.
    static_cast<EnvMapManager*>(g_EnvMapManager)->removeEntry(m_entry);
    m_entry=0;
    m_count=0;
    static_cast<EnvMapManager*>(g_EnvMapManager)->queueRemoval(this);
}
} } }
#endif
