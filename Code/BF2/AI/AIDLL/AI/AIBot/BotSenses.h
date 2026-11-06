// Per-bot sight, hearing and communication state. Owns a view frustum and sensed-object records.

#ifndef AI_AIDLL_AI_AIBOT_BOTSENSES_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_BOTSENSES_H

#include <list>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "../../../../Math/Frustum.h"

namespace dice {
namespace hfe {
namespace ai {

class BotMain;
class IAIObject;
class Information;
// The two things a bot's perception owns and deletes (AIBot/SensedObject.h, AIKnowledge/FireObject.h).
class FireObject;
class SensedObject;

class BotSenses
{
public:
    // (`operator new(0x94)` in SensingAgent's constructor).
    explicit BotSenses(BotMain* owner);
    ~BotSenses();
    void reset();                            // - (two arms frontier)

    // attested: an 11-byte body that returns `this + 112`.
    const std::map<unsigned int, SensedObject*>& getSensedObjects() const { return m_sensedObjects; }

    typedef std::pair<FireObject*, unsigned int>  FireEntry;

    // ELF 0x198420 (6421 bytes) [FRONTIER].
    void see(const double& slice);
    // ELF 0x19a040: hearing, fire objects, then seeing with the remaining slice.
    void updateSensing(const double& slice);
    void updateHearing(const double& slice);      // ELF 0x19be60 [FRONTIER]
    void updateFireObjects(const double& slice);  // ELF 0x19c460 [FRONTIER]
    void updateSeeing(const double& slice);       // ELF 0x19a0f0 [FRONTIER]
    // ELF 0x19d8a0: the record for a handle, or null.
    SensedObject* getSensedObjectFromHandle(unsigned int handle) const;
    // ELF 0x19C930: the attacker map's fire time for a shooter, else -1000.0.
    bool isAttacking(unsigned int handle, double& time) const;
    // ELF 0x19C9A0: queue a reported shot; an enemy shooter's primary object counts as seen.
    void event_projectile(FireObject* fire, unsigned int handle);
    // ELF 0x19CB60: a seen object (handle, seen position, seen part, information, direct, flag40 off).
    void event_seenObject(unsigned int handle, const Vec3& position, unsigned int part,
                          Information* information, bool direct, bool notFlag40);
    // ELF 0x19D270: an engine sound event for a handle.
    void event_heardObject(unsigned int handle);
    // ELF 0x19D8F0: the hearing probability of an object, or 0 when not heard.
    float hear(unsigned int handle, Information* information);
    // ELF 0x19DD70: drop a handle from the seen list.
    void forgetObject(unsigned int handle);
    // ELF 0x19DF50: `object`'s primary information is within 200 m and in front of `information`'s
    // primary object.
    bool isCloseAndInFront(Information* information, IAIObject* object) const;
    // ELF 0x18b0c0..0x18b120 (BotMain's forwarders read these fields inline).
    std::list<FireEntry>* getIncommingFire() { return &m_list84; }
    float getIncommingFireTotalStrength() const { return m_fireTotalStrength; }
    float getIncommingFireTotalStrengthAgainstSelf() const { return m_fireTotalStrengthAgainstSelf; }
    float getAttackerStrength() const { return m_attackerStrength; }

private:
    // BotMain::getAttackers (ELF 0x18B180) returns the map at +100 inline.
    friend class BotMain;
    typedef std::pair<double, SensedObject*>      TimedObject;   // a time and a thing
    typedef std::list<TimedObject>                TimedList;
    typedef std::map<unsigned int, SensedObject*> SensedObjectMap;

    /*   0 */ BotMain* m_bot;                // the ctor argument, and the first store
    /*   4 */ Frustum* m_frustum;            // new(0x1E8) - . See the header block.
    // see (ELF 0x198420): 1 when the equipment is ITObstructedView (bit 22, AIDLL_r enum switch 0x100D3010); the
    // row of tweak_frustumUpdateAngle used for the frustum.
    /*   8 */ int m_viewMode;
    /*  12 */ double m_collectTime;          // = -1000000.0; see: the game time of the last candidate scan
    // the distance sort buffer - what `InfoDistPred` and `partial_sort` operate on. Held as a
    // member rather than a local, so the seeing path does not reallocate every tick.
    /*  20 */ std::vector<std::pair<float, unsigned int> > m_sortBuffer;
    /*  32 */ int m_frustumStep;             // ctor/reset 0; see cycles it 0 -> 1 -> 2 -> 0
    /*  36 */ int m_seeState;                // ctor/reset 0; see: 0 scan, 1 sort, 2 test, 3 done
    /*  40 */ int m_seeIndex;                // ctor/reset 0; see: the next sort-buffer entry to test
    // handle -> position in the list at +56. See the header block.
    /*  44 */ std::map<unsigned int, TimedList::iterator> m_map44;
    /*  56 */ TimedList m_list56;
    // A cursor into the list above, seeded to end by the constructor's last statement.
    /*  60 */ TimedList::iterator m_cursor60;
    /*  64 */ std::set<unsigned int> m_hearingMemHelperSet;   // name: AIDLL_r assert 0x101D6BF0 (PE +0x50)
    /*  76 */ TimedList m_hearingMemCheckQueue;   // name: AIDLL_r assert 0x101D6BF0 (PE +0x5C)
    // A cursor into the list above - and unlike +60 the constructor leaves it default-constructed.
    // Only `reset` makes it valid. See the header block.
    /*  80 */ TimedList::iterator m_cursor80;
    /*  84 */ std::list<FireEntry> m_list84;
    // Floats: BotMain::getIncommingFireTotalStrength (ELF 0x18b0e0) and
    // ...AgainstSelf (0x18b100) return `flds 0x58/0x5c(senses)`, getAttackerStrength (0x18b120)
    // `flds 0x60`. The zero stores of ctor/reset are the same bits.
    /*  88 */ float m_fireTotalStrength;            // ctor: 0, reset: 0
    /*  92 */ float m_fireTotalStrengthAgainstSelf; // ctor: 0, reset: 0
    /*  96 */ float m_attackerStrength;             // not written by ctor/reset
    /* 100 */ std::map<unsigned int, FireObject*> m_map100;
    /* 112 */ SensedObjectMap m_sensedObjects;
    // the only two containers `reset` does not empty. Element width 4 and trivially destructible
    // (the destructor deallocates without a per-element loop); the element type is [INFERRED].
    /* 124 */ std::vector<IAIObject*> m_candidates;  // see: getWorldPlayerOccupiedObjects' output
    /* 136 */ std::vector<unsigned int> m_vector136;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_BOTSENSES_H
