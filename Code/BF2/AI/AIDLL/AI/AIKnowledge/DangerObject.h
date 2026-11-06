// Identity and position of a threat used by the cover behaviours.
// Source placement: the image's initializer names AI/AIDLL/AI/AIKnowledge/DangerObject.cpp.
#ifndef AI_AIDLL_AI_AIKNOWLEDGE_DANGEROBJECT_H
#define AI_AIDLL_AI_AIKNOWLEDGE_DANGEROBJECT_H

#include "FireObject.h"
#include "Information.h"
#include "../Main/InformationManager.h"

namespace dice { namespace hfe { namespace ai {

// The shipped tables contain five virtual slots and no virtual destructor. Objects are borrowed
// threat identities; their FireObject/Information pointees are never owned.
class DangerObject
{
public:
    virtual bool isFireObject() const = 0;
    virtual bool isInformation() const = 0;
    virtual bool getPosition(Vec3& position) = 0;
    // [INFERRED] void* for the untyped identity pointer compared by operator==.
    virtual void* getPointer() const = 0;

    // ELF 0x2206C0
    virtual bool operator==(const DangerObject& other) const
    {
        if (isInformation() && !other.isInformation())
            return false;
        if (isFireObject() && !other.isFireObject())
            return false;
        if (!getPointer())
            return false;
        void* pointer = getPointer();
        return pointer == other.getPointer();
    }
};

class FireDanger : public DangerObject
{
public:
    // [INFERRED] Inline construction: the cover bodies store null or their borrowed shot pointer.
    FireDanger() : m_fire(0) {}
    explicit FireDanger(FireObject* fire) : m_fire(fire) {}

    // ELF 0x220570
    virtual bool isFireObject() const { return true; }
    // ELF 0x220580
    virtual bool isInformation() const { return false; }
    // ELF 0x220610
    virtual bool getPosition(Vec3& position)
    {
        if (!m_fire)
            return false;
        position = m_fire->getOrigin();
        return true;
    }
    // ELF 0x220590
    virtual void* getPointer() const { return m_fire; }

    /* +0x04 */ FireObject* m_fire;
};

class InfoDanger : public DangerObject
{
public:
    // [INFERRED] Inline construction: the cover bodies store -1 or an information handle.
    InfoDanger() : m_information(0xFFFFFFFFu) {}
    explicit InfoDanger(unsigned int information) : m_information(information) {}

    // ELF 0x2205A0
    virtual bool isFireObject() const { return false; }
    // ELF 0x2205B0
    virtual bool isInformation() const { return true; }
    // ELF 0x220640
    virtual bool getPosition(Vec3& position)
    {
        Information* information = informationManager->getInformation(m_information);
        if (!information)
            return false;
        position = information->getPosition();
        return true;
    }
    // ELF 0x2205C0
    virtual void* getPointer() const
    {
        if (m_information == 0xFFFFFFFFu)
            return 0;
        return informationManager->getInformation(m_information);
    }

    /* +0x04 */ unsigned int m_information;
};

} } }

#endif
