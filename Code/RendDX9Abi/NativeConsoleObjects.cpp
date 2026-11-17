// [PLATFORM] Native-only bridge to the host ConsoleObjects registry. Registers before
// the module event handler and unregisters after handler removal. Reuses the host registry.
// Also the RendDX9 image's own copy of the console class defaults (SetConsoleClassDefaults).

#include "PluginAbi.h"

#if defined(BF2_IMAGE_RENDDX9)

#include "../BF2/Core/ConsoleObjectBase.h"
#include "../BF2/Core/ConsoleObjectMacro.h"

#include <cctype>

namespace dice {
namespace hfe {

// The defaults the image's binding constructors copy (PE 0x10247C20 / 0x10247C24 / 0x10247C28 /
// 0x10247C2C; the Linux ELF names its four ConsoleObject::mDefaultStaticObjectName,
// mDefaultDynamicObjectName, mDefaultAccess and mDefaultCategory). [CHOSEN, not recovered] free
// accessors over file statics, the shape of the shared Core copy. On the macOS static client these
// names are image-private (Code/StaticPlugins/image-private-symbols.txt), so the renderer's
// bindings read this copy and the host's read Core/ConsoleObjectBase.cpp's, as with two DLLs.
// Retail's start zeroed (.bss); [CHOSEN, not recovered] the names start empty, as in the Core copy,
// so a binding that this tree's TU order constructs before any defaults gets no null name.
namespace {
const char*   g_defaultStaticName = "";
const char*   g_defaultNameLower  = "";
unsigned int  g_defaultAccess     = 0;
unsigned char g_defaultCategory   = 0;
}

const char*   consoleDefaultStaticName() { return g_defaultStaticName; }
const char*   consoleDefaultName()       { return g_defaultNameLower; }
unsigned int  consoleDefaultAccess()     { return g_defaultAccess; }
unsigned char consoleDefaultCategory()   { return g_defaultCategory; }

// PE 0x1007BF60 (RendDX9's copy; [INFERRED] placement: Core/ConsoleObjectBase.cpp in the image).
// Copies the name unbounded into a 32-byte operator new[] block (0x1005F340), lowers its first
// character with the CRT's tolower, then stores the caller's pointer, the copy, access and
// category. BF2.exe 0x004C44A0 and the Linux ELF 0x083BE5C0 (strcpy, tolower) do the same; the
// shared Core copy keeps its own behaviour for the other images. The copy is never freed.
SetConsoleClassDefaults::SetConsoleClassDefaults(const char* objectName, unsigned int access,
                                                 unsigned char category)
{
    char* const lowered = new char[0x20];
    const char* source = objectName;
    char* target = lowered;
    char c;
    do
    {
        c = *source++;
        *target++ = c;
    }
    while (c != '\0');
    lowered[0] = static_cast<char>(std::tolower(lowered[0]));

    g_defaultStaticName = objectName;
    g_defaultNameLower  = lowered;
    g_defaultAccess     = access;
    g_defaultCategory   = category;
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9

// [CHOSEN, not recovered] The native static image calls the host's identical
// ConsoleObjects methods directly; the per-DLL bridge remains unchanged on Windows.
#if defined(BF2_IMAGE_RENDDX9) && !defined(BF2_STATIC_PLUGINS)

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
