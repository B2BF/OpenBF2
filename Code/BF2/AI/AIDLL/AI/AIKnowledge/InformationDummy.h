// Placeholder information record that is not backed by an engine object: it answers from its own
// position, transformation, side and name, and owns its plug-ins through Information. The
// contact slots (+6..+9) stay pure, so the class is abstract; nothing in libaidll.so constructs
// it or derives from it.
// Evidence: libaidll.so 1.5.3153 (ia-32) 0x2D9520..0x2D9F3C, vtable 0x4BDF80 (29 slots);
// FILE symbol InformationDummy.cpp.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONDUMMY_H
#define AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONDUMMY_H

#include <string>

#include "Information.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

class InformationDummy : public Information
{
public:
    // A dummy at `position` with `transformation`, of `side`, carrying the object types `types`;
    // named "DummyInformation".
    InformationDummy(Vec3& position, Mat4& transformation, int side, unsigned int types);
    // A dummy at `position` with a fixed orientation (rows (0,0,1), (0,1,0), (1,0,0)) and an
    // empty name.
    InformationDummy(Vec3& position, int side, unsigned int types);
    virtual ~InformationDummy();

    virtual int    getInformationSort() { return 2; }                   // +2 ELF 0x2D9DD0
    virtual bool   setTime(double) { return false; }                    // +3 ELF 0x2D9E40
    virtual bool   setTime(double, float) { return false; }             // +4 ELF 0x2D9E50
    virtual double getTimeStamp() const { return 0.0f; }                // +5 ELF 0x2D9E10
    // +6..+9 (addContact, removeContact, removeAllContacts, hasContact) stay pure.
    virtual float  getSecurity(double) { return 1.0f; }                 // +10 ELF 0x2D9E60
    virtual float  getSecurity() { return 1.0f; }                       // +11 ELF 0x2D9E80
    virtual Vec3&  getPosition() { return m_position; }                 // +12 ELF 0x2D9E30
    virtual float  getAltitude();                                       // +13
    virtual float  getAltitude(const Vec3& direction);                  // +14
    virtual Mat4&  getTransformation() { return m_transformation; }     // +15 ELF 0x2D9EA0
    virtual Vec3   getFaceing()                                         // +16 ELF 0x2D9EB0 the third row
    {
        Vec3 facing;
        facing.x = m_transformation.m[8];
        facing.y = m_transformation.m[9];
        facing.z = m_transformation.m[10];
        return facing;
    }
    virtual int    getSide() const { return m_side; }                   // +17 ELF 0x2D9ED0

    // New virtual slots. They hide Information's non-virtual forwarders of the same names.
    virtual const std::string& getName() const { return m_name; }                  // +18 ELF 0x2D9F20
    virtual const std::string& getAIName() const { return m_name; }                // +19 ELF 0x2D9F30
    virtual const std::string& getObjectTypeName() const { return m_name; }        // +20 ELF 0x2D9EE0
    virtual unsigned int getHandle() const { return 0xFFFFFFFFu; }                 // +21 ELF 0x2D9DE0
    virtual bool   isPrimary() const { return true; }                              // +22 ELF 0x2D9EF0
    virtual unsigned int getPrimaryObject(int) { return 0xFFFFFFFFu; }             // +23 ELF 0x2D9F00
    virtual unsigned int getSecondaryObject(int& iterator, int side);              // +24
    virtual unsigned int getFirstSecondaryObject(int& iterator, int side);         // +25
    virtual int    getFirstSecondaryObjectIterator() const { return -1; }          // +26 ELF 0x2D9F10
    virtual bool   getObjectSpawnerHolding() const { return false; }               // +27 ELF 0x2D9DF0
    virtual unsigned int getObjectSpawner(int) const { return 0xFFFFFFFFu; }       // +28 ELF 0x2D9E00

protected:
    // ELF layout; sizeof 0x98.
    /* +0x44 */ Vec3        m_position;
    /* +0x50 */ Mat4        m_transformation;
    /* +0x90 */ int         m_side;
    /* +0x94 */ std::string m_name;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONDUMMY_H
