// Strategic-map rendering hooks. The parameter names are attested by the ELF symbols;
// [CHOSEN, not recovered] a static utility class spells the otherwise ambiguous scope.
#ifndef AI_AIDLL_AI_AISTRATEGICMAP_SAIMAPRENDERUTIL_H
#define AI_AIDLL_AI_AISTRATEGICMAP_SAIMAPRENDERUTIL_H

namespace dice {
namespace hfe {
namespace ai {

class SAIMap;
class SAIMapRendInfo;
// [CHOSEN, not recovered] int permits an opaque enum declaration. Neither image reads
// this argument or attests enumerators or signedness; no values are introduced here.
enum SAIMapRenderType : int;

class SAIMapRenderUtil
{
public:
    // The shipped AI image provides no rendering information and always returns null.
    static SAIMapRendInfo* createRenderInfo(SAIMap* map, SAIMapRenderType type);
    // These shipped hooks leave the supplied opaque information untouched.
    static void update(SAIMapRendInfo* info);
    static void render(SAIMapRendInfo* info);
    static void releaseRenderInfo(SAIMapRendInfo* info);
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICMAP_SAIMAPRENDERUTIL_H
