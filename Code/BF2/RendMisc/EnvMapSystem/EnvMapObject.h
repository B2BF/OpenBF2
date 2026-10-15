// Implementation path attested at RendDX9 10222978. PE-only renderer objects;
// no dedicated ELF equivalents. Accessor names except getName are [INFERRED].
#ifndef BF2_REND_ENVMAPOBJECT_H
#define BF2_REND_ENVMAPOBJECT_H
#ifdef BF2_IMAGE_RENDDX9
#include <string>
#include "../../Math/Mat4.h"
#include <cstddef>
namespace dice { namespace hfe { namespace Rend {
struct EnvMapEntry;
class EnvMapObjectTemplate
{
public:
    explicit EnvMapObjectTemplate(const std::string& name);
    virtual const std::string& getName() const;
    virtual void setSize(float);                           // +04,101C43C0; ELF caller82025F0
    virtual float getSize() const;
    virtual void setIsStatic(bool);                        // +0C,10193820; ELF caller8202650
    virtual bool getIsStatic() const;
    virtual ~EnvMapObjectTemplate();
    float m_size;
    bool m_isStatic;
    unsigned char m_pad09[3];
    std::string m_name;
};
class EnvMapObject
{
public:
    explicit EnvMapObject(EnvMapObjectTemplate*);
    virtual void setEnabled(bool);
    virtual bool getEnabled() const;
    virtual bool update(const Mat4&,EnvMapObjectTemplate*,void*); // +08,101C4180; last arg discarded
    virtual void clear();                                 // +0C,101C4100
    virtual unsigned int getCount() const;
    virtual void release();
    virtual ~EnvMapObject();
    bool m_enabled;                                      // +04
    unsigned char m_pad05[3];
    EnvMapObjectTemplate* m_template;                     // +08
    EnvMapEntry* m_entry;                                 // +0C
    unsigned int m_value10;                               // +10,ctor1; role unresolved
    unsigned int m_count;                                 // +14
    unsigned int m_limit;                                 // +18,ctor20
};
#if defined(_M_IX86)
static_assert(offsetof(EnvMapObjectTemplate,m_name)==0x0C,"template name offset");
static_assert(sizeof(EnvMapObject)==0x1C,"101C4150 object allocation");
static_assert(offsetof(EnvMapObject,m_entry)==0x0C,"entry pointer offset");
#endif
} } }
#endif
#endif
