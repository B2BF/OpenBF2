// [TEMP] Untouched production types for the exact local predicate fixture.
#pragma once
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/AIInformationGrid.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/Information.h"
#include "../../BF2/AI/AIDLL/AI/Main/InformationManager.h"
#include "../../BF2/AI/AIDLL/AI/AIKnowledge/IPIUnit.h"
#include "../../BF2/AI/AIDLL/AI/Main/AIDllFrontier.h"
#include <cstddef>
namespace dice { namespace hfe { namespace ai {
// [TEMP] Typed access to the actual namespace-local object, no layout casts.
IAIInformationGridPredicate* makeFixturePredicate(int side,int*& sideSlot,float*& strengthSlot);
void destroyFixturePredicate(IAIInformationGridPredicate* predicate);
} } }
