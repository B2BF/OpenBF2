// Shared view of the geometry-template registry. The concrete manager is private.
// Reserved entries preserve ABI positions and must not be called.

#ifndef BF2_GEOM_GEOMETRYTEMPLATEMANAGER_H
#define BF2_GEOM_GEOMETRYTEMPLATEMANAGER_H

#include "../../RendDX9Abi/HfeAbi.h"
#include <string>
#include "../Core/IBase.h"

#include <map>

namespace dice {
namespace hfe {
namespace geom {

class IGeometryTemplate;

// The registry's key order: `_stricmp` over the two names, the instruction-proven comparison. A
// name keeps the spelling it was first registered with. Defined in GeometryTemplateManager.cpp.
struct GeometryTemplateNameLess
{
    bool operator()(const std::string& a, const std::string& b) const;
};

// name -> owning template pointer, in name order.
typedef std::map<std::string, IGeometryTemplate*, GeometryTemplateNameLess>
    GeometryTemplateRegistry;

#define BF2_GTM_RESERVED(offset) virtual void reserved_##offset() = 0

class IGeometryTemplateManager : public IBase
{
public:
    // Cross-image methods use HostString, then convert once to the internal string storage.
    // Do not expose std::string directly to a native-host caller.
    virtual void registerTemplate(IGeometryTemplate* value,
                                  const ::renddx9abi::HostString& name) = 0;    // +0x10
    virtual IGeometryTemplate* createTemplate(unsigned int classId,
                                              const ::renddx9abi::HostString& name) = 0; // +0x14
    virtual IGeometryTemplate* getTemplate(const ::renddx9abi::HostString& name) = 0;    // +0x18
    virtual void setActive(IGeometryTemplate* value) = 0;                        // +0x1C
    virtual IGeometryTemplate* getActive() const = 0;                            // +0x20
    virtual void removeTemplate(const ::renddx9abi::HostString& name) = 0;       // +0x24
    virtual void clear() = 0;                                                    // +0x28

    BF2_GTM_RESERVED(2C); BF2_GTM_RESERVED(30); BF2_GTM_RESERVED(34);
    BF2_GTM_RESERVED(38);
    // PE 0x1015D9C0/0x1015D7C0/0x1015D740, called by ResourceManager's load lifecycle.
    // Names are [INFERRED]; signatures and the template-use byte at +4 are instruction-proven.
    virtual void loadUsedTemplates() = 0;                                // +0x3C
    virtual void resetTemplateUse() = 0;                                 // +0x40
    virtual int getUsedTemplateCount() = 0;                              // +0x44
    BF2_GTM_RESERVED(48); BF2_GTM_RESERVED(4C);
    BF2_GTM_RESERVED(50); BF2_GTM_RESERVED(54); BF2_GTM_RESERVED(58);
    BF2_GTM_RESERVED(5C);
    // Returns the case-insensitive name/template registry for ordered traversal.
    virtual GeometryTemplateRegistry* getTemplateRegistry() = 0;           // +0x60

protected:
    ~IGeometryTemplateManager() {}
};

// ModuleGeom.cpp, dword_1024B8F4 (G1 verifier: 0x1024B910 is
// g_RoadEditableRenderer). The concrete object is created by createGeometryTemplateManager.
extern IBase* g_GeometryTemplateManager;

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_GEOMETRYTEMPLATEMANAGER_H
