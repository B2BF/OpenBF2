// Sensed information about an object of another side. While the owning side has contact, the
// position and transformation are read live from the engine object; when the last contact goes,
// they are frozen at the last seen values and the record's security decays with time.
// Owns its attached plug-ins (through Information).
// [INFERRED] Source placement beside InformationReal.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONKNOWN_H
#define AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONKNOWN_H

#include "Information.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

class InformationKnown : public Information
{
public:
    // `degeneration` scales the security decay, `basicTemp` and `allowedTimeDiff` fill the base
    // prefix as in InformationReal, `ownerSide` is the side that knows the object.
    InformationKnown(IAIObject* object, unsigned int types, double time, float degeneration,
                     float basicTemp, float allowedTimeDiff, int ownerSide);
    virtual ~InformationKnown();

    virtual int    getInformationSort();                                // +2 - 1
    virtual bool   setTime(double time);                                // +3
    virtual bool   setTime(double time, float security);                // +4
    virtual double getTimeStamp() const;                                // +5 - the last update
    virtual void   addContact();                                        // +6
    virtual void   removeContact();                                     // +7
    virtual void   removeAllContacts();                                 // +8
    virtual bool   hasContact() const;                                  // +9
    virtual float  getSecurity(double time);                            // +10
    virtual float  getSecurity();                                       // +11
    virtual Vec3&  getPosition();                                       // +12
    virtual float  getAltitude();                                       // +13
    virtual float  getAltitude(const Vec3& direction);                  // +14
    virtual Mat4&  getTransformation();                                 // +15
    virtual Vec3   getFaceing();                                        // +16
    virtual int    getSide() const;                                     // +17 - the cached side

    // Non-virtual: updates to the current game time.
    bool setTime();
    int  getOwnerSide();
    void setOwnerSide(int side);

private:
    // [INFERRED] helper: the same inline copy appears in the constructor, setTime and both
    // contact removals.
    void storeLastSeen();

    /* +0x44 */ double m_lastUpdate;           // [INFERRED] name; getTimeStamp
    /* +0x4C */ float  m_degeneration;         // [INFERRED] name; divides the age in getSecurity
    /* +0x50 */ Vec3   m_lastPosition;         // [INFERRED] name
    /* +0x5C */ Mat4   m_lastTransformation;   // [INFERRED] name
    /* +0x9C */ int    m_side;                 // the object's side (IAIObject +0x12C)
    /* +0xA0 */ int    m_ownerSide;
    /* +0xA4 */ int    m_contacts;             // [INFERRED] name; add/removeContact
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONKNOWN_H
