// [TEMP] [CHOSEN, not recovered] macOS port test tool: the scripted input engine
// (Platform/InputScript.h, docs/macos-c-input-script.md) under the names ClientHost uses.
// MacInputScript.mm holds its kVK_ key table. [RETIRE-WHEN: client-test-automation-replaced]
#ifndef PLATFORM_MACOS_MACINPUTSCRIPT_H
#define PLATFORM_MACOS_MACINPUTSCRIPT_H

#include "../InputScript.h"

namespace dice {
namespace hfe {
namespace macos {

using inputscript::IInputScriptSink;
using inputscript::InputScript;
using inputscript::ScriptQueryFn;
using inputscript::inputScript;
using inputscript::setScriptQuery;

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // PLATFORM_MACOS_MACINPUTSCRIPT_H
