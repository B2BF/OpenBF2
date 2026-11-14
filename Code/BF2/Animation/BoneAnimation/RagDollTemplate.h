// Shared particle/constraint definition and live ragdoll registry.
// [CHOSEN] split from RagDoll.cpp. Owns the reference skeleton and updates registered instances
// Once per frame; each RagDoll unregisters itself on destruction.
// The singleton name and private field names are [INFERRED]. Constructor-derived offsets
// Beyond +0x6C are [INFERRED] and must not be copied across STL implementations.

#ifndef ANIMATION_BONEANIMATION_RAGDOLLTEMPLATE_H     // [INFERRED] guard style
#define ANIMATION_BONEANIMATION_RAGDOLLTEMPLATE_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../../Math/Vec3.h"     // dice::hfe::Vec3 (typedef of Math::Vec3)
#include "../../Math/Mat4.h"     // dice::hfe::Mat4 (makeInstance signature)
#include "RagDoll.h"             // RagDoll::Particle - the template stores the prototypes

namespace dice {
namespace anim {

class Skeleton;          // Animation/BoneAnimation/Skeleton.cpp

class RagDollTemplate
{
public:
    // The element types (attested type names, [INFERRED] member names)
    struct BoneMapping                       // toSkeleton(bone, partA, partB)
    {
        int m_bone;
        int m_partA;                         // negative = "use the skeleton bone's own position"
        int m_partB;
    };
    struct Constraint                        // distance / capsule / joint pairs
    {
        int   m_a;                           // particle indices, not bone ids
        int   m_b;
        float m_length;                      // the rest distance (0 for capsule/joint pairs)
    };
    struct AngleConstraint
    {
        int   m_a, m_b, m_c;
        float m_distAB, m_distBC;
        float m_distAC;                      // a-c distance for the angle; negative = a maximum
    };
    struct DihedralAngleConstraint
    {
        int   m_a, m_b, m_c, m_d;
        float m_distAB, m_distBC, m_distCD;
        float m_targetAD;                    // the (d-c)+(b-a) length the angle implies
    };
    struct DistanceLessThanConstraint        // keeps a pair at least this far apart
    {
        int   m_a, m_b;
        float m_minDistance;
    };

    // lifecycle
    static void init();
    static void shutdown();
    static void autoInit();                  // empty

    RagDollTemplate();
    // Requires all instances to be removed; aborts otherwise.
    ~RagDollTemplate();
    // Creates and registers a ragdoll using clamped inherited velocity; caches it as the last instance.
    RagDoll* makeInstance(const hfe::Mat4& transform, const hfe::Vec3& velocity, Skeleton& skeleton);
    void removeInstance(RagDoll* ragDoll);
    void updateInstances(bool sortFromViewer, const hfe::Vec3& viewerPos, const hfe::Vec3& viewerDir,
                         float dt);          // the mainLoop per-frame step

    // Console-driven configuration (bound as the "RagDoll" console object)
    // Return types are [CHOSEN] void where the ELF leaves only residue in eax.
    void  addForce(int bone, const hfe::Vec3& force);
    float getBoneDistance(int boneA, int boneB);
    void  lockBone(int fromBone, int toBone);
    void  addParticle(int bone, float mass);
    std::list<Constraint>::iterator findConstraint(int particleA, int particleB);
    int   findParticleIndex(int bone);
    int   findBoneIndex(int particleIndex);
    void  addAngularConstraint(int a, int b, int c, float angleDeg);
    void  addDihedralAngleConstraint(int a, int b, int c, int d, float angleDeg);
    void  addDistanceLessThanConstraint(int a, int b, float minDistance);
    void  toSkeleton(int bone, int partA, int partB);
    void  addDistanceConstraint(int a, int b);
    void  addCapsuleCollision(int a, int b);
    void  addJointConstraint(int a, int b);
    void  removeConstraint(int a, int b);
    // Writes the distance constraints to ragDollConstraints.inc as ragDoll.addConstraint lines
    // (the console `ragDoll.save` tool, ELF 0x8341110).
    void  writeConstraints();
    void  skeleton(const std::string& name);
    void  setParticleCollisionCriteria(float minImpact, float debounce);

    // layout. Public because RagDoll's bodies read these directly (no accessor in the ELF);
    // visibility is [CHOSEN].
    /* +0x00 */ bool  m_drawConstraints;                 // console `drawConstraints`
    /* +0x01 */ bool  m_active;                          // RagDoll::reset clears it. [UNRESOLVED] reader
    /* +0x02 */ bool  m_resetRequested;                  // console cc96. [UNRESOLVED] reader
    /* +0x04 */ float m_gravity;                         // ctor -30.0
    /* +0x08 */ std::vector<BoneMapping>       m_boneMappings;
    /* +0x14 */ std::vector<RagDoll::Particle> m_particles;      // the prototypes reset() copies
    /* +0x20 */ std::vector<std::pair<int, int> > m_bones;       // [UNRESOLVED] use - ctor/dtor only
    /* +0x2C */ std::vector<bool> m_lockedBones;         // 128, lockBone sets, applyOnSkeleton reads
    /* +0x40 */ std::vector<int>  m_linkModes;           // 128 zeroes. [UNRESOLVED] use
    /* +0x4C */ std::vector<int>  m_vector4C;            // empty. [UNRESOLVED] use
    /* +0x58 */ std::list<Constraint> m_distanceConstraints;
    /* +0x5C */ std::list<Constraint> m_capsuleConstraints;      // IDA "constraints"
    /* +0x60 */ std::list<Constraint> m_jointConstraints;        // [UNRESOLVED] solver - never read
    /* +0x64 */ std::list<AngleConstraint> m_angularConstraints;
    /* +0x68 */ std::list<DihedralAngleConstraint> m_dihedralAngleConstraints;
    /* +0x6C */ std::list<DistanceLessThanConstraint> m_distanceLessThanConstraints;
    /* +0x70 */ Skeleton* m_skeleton;                    // owned - the dtor deletes it
    /* +0x74 */ RagDoll*  m_lastInstance;                // addForce's target
    /* +0x78 */ float m_particleCollisionCriteria1;      // -1 = the collision report is off
    /* +0x7C */ float m_particleCollisionCriteria2;      // seconds between reports

private:
    // +0x84/+0x88 - SGI _Rb_tree<RagDoll*, pair<RagDoll* const, bool>>; std::map is the ordinary
    // C++ that emits exactly that. Key order = pointer order, which is what makes the no-viewer
    // update order reproducible. The bool payload is always true. [INFERRED name]
    std::map<RagDoll*, bool> myInstances;

public:
    /* +0x90 */ int m_particleIndexes[128];              // bone id -> particle index, -1 = none
};

// [INFERRED] Global template name.
extern RagDollTemplate* g_ragDollTemplate;

} // namespace anim
} // namespace dice

#endif // ANIMATION_BONEANIMATION_RAGDOLLTEMPLATE_H
