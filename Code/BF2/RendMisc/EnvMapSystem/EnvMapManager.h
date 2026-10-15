// Path: implementation attested by RendDX9.dll 1.50 string10222428.
// PE-only renderer class, table102224FC. Names without RTTI are [INFERRED].
#ifndef BF2_REND_ENVMAPMANAGER_H
#define BF2_REND_ENVMAPMANAGER_H
#ifdef BF2_IMAGE_RENDDX9
#include "../../Core/IBase.h"
#include "../../Math/Vec3.h"
#include <string>
#include <cstddef>
#include <map>
#include <string.h>
#include <string>
#include <vector>
namespace dice { namespace hfe {
namespace dx9rend { struct EngineTexture; }
namespace Rend {
class EnvMapObject;
class EnvMapObjectTemplate;
struct EnvMapEntry
{
    Vec3 position;
    bool flag;
    unsigned char pad[3];
    dx9rend::EngineTexture* texture;
    EnvMapEntry();
};
#if defined(_M_IX86)
static_assert(sizeof(EnvMapEntry)==0x14,"EnvMap entry allocation");
#endif
// The template registry's key order: MSVCR71 `_stricmp`, as the shipped find/insert compare.
struct EnvMapNameLessNoCase
{
    bool operator()(const std::string& a,const std::string& b) const
    { return ::_stricmp(a.c_str(),b.c_str())<0; }
};
class EnvMapManager : public IBase
{
public:
    EnvMapManager();
    ~EnvMapManager(); // non-virtual: no destructor slot in this table
    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual void reset();
    virtual EnvMapObject* createObject(EnvMapObjectTemplate*);
    virtual void queueRemoval(EnvMapObject*);
    virtual EnvMapObjectTemplate* getTemplate(const std::string&); // +1C,101BFC60
    virtual void removeTemplate(EnvMapObjectTemplate*);         // +20,101BF0C0 retail empty
    virtual void setEnabled(bool);
    virtual bool getEnabled() const;
    virtual void setPerPixelEnabled(bool);                     // +2C,101C1F50
    virtual bool getPerPixelEnabled() const;
    virtual bool loadInfo(const std::string&);
    virtual void clear();
    void flushRemovals();
    void clearObjects();
    void clearMaps();
    void onPostLoad();
    void update(float);
    void removeEntry(EnvMapEntry*);
    // The editor's capture and writer. Non-virtual: the `generateEnvMaps` and
    // `save` console executors (101C21D7 / 101C2244) call them directly on the published manager
    // and discard the answer. [INFERRED] names, from those bindings.
    bool generateEnvMaps();
    bool save();

    int m_refCount;                             // +04
    std::vector<EnvMapObject*> m_objects;        // +08
    // +18, case-insensitive string -> template
    std::map<std::string,EnvMapObjectTemplate*,EnvMapNameLessNoCase> m_templates;
    std::vector<EnvMapObject*> m_pendingRemoval; // +24
    // +34/+40: 24-byte nodes (a four-byte key and a four-byte value) that no recovered path fills;
    // [UNRESOLVED] key and value types. +34 is cleared with the objects, +40 only on destruction.
    std::map<unsigned int,unsigned int> m_map34; // +34
    std::map<unsigned int,unsigned int> m_map40; // +40
    std::map<std::string,EnvMapEntry*> m_maps;   // +4C,string ->20-byte map record
    unsigned char m_untouched58[8];              // +58,constructor does not initialize
    unsigned short m_templateCount;             // +60
    bool m_enabled;                             // +62
    unsigned char m_pad63;
    dx9rend::EngineTexture* m_currentTexture;    // +64,borrowed
    bool m_perPixelEnabled;                     // +68
    unsigned char m_pad69[3];
};
extern IBase* g_EnvMapManager;
IBase* __fastcall createEnvMapManager(unsigned int iid,IBase* outer);
} } }
#endif
#endif
