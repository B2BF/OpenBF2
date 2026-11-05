// AI information record for an engine object. Owns its attached information plug-ins.
// Position and transformation accessors return mutable references; preserve the declared return types.
// InformationDummy introduces virtual methods that hide nonvirtual methods on this base.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_INFORMATION_H        // [INFERRED] guard style
#define AI_AIDLL_AI_AIKNOWLEDGE_INFORMATION_H

#include <string>

#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/SoldierPose.h"
#include "../../../AIInterface/BFKnowledge/InformationType.h"

namespace dice {
namespace hfe {

struct Mat4;                                         // Math/Mat4.h - a struct

namespace ai {

class IAIObject;
class IPIArmament;
class IPIControlInfo;
class IPICover;
class IPIMobile;
class IPIPhysical;
class IPIUnit;

class Information
{
public:
    // The owner and the time; not booked
    // (-1); no plug-in. The +0x04..+0x14 prefix is left to the subclass constructors.
    Information(IAIObject* object, double time);

    // the information owns its six plug-INS: each present
    // one is destroyed, in the order physical, cover, mobile, armament, unit, control info.
    virtual ~Information();                          // +0 / +1

    // +2 - the only non-pure slot: 3 in the base (, ).
    virtual int  getInformationSort();

    virtual bool   setTime(double time) = 0;                            // +3 bool (corrected)
    virtual bool   setTime(double time, float f) = 0;                   // +4 bool (corrected)
    virtual double getTimeStamp() const = 0;                            // +5

    // Slots 6..9 are pure in `InformationDummy` as well - Real answers them emptily / true.
    virtual void   addContact() = 0;                                    // +6
    virtual void   removeContact() = 0;                                 // +7
    virtual void   removeAllContacts() = 0;                             // +8
    virtual bool   hasContact() const = 0;                              // +9

    virtual float  getSecurity(double time) = 0;                        // +10
    virtual float  getSecurity() = 0;                                   // +11 float: SAI::updateStrengths paired caller

    // slot 12 - the one `AIInformationGrid` calls. Non-const in the mangled name.
    virtual Vec3&  getPosition() = 0;                                   // +12

    virtual float  getAltitude() = 0;                                   // +13
    virtual float  getAltitude(const Vec3& direction) = 0;              // +14
    virtual Mat4&  getTransformation() = 0;                             // +15
    virtual Vec3   getFaceing() = 0;                                    // +16 dice's spelling
    virtual int    getSide() const = 0;                                 // +17 int: paired side comparison

    // the plug-ins
    // removePlugIn's case numbers are the plug-ins' type ids: 0 physical, 1 cover, 2 mobile, 3 unit
    // 4 armament, 5 control info (IPIPhysical::getType answers 0, IPIUnit::getType 3).
    bool removePlugIn(int type);
    void setTimeOnPlugIns(double time);
    bool setArmament(IPIArmament* armament);                            // first wins
    void borrowArmament(Information* lender);
    void resetArmament();
    bool setControlInfo(IPIControlInfo* controlInfo);                   // first wins
    bool setCover(IPICover* cover);                                     // first wins
    bool setMobile(IPIMobile* mobile);                                  // first wins
    bool setPhysical(IPIPhysical* physical);                            // first wins
    bool setUnit(IPIUnit* unit);                                        // first wins

    // forwards to the owning engine object (IAIObject)
    const std::string& getObjectTypeName() const;                       // +0x58
    unsigned int getObjectId() const;                                   // +0xE4
    int getPlayerId() const;                                            // +0x148
    bool isPrimary() const;                                             // +0xF8
    unsigned int getPrimaryObject(int side);                            // +0xFC, twice
    unsigned int getSecondaryObject(int& iterator, int side);           // +0x114
    unsigned int getFirstSecondaryObject(int& iterator, int side);      // +0x11C
    int getFirstSecondaryObjectIterator() const;                        // +0x124
    const std::string& getName() const;                                 // +0x50
    const std::string& getAIName() const;                               // +0x54
    bool getObjectSpawnerHolding() const;                               // +0x138
    unsigned int getObjectSpawner(int side) const;                      // +0x13C
    unsigned int getHandle() const;                                     // sides 1..2
    int getKitType() const;                                             // +0xF4
    bool isRootObject() const;                                          // +0x140
    SoldierPose getSoldierPose() const;                                 // +0xEC

