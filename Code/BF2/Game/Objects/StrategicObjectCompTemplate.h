// Strategic-object configuration and intact/destroyed map icons.
// Paths are authored strings; resolved textures are integer handles. [INFERRED] Source placement.
// [PLATFORM] String-containing layout differs between toolchains.

#ifndef GAME_OBJECTS_STRATEGICOBJECTCOMPTEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_STRATEGICOBJECTCOMPTEMPLATE_H

#include <cstdint>
#include <string>

#include "CompTemplateInterfaces.h"
#include "../../IO/Console/IConsoleSaveable.h"

namespace dice {
namespace hfe {
namespace world {

class IObjectComponent;

// Strategic-object names/icons use by-value setters and reference getters.
class StrategicObjectCompTemplate : public IStrategicObjectCompTemplate, public io::IConsoleSaveable
{
public:
    StrategicObjectCompTemplate(); // ELF 8228EB0 / BF2.exe 65E0E0
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;
    virtual unsigned int getClassID() const;
    virtual unsigned int getInterfaceID();
    virtual IObjectComponent* createComponent();
    virtual void  objectCreated(IObject*);
    virtual void  objectDestroyed(IObject*);
    virtual void  setTemplate(IObjectTemplate*);
    virtual void* queryImplementation(unsigned int);
    virtual void setStrategicObjectName(std::string);
    virtual const std::string& getStrategicObjectName();
    virtual void  setDestroyedIcon(std::string);                     // resolves
    virtual const std::string& getDestroyedIcon();
    virtual std::intptr_t getDestroyedIconPtr();                     // int on x86
    virtual void  setIntactIcon(std::string);                        // resolves
    virtual const std::string& getIntactIcon();
    virtual std::intptr_t getIntactIconPtr();                        // int on x86
    virtual void  setPart(int);                  virtual int  getPart();
    virtual void  set3dMapIcon(int);             virtual int  get3dMapIcon() const;
    virtual bool  has3dMapIcon() const;
    virtual void  setIsOnePiece(bool);           virtual bool getIsOnePiece() const;
    virtual ~StrategicObjectCompTemplate();
    virtual bool  makeScript(io::IStream*);                          // +0x70 last

protected:
    friend class StrategicObjectComp; // getHudInfo/getHudIcons return addresses within this payload
    /*  8 */ int   m_refCount;

    /* 12 */ std::string m_strategicObjectName;
    /* 16 */ int         m_part;

    /* 20 */ std::string m_destroyedIcon;         // authored path
    /* 24 */ std::string m_intactIcon;            // authored path
    // Resolved texture handles: std::intptr_t is int on x86 (offsets unchanged), pointer-wide on x64.
    /* 28 */ std::intptr_t m_destroyedIconPtr;
    /* 32 */ std::intptr_t m_intactIconPtr;

    /* 36 */ int   m_mapIcon3D;                   // console binding: strategicObject.3dMapIcon
    /* 40 */ bool  m_isOnePiece;
    /* 41 */ bool  _byte29;                       // [UNRESOLVED]
    /* 42 */ bool  _byte2A;
    /* 43 */ bool  _byte2B;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_STRATEGICOBJECTCOMPTEMPLATE_H
