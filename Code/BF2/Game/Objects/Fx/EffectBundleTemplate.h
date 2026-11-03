// Groups effect emitters and supplies EffectProperties as a secondary base.
// Client creation constructs an EffectBundle; dedicated creation returns null. Initialization adds a 3D looping Start sound.
// preCache explicitly calls ObjectTemplate, and child info uses the EffectInfo subclass.
// RandomDistribution members retain their embedded seed/layout even though sampling uses Rand.
// Original ELF size is 0x340; unresolved own-field roles remain marked below. Placement is [INFERRED].

#ifndef GAME_OBJECTS_FX_EFFECTBUNDLETEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_FX_EFFECTBUNDLETEMPLATE_H

#include <string>
#include <vector>
#include "../../../Core/Random.h"       // the two RandomDistributions at +0x2E0/+0x2F4
#include "EffectProperties.h"
#include "../Base/BundleTemplate.h"

namespace dice {
namespace hfe {
namespace world {

class EffectBundle;

class EffectBundleTemplate : public BundleTemplate, public EffectProperties
{
public:
    // The per-child info subclass createTemplateInfo substitutes for Bundle's TemplateInfo:
    // base ctor + its own vtable + two zeroed dwords (0x5C total vs TemplateInfo's 0x54).
    class EffectInfo : public TemplateInfo
    {
    public:
        explicit EffectInfo(const std::string& templateName);
        int m_field54;                             // +0x54 ctor 0 [UNRESOLVED]
        int m_field58;                             // +0x58 ctor 0 [UNRESOLVED]
    };

    EffectBundleTemplate();                                   // read in full

    virtual unsigned int getClassID() const;                  // reads the CID global
    virtual ~EffectBundleTemplate();                          // frees the two
                                                              // vectors, then EP + Bundle
    virtual void templateInitialize();                        // the "Start"
                                                              // sound template, 3D + loop 1
    virtual void* queryImplementation(unsigned int classId);  // CID hop -> Bundle's chain
    virtual IObject* createObject() const;                    // dedicated null; client PE 0x642680
    virtual void preCache() const;                            // flags-gated grandparent call
    virtual bool makeScript(io::IStream*);                    // ELF/PE recovered
    virtual bool makeMemberScript(const TemplateInfo*, io::IStream*);   // dedicated ELF81FC410 / PE5F9250; client frontier
    virtual TemplateInfo* createTemplateInfo(const std::string&);       // news an EffectInfo

    virtual void  setEmitFrequencyScaleFactorLowerBound(float);   // the +0x30C store
    virtual float getEmitFrequencyScaleFactorLowerBound() const;

    bool physicsUsed() const;
    void preCacheGeoms() const;                              // client PE 0x6417B0
    void addBundleInstances(EffectBundle*) const;             // client PE 0x642C70
    friend class EffectBundle; // start/stop compare child IDs with the stored template-info vector

    // fields (+0x2D8..+0x33F; ctor values in the header block)
    float           m_field2D8, m_field2DC;                   // the -1.0f twin pair
    // GenericProjectile::activate (ELF 0x8280700) samples this ONE to decide whether to build a
    // trail effect at all: a draw of exactly -1.0f suppresses the trail. The drawn value is used
    // for nothing else. [UNRESOLVED] what the distribution means when it is not -1.
    RandomDistribution m_dist2E0;                             // +0x2E0..+0x2F3
    RandomDistribution m_dist2F4;                             // +0x2F4..+0x307 [UNRESOLVED role]
    float           m_field308;                               // 500.0f
    float           m_emitFrequencyScaleFactorLowerBound;     // +0x30C = 0.05f
    float           m_field310;                               // -1.0f
    unsigned char   m_byte314;
    std::vector<unsigned int> m_vec318;                       // [UNRESOLVED element meaning]
    struct Elem64 { char _opaque[64]; };                      // [UNRESOLVED] the 64-B element
    std::vector<Elem64> m_vec324;
    int             m_field330, m_field334;
    unsigned char   m_byte338, m_byte339, m_byte33A;
    int             m_field33C;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif
