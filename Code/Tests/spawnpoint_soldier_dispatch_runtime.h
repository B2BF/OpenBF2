// [TEMP] Real declarations for the focused spawn occupancy regression.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include "../BF2/Core/Ids.h"
#include "../BF2/Game/IGameLogic.h"
#include "../BF2/Game/Objects/SpawnPoint.h"
#include "../BF2/Game/Objects/Game/Soldier.h"
#include "../BF2/Game/Objects/Game/ControlPoint.h"
#include "../BF2/Game/Objects/Game/EntryPoint.h"
#include "../BF2/Game/Objects/IPlayer.h"
#include "../BF2/Game/Objects/IObjectTemplateComponent.h"
#include "../BF2/Game/Objects/ObjectLayoutAccess.h"
#include "../BF2/Game/Objects/GameLogicComp/IArmorGLComp.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/ObjectPredicator.h"
#include "../BF2/Physics/HeightmapCluster.h"

[[noreturn]] void spawnUnexpected(const char* method);
