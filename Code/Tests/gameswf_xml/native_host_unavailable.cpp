// [TEMP] [CHOSEN, not recovered] CPU gameswf test link scaffolding. SwiffPlayer's
// archive also carries game-owned UI natives; this fixture must never use them.
// [RETIRE-WHEN: swf-xml-link-isolation] Remove when the XML and popup fixtures no longer
// pulls those natives from the production archive, and verify its strict link.
#include <cstdio>
#include <cstdlib>

#include "../../BF2/Platform/Host/ClientHost.h"

namespace dice {
namespace hfe {
namespace host {

namespace {
[[noreturn]] void unavailable(const char* method)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::host::ClientHost::%s reached in CPU gameswf fixture\n", method);
    std::fflush(stderr);
    std::abort();
}
}

ClientHost& ClientHost::instance()
{
    unavailable("instance");
}

void ClientHost::setCursor(void*)
{
    unavailable("setCursor");
}

unsigned int ClientHost::modifierFlags() const
{
    unavailable("modifierFlags");
}

unsigned short ClientHost::consumeAsyncEnterState()
{
    unavailable("consumeAsyncEnterState");
}

} // namespace host
} // namespace hfe
} // namespace dice
