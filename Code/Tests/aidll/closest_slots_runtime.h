// [TEMP] Real headers and native chosen lifetimes; no retail object-layout claim.
#pragma once
#include "../../BF2/AI/AIDLL/AI/AIBot/SquadFormation.h"
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <crtdbg.h>
#include <type_traits>

static_assert(sizeof(void*) == 4, "x86 only");
static_assert(sizeof(dice::hfe::ai::SquadFormation::Slot) == 36, "native Slot extent");
static_assert(std::is_same<decltype(dice::hfe::ai::SquadFormation::Slot::m_i28), float>::value,
              "attested float payload");
