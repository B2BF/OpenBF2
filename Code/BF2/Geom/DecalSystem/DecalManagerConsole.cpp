// Native DecalManager console bindings. [INFERRED] companion source placement.
// Commands require the manager singleton. Preserve the trailing 'l' in
// startFadePercentageOfFadeDistancel and the existing no-op setter behavior.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/PluginAbi.h"


#include "DecalManager.h"

#include "../../Core/ConsoleObjectMacro.h"
#include "../../Core/ConsoleObjects.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include <string>

#include <string>

namespace dice {
namespace hfe {
namespace geom {

namespace {

const char kDecalManagerConsoleTag[] = "geom.decalManager";

// g_DecalManager is published as an IBase*; DecalManager derives from IDecalManager (an IBase)
// first, so the downcast adjusts nothing.
inline DecalManager* decalManager()
{
    return static_cast<DecalManager*>(g_DecalManager);
}

// GET `m_ret = sub_10124140()`, SET `nullsub_9(value)`.
bool getDrawStats() { return decalManager()->getDrawStats(); }
void setDrawStats(bool value) { decalManager()->setDrawStats(value); }

// the executors load and store the fields themselves.
float getDecalNormalOffset() { return decalManager()->decalNormalOffset(); }                // +0x2C0
void  setDecalNormalOffset(float value) { decalManager()->setDecalNormalOffsetField(value); }
float getDecalGeomClipAngle() { return decalManager()->decalGeomClipAngle(); }              // +0x2C4
void  setDecalGeomClipAngle(float value) { decalManager()->setDecalGeomClipAngleField(value); }

// The four slot-backed rows, through IDecalManager's typed table.
float getFadeDistance() { return decalManager()->getFadeDistance(); }                        // +0x40
void  setFadeDistance(float value) { decalManager()->setFadeDistance(value); }               // +0x3C
float getStartFadePercentage() { return decalManager()->getStartFadePercentageOfFadeDistance(); } // +0x48
// +0x44 - the executor pushes the parsed float (0x100BC5B5 `fstp [esp]`); the body never stores it.
void  setStartFadePercentage(float value)
{
    decalManager()->setStartFadePercentageOfFadeDistance(value);
}
float getTimeToLive() { return decalManager()->getTimeToLive(); }                            // +0x38
void  setTimeToLive(float value) { decalManager()->setTimeToLive(value); }                   // +0x34
float getFadeOutPercentage() { return decalManager()->getFadeOutAtPercentageOfTimeToLive(); } // +0x50
void  setFadeOutPercentage(float value) { decalManager()->setFadeOutAtPercentageOfTimeToLive(value); } // +0x4C

// with no argument: generateTexture(std::string("DecalAtlas")); the bool is the reply.
bool generateDecalAtlas()
{
    return decalManager()->generateTexture(std::string("DecalAtlas"));
}

BF2_CONSOLE_OBJECT("DecalManager", 2, 0)

BF2_CONSOLE_PROPERTY_FN(drawStats, bool, decalManager() != 0,
                        getDrawStats(), setDrawStats, kDecalManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(decalNormalOffset, float, decalManager() != 0,
                        getDecalNormalOffset(), setDecalNormalOffset, kDecalManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(decalGeomClipAngle, float, decalManager() != 0,
                        getDecalGeomClipAngle(), setDecalGeomClipAngle, kDecalManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(fadeDistance, float, decalManager() != 0,
                        getFadeDistance(), setFadeDistance, kDecalManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(startFadePercentageOfFadeDistancel, float, decalManager() != 0,
                        getStartFadePercentage(), setStartFadePercentage, kDecalManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(timeToLive, float, decalManager() != 0,
                        getTimeToLive(), setTimeToLive, kDecalManagerConsoleTag)
BF2_CONSOLE_PROPERTY_FN(fadeOutAtPercentageOfTimeToLive, float, decalManager() != 0,
                        getFadeOutPercentage(), setFadeOutPercentage, kDecalManagerConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(generateTexture, generateDecalAtlas, bool, decalManager() != 0,
                                kDecalManagerConsoleTag)

} // anonymous namespace

// ModuleGeom::init / close must call these beside the other console lists (integration change).
bool registerDecalManagerConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kDecalManagerConsoleTag));
}

bool unregisterDecalManagerConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kDecalManagerConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
