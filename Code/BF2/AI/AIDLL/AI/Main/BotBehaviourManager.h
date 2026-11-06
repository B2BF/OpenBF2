// Owns bot behavior definitions and their console configuration.

#ifndef AI_AIDLL_AI_MAIN_BOTBEHAVIOURMANAGER_H       // [INFERRED] guard style
#define AI_AIDLL_AI_MAIN_BOTBEHAVIOURMANAGER_H

#include <map>
#include <string>
#include <vector>

#include "../../../../IO/File/IFileSystem.h"     // hfe::NoCaseStringCompare - all three maps use it

namespace dice {
namespace hfe {
namespace ai {

class BotBehaviour;
class UrgeCurve;

// Factory interface for constructing bot behaviors.
class BBCreateFunction
{
public:
    // Returns the new behaviour (`movl %edi, %eax` after `new BB...`); an unknown pattern name
    // still builds the behaviour, with a null pattern.
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                                     UrgeCurve* urgeCurve, float* weights) = 0;
};

class BotBehaviourManager
{
public:
    BotBehaviourManager();
    ~BotBehaviourManager();                         // , not virtual

    // The only virtual, slot 0. The body materialises 1 and both callers (the two destructors)
    // discard it, so `bool` is [INFERRED] - chosen to match `AIConsole::reset`, which has the same
    // one-virtual-no-virtual-destructor shape in the same init sequence.
    virtual bool reset();

    // the behaviour name/id table
    void setBehaviourName(int id, std::string name);
    int getBehaviourId(std::string name);               // - -1 when unknown
    std::string getBehaviourName(int id);               // - "" when out of range

    // the factory dispatch
    // Called by `AIConsole::setVehicleBehaviour` , which is the `.ai` script binding.
    // Returns the created behaviour, or null with `message` set (AIConsole::setVehicleBehaviour
    // registers a non-null result).
    BotBehaviour* create(std::string behaviourName, std::string createFunctionName, std::string name,
               int mode, std::string urgeCurveName, float* weights, std::string& message);

    // the urge-curve factories
    // Each returns a human-readable confirmation string that the console prints; none reports
    // failure, because `std::map::operator[]` cannot fail and the allocation is unchecked.
    std::string createUCConstant(std::string& name, float value);
    std::string createUCLinear(std::string& name, float slope, float offset);
    std::string createUCXInverse(std::string& name, float numerator, float offset,
                                 float slope, float bias);

    // Set by the constructor, cleared by the destructor. `BF2AI::clear` deletes through it.
    static BotBehaviourManager* m_instance;

private:
    // Offsets describe ELF layout; native STL sizes depend on the compiler.
    // [INFERRED] Member names follow their factory and lookup roles.
    /*  +4 */ std::map<std::string, BBCreateFunction*, NoCaseStringCompare> m_createFunctions;
    /* +16 */ std::map<std::string, int, NoCaseStringCompare>               m_behaviourIds;
    /* +28 */ std::map<std::string, UrgeCurve*, NoCaseStringCompare>        m_urgeCurves;
    /* +40 */ std::vector<std::string>                                      m_behaviourNames;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_BOTBEHAVIOURMANAGER_H