    // vehicle booking
    int getBookingPlayerId();
    bool isBooked();
    bool isBookedBy(int playerId);
    void bookVehicle(int playerId);
    void cancelBookVehicle();

    // inlined readers (no symbols: every caller inlines the load)
    // [INFERRED] names; the fields are pinned by the setters and constructors above.
    IPIPhysical* getPhysical() const { return m_physical; }
    IPIUnit* getUnit() const { return m_unit; }
    unsigned int getObjectTypeFlags() const { return m_objectTypeFlags; }
    // DICE's name and form: AIDLL_r.dll (BoosterPack2 debug build) asserts
    // "!targetPrimaryInfo->isOfType(ITSoldier)" (BBPChange.cpp, 0x100FD302) and inlines it as
    // `(types >> n) & 1` (shr 0x15 / test 1); "Object has no cover plugin but is of type ITCover"
    // (BFInformationEnvironment.cpp, 0x101A8419) tests bit 14. Bit n is InformationType n
    // (AIDLL_r's enum-to-string switch 0x100D3010, jump table 0x100D32EC: 0 ITProduction .. 26
    // ITHasCarHorn). Both retail images expand it inline as a single-bit test.
    bool isOfType(InformationType type) const { return ((m_objectTypeFlags >> type) & 1) != 0; }
    float getStrength() const { return m_strength; }
    // BotMain::updateBotVehicle (ELF 0x18BCC0) scales +0x10 inline (x0.75 / x1.3333334).
    void setStrength(float strength) { m_strength = strength; }
    // [INFERRED] name; SAIMap::updateInformation (ELF 0x3641F0) copies +0x14 into +0x10 inline.
    float getBasicTemp() const { return m_basicTemp; }
    bool hasMobile() const { return m_mobile != nullptr; }
    IPIMobile* getMobile() const { return m_mobile; }
    // [INFERRED] inline reader of +0x34 (InfoCoverPredicate 0x2E7E80 reads it without a call).
    IPICover* getCover() const { return m_cover; }
    IAIObject* getAIObject() const { return m_object; }
    bool hasArmament() const { return m_armament != nullptr; }
    // [INFERRED] source-level inline accessor. Both retail images read the same +0x2C member in
    // InformationFactory::updateInformationPlugin before the virtual updateWeapons call.
    IPIArmament* getArmament() const { return m_armament; }
    IPIControlInfo* getControlInfo() const { return m_controlInfo; }

protected:
    /* +0x00 */ // vptr
    // Offsets describe ELF. PE members are four bytes higher through +0x18 and eight higher
    // from the first double onward because of alignment. Original sizes: ELF 68, PE 80 bytes.
    /* +0x04 */ unsigned int  m_objectTypeFlags;  // [INFERRED] name; InformationReal: the template's types (InformationType bits, see isOfType)
    /* +0x08 */ float         m_allowedTimeDiff;  // [INFERRED] name; InformationReal's constructor
    /* +0x0C */ unsigned char m_unwritten0C[4];   // not written by InformationReal
    /* +0x10 */ float         m_strength;         // [INFERRED] name; InformationReal: the template's basicTemp
    /* +0x14 */ float         m_basicTemp;        // [INFERRED] name; InformationReal: basicTemp again
    /* +0x18 */ double        m_time;             // the constructor's time
    /* +0x20 */ IAIObject*    m_object;           // the constructor's owner
    /* +0x24 */ bool          m_isBooked;           // constructed false; bookVehicle / cancelBookVehicle  // name: AIDLL_r asserts "m_isBooked != false" / "== false" (0x10161E70)
    /* +0x28 */ int           m_bookingPlayerId;  // constructed -1
    /* +0x2C */ IPIArmament*    m_armament;       // type 4
    /* +0x30 */ IPIControlInfo* m_controlInfo;    // type 5
    /* +0x34 */ IPICover*       m_cover;          // type 1
    /* +0x38 */ IPIMobile*      m_mobile;         // type 2
    /* +0x3C */ IPIPhysical*    m_physical;       // type 0
    /* +0x40 */ IPIUnit*        m_unit;           // type 3
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_INFORMATION_H
