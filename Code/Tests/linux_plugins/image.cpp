// [TEMP] [CHOSEN, not recovered] A host/plugin body compiled with distinct state.
#include "image.h"
namespace bf2PluginFixture { int imageState=FIXTURE_INITIAL; }
extern "C" bf2PluginFixture::Image* FIXTURE_ENTRY() {
    static bf2PluginFixture::Image instance;
    return &instance;
}
