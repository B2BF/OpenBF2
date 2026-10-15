// [PLATFORM] Native-only bridge to the host ConsoleObjects registry. Registers before
// the module event handler and unregisters after handler removal. Reuses the host registry.

#include "PluginAbi.h"

#if defined(BF2_IMAGE_RENDDX9)

#include "../BF2/Core/ConsoleObjects.h"

#include <string>
#include <utility>

namespace dice {
namespace hfe {

namespace {

inline std::string registryKey(const ConsoleObject* object)
{
    return std::string(object->getObjectName()) + std::string(object->getFunctionName());
}

} // anonymous namespace

bool ConsoleObjects::registerConsoleObjects(ConsoleObjectList& objects)
{
    for (ConsoleObjectList::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        ConsoleObject* const object = *it;
        m_consoleObjectMap.insert(std::make_pair(registryKey(object), object));
        m_consoleObjects.insert(m_consoleObjects.end(), object);
    }
    return true;
}

bool ConsoleObjects::unregisterConsoleObjects(ConsoleObjectList& objects)
{
    for (ConsoleObjectList::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        ConsoleObject* const object = *it;
        m_consoleObjectMap.erase(registryKey(object));

        ConsoleObjectList::iterator found = m_consoleObjects.begin();
        while (found != m_consoleObjects.end() && *found != object)
            ++found;
        if (found != m_consoleObjects.end())
            m_consoleObjects.erase(found);
    }
    return true;
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
