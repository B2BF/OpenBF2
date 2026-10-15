// Hemi-map console bindings. setBaseHemiMap accepts three or four arguments, defaulting
// the fourth to 500, and exposes no return value despite its Boolean metadata.

#ifdef BF2_IMAGE_RENDDX9

#include "HemiMapManager.h"

#include "../../Core/ConsoleObjectMacro.h"
#include "../../Core/ConsoleObjects.h"
#include "../../../RendDX9Abi/PluginEntry.h"


namespace dice {
namespace hfe {
namespace geom {

namespace {

const char kHemiMapManagerConsoleTag[] = "geom.hemiMapManager";

BF2_CONSOLE_OBJECT("HemiMapManager", 2, 0)

class HemiMapManagerSetBaseBinding : public ConsoleObjectBaseImpl
{
public:
    HemiMapManagerSetBaseBinding()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "setBaseHemiMap";
        m_type = 0;
        m_minNoArgs = 3;
        m_maxNoArgs = 4;
        m_returnType = "bool";
        m_hasReturnValue = false;
        m_isDynamic = false;
        m_argTypes[0] = "std::string";
        m_argTypes[1] = "Vec3";
        m_argTypes[2] = "float";
        m_argTypes[3] = "float";
        for (int i = 0; i != 4; ++i)
            m_argNeedsConversion[i] =
                std::string(m_argTypes[i]) == "std::string" ? 0 : -1;
        consoleBindingList(kHemiMapManagerConsoleTag).push_back(this);
    }

    bool isObjectActive() const override
    {
        return g_HemiMapManager != 0;
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 3 && m_argCount != 4)
            return;

        const HemiMapName name(BF2_CC_ARG(std::string, 0).c_str());
        const Vec3 position = BF2_CC_ARG(Vec3, 1);
        const float range = BF2_CC_ARG(float, 2);
        const float falloff = m_argCount == 3 ? 500.0f : BF2_CC_ARG(float, 3);
        m_hasReturnValue = false;
        g_HemiMapManager->setBaseHemiMap(name, position, range, falloff);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        BF2_CC_CONVERT(std::string, 1)
        BF2_CC_CONVERT(Vec3, 2)
        BF2_CC_CONVERT(float, 3)
        BF2_CC_CONVERT(float, 4)
    }

    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};

HemiMapManagerSetBaseBinding s_hemiMapManagerSetBaseBinding;

// Exposes makeHemiMap(resolution, name, position, size) as a dynamic console command.
// Requires exactly four arguments and returns the manager's Boolean result.
class HemiMapManagerMakeHemiMapBinding : public ConsoleObjectBaseImpl
{
public:
    HemiMapManagerMakeHemiMapBinding()
    {
        m_objectName = consoleDefaultName();
        m_access = consoleDefaultAccess();
        m_category = consoleDefaultCategory();
        m_functionName = "makeHemiMap";
        m_type = 0;
        m_minNoArgs = 4;
        m_maxNoArgs = 4;
        m_returnType = "bool";
        m_hasReturnValue = false;
        m_isDynamic = true;
        m_argTypes[0] = "unsigned long";
        m_argTypes[1] = "std::string";
        m_argTypes[2] = "Vec3";
        m_argTypes[3] = "float";
        for (int i = 0; i != 4; ++i)
            m_argNeedsConversion[i] =
                std::string(m_argTypes[i]) == "std::string" ? 0 : -1;
        consoleBindingList(kHemiMapManagerConsoleTag).push_back(this);
    }

    bool isObjectActive() const override
    {
        return g_HemiMapManager != 0;
    }

    void executeObjectMethod() override
    {
        if (m_argCount != 4)
            return;

        const unsigned long resolution = BF2_CC_ARG(unsigned long, 0);
        const HemiMapName name(BF2_CC_ARG(std::string, 1));
        const Vec3 position = BF2_CC_ARG(Vec3, 2);
        const float orthoExtent = BF2_CC_ARG(float, 3);
        m_hasReturnValue = true;
        m_returnValue = g_HemiMapManager->makeHemiMap(resolution, name, position, orthoExtent);
    }

    void setArgFromString(int index, const std::string& value) override
    {
        BF2_CC_CONVERT(unsigned long, 1)
        BF2_CC_CONVERT(std::string, 2)
        BF2_CC_CONVERT(Vec3, 3)
        BF2_CC_CONVERT(float, 4)
    }

    BF2_CC_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS

private:
    bool m_returnValue;
};

HemiMapManagerMakeHemiMapBinding s_hemiMapManagerMakeHemiMapBinding;

} // anonymous namespace

bool registerHemiMapManagerConsoleObjects()
{
    return mainConsoleObjects->registerConsoleObjects(
        consoleBindingList(kHemiMapManagerConsoleTag));
}

bool unregisterHemiMapManagerConsoleObjects()
{
    return mainConsoleObjects->unregisterConsoleObjects(
        consoleBindingList(kHemiMapManagerConsoleTag));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
