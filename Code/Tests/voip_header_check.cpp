// Test scaffolding: compiles the recovered BF2VoipServer public headers for syntaxcheck.cmd.
// These headers are not yet reached by any translation unit, so without this include they would be
// written but never parsed. Fix declaration errors in the owning headers, not here.
#include "../BF2/Core/Timer.h"
#include "../BF2/VoipServer/VoipServerApi.h"
#include "../BF2/VoipServer/VoipProtocol.h"
#include "../BF2/VoipServer/VoipResult.h"
#include "../BF2/VoipServer/VoipClients.h"
#include "../BF2/VoipServer/VoipRouter.h"
#include "../BF2/VoipServer/VoipDebug.h"
#include "../BF2/VoipServer/VoipPackets.h"
#include "../BF2/VoipServer/VoipServerInstance.h"
