// [INFERRED] split in the attested BF2HudManager.cpp family. Complete PE755870
// and every local helper read.
#include "BF2HudManager.h"
#if !DEDICATED
#include "Ref2System.h"
#include "../Core/ClassManager.h"
#include "../Core/SmartItf.h"
#include "../Core/Ids.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/Utility/Vars.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/ObjectTemplate.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/ObjectPredicator.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Physics/HeightmapCluster.h"
#include "../RendMisc/IRenderView.h"
#include "../Math/Frustum.h"
#include <cmath>
#include <vector>

namespace dice { namespace hfe { namespace hud {
namespace {
// [INFERRED] local names. Full four-slot tables925CA8/925CB8. These are not
// ObjectFlagPredicator's layout: root precedes the mask, at native +4/+8.
class SpottedRayPredicator : public world::ObjectPredicator {
public:
    explicit SpottedRayPredicator(world::IObject* root)
        : m_root(root), m_flags(0x100), m_ignoreSoldiers(false) {}
    bool predicate(world::IObject* object) override {
        if (world::getRootParent(object) == m_root) return false;
        if (m_ignoreSoldiers && object && object->getTemplate()
            && object->getTemplate()->getClassID() == world::CID_SoldierTemplate)
            return false;
        return (object->getFlags() & m_flags) == m_flags;
    }
    unsigned int getFlags() const override { return m_flags; }
    bool performCulling(world::IObject*) override { return true; }
    bool predicateSurface(int) override { return true; }
private:
    world::IObject* m_root;
    unsigned int m_flags;
    bool m_ignoreSoldiers;
};
class SpottedPcoPredicator : public SpottedRayPredicator {
public:
    explicit SpottedPcoPredicator(world::IObject* root) : SpottedRayPredicator(root) {}
    bool predicate(world::IObject* object) override {
        if (!SpottedRayPredicator::predicate(object)) return false;
        return object->queryInterface(world::IID_IPlayerControlObject)
            && world::getRootParent(object) == object;
    }
};
#if defined(_M_IX86)
static_assert(sizeof(SpottedRayPredicator)==16 && sizeof(SpottedPcoPredicator)==16,
              "native client spotted-query predicate storage");
#endif

float multiplyAdd(float value, float factor, float translation)
{
#if defined(_M_IX86)
    float result;
    __asm {
        fld value
        fmul factor
        fadd translation
        fstp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    return static_cast<float>(static_cast<double>(value) * factor + translation);
#endif
}

// PE755A60/755B1E retain all differences through sqrt; helper464660 instead
// stores the X difference first. Both sums are Z^2 + X^2 + Y^2.
float targetDistance(const Vec3& from, const Vec3& to, float scale, bool storeX)
{
    const float fromX=from.x, fromY=from.y, fromZ=from.z;
    const float toX=to.x, toY=to.y, toZ=to.z;
    float x, result;
#if defined(_M_IX86)
    if (storeX) {
        __asm {
            fld toX
            fsub fromX
            fstp x
        }
        __asm {
            fld toZ
            fsub fromZ
            fmul st(0), st(0)
            fld x
            fmul st(0), st(0)
            faddp st(1), st(0)
            fld toY
            fsub fromY
            fmul st(0), st(0)
            faddp st(1), st(0)
            fsqrt
            fmul scale
            fstp result
        }
    } else {
        __asm {
            fld toZ
            fsub fromZ
            fmul st(0), st(0)
            fld toX
            fsub fromX
            fmul st(0), st(0)
            faddp st(1), st(0)
            fld toY
            fsub fromY
            fmul st(0), st(0)
            faddp st(1), st(0)
            fsqrt
            fmul scale
            fstp result
        }
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // Sum order Z^2 + X^2 + Y^2; only the storeX arm rounds the X difference to float first.
    const double dz = static_cast<double>(toZ) - fromZ;
    double dx = static_cast<double>(toX) - fromX;
    if (storeX) {
        x = static_cast<float>(dx);
        dx = x;
    }
    const double dy = static_cast<double>(toY) - fromY;
    result = static_cast<float>(std::sqrt(dz * dz + dx * dx + dy * dy) * scale);
#endif
    return result;
}

// PE755D11..755DCA biases from the captured shared zero, not the camera.
// The distance comparison uses retained x87 precision; the divisor and Y/Z
// differences are stored floats, X's difference is retained until its product.
void pullHitTowardOrigin(const Vec3& hit, const Vec3& origin, Vec3& output)
{
    const float hx=hit.x, hy=hit.y, hz=hit.z;
    const float ox=origin.x, oy=origin.y, oz=origin.z;
    const float bias=0.2f;
    float dy, dz, magnitude, scale, offsetX;
#if defined(_M_IX86)
    float* result=&output.x;
    __asm {
        fld hz
        fsub oz
        fst dz
        fmul st(0), st(0)
        fld hy
        fsub oy
        fst dy
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld hx
        fsub ox
        fmul st(0), st(0)
        faddp st(1), st(0)
        fsqrt
        fst magnitude
        fcomp bias
        fnstsw ax
        test ah, 41h
        jnz pullDone
        fld bias
        fdiv magnitude
        fstp scale
        fld hx
        fsub ox
        fmul scale
        fstp offsetX
        fld hx
        fsub offsetX
        mov edx, result
        fstp dword ptr [edx]
        fld dy
        fmul scale
        fsubr hy
        fstp dword ptr [edx+4]
        fld dz
        fmul scale
        fsubr hz
        fstp dword ptr [edx+8]
    pullDone:
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // Z and Y differences are stored as floats; X stays unrounded until its product. Only an
    // ordered root above the bias writes the output (x, y, z = +0/+4/+8).
    const double diffZ = static_cast<double>(hz) - oz;
    dz = static_cast<float>(diffZ);
    const double diffY = static_cast<double>(hy) - oy;
    dy = static_cast<float>(diffY);
    const double diffX = static_cast<double>(hx) - ox;
    const double root = std::sqrt(diffZ * diffZ + diffY * diffY + diffX * diffX);
    magnitude = static_cast<float>(root);
    if (root > bias) {
        scale = static_cast<float>(bias / static_cast<double>(magnitude));
        offsetX = static_cast<float>(diffX * scale);
        output.x = static_cast<float>(static_cast<double>(hx) - offsetX);
        output.y = static_cast<float>(hy - static_cast<double>(dy) * scale);
        output.z = static_cast<float>(hz - static_cast<double>(dz) * scale);
    }
#endif
}
}

bool BF2HudManager::findSpottedTarget(world::IObject*& object, Vec3& position,
                                    float distance, bool vehiclePosition)
{
    position={-4000.0f,-4000.0f,-4000.0f};
    object=nullptr;
    const Vec3 origin(Vec3::getZero());
    Vec3 eye(Vec3::getZero());
    Vec3 endpoint(Vec3::getZero());
    (void)Vec3::getZero();
    Vec3 direction(Vec3::getZero());
    Vec3 hit(Vec3::getZero());
    Vec3 normal(Vec3::getZero());
    unsigned char material=0;
    int part=0;
    float range=*Vars::getFloat("coll-name-dist",distance);
    world::IPlayer* local=nullptr;
    world::IObject* vehicle=nullptr;
    if (world::playerManager) {
        local=world::playerManager->getLocalHumanPlayer();
        if (local) {
            vehicle=world::getRootParent(local->getVehicle());
            if (vehicle) {
                (void)vehicle->getAbsoluteTransformation();
                const Mat4& camera=local->getCamera()->getTransformation(0.0f);
                endpoint={camera.m[8],camera.m[9],camera.m[10]};
                eye={camera.m[12],camera.m[13],camera.m[14]};
            }
        }
    }
    auto* heightmap=static_cast<world::IHeightmapCluster*>(
        classManager->getSingleton(std::string("HeightmapCluster")));
    direction=endpoint;
    endpoint.z=endpoint.z*range; // PE stores Z's product before adding origin.
    endpoint.x=multiplyAdd(endpoint.x,range,eye.x);
    endpoint.y=multiplyAdd(endpoint.y,range,eye.y);
    endpoint.z=endpoint.z+eye.z;
    float terrainFraction=0.0f;
    if (heightmap->intersectRayInWorldCoords(eye,endpoint,hit,terrainFraction,
                                           normal,material,true)
        && !(-terrainFraction > distance)) // PE also enters on unordered.
        range=targetDistance(eye,hit,1.3f,false);
    else
        range=distance;
    SpottedRayPredicator ray(world::getRootParent(vehicle));
    direction.x*=range; direction.y*=range; direction.z*=range;
    world::objectManager->intersectLine(object,hit,normal,part,eye,direction,
                                       &ray,true,false,false,true,0);
    if (object) range=targetDistance(eye,hit,1.0f,false);

    std::vector<world::IObject*> candidates;
    auto* previous=static_cast<dx9rend::IRenderView*>(ref2System->getPreviousRenderView());
    if (previous) {
        Frustum frustum(*previous->getFrustumPtr()); // native copy constructor752D90
        frustum.setFarDist(range);
        SpottedPcoPredicator pred(world::getRootParent(vehicle));
        world::objectManager->getObjectsFromGridNoCullConst(frustum,candidates,&pred,false,0.0f,0.0f);
    }
    for (world::IObject* candidate : candidates) {
        if (!candidate) continue;
        const Vec3 candidatePosition(candidate->getAbsolutePosition());
        if (local) {
            world::IObject* localRoot=world::getRootParent(local->getVehicle());
            if (world::getRootParent(candidate)!=localRoot
                && static_cast<HudInformationLayer*>(g_hudInformationLayer)->isWithinRadius(eye,candidatePosition,range)
                && isWithinAimThreshold(candidate,m_field1f4)) {
                range=targetDistance(eye,candidatePosition,1.0f,true);
                object=candidate;
            }
        }
    }
    world::IObject* root=world::getRootParent(object);
    bool found=false;
    if ((!(std::fabs(hit.x)<=1.1920929e-7f) || !(std::fabs(hit.y)<=1.1920929e-7f)
         || !(std::fabs(hit.z)<=1.1920929e-7f)) && range>1.0f && range<distance) {
        m_field264=hit.x; m_field268=hit.y; m_field26c=hit.z;
        position=hit;
        pullHitTowardOrigin(hit,origin,position);
        found=true;
    }
    if (root && root->getNetworkable() && vehiclePosition) {
        object=root;
        position=root->getAbsolutePosition();
        found=true;
    }
    return found;
}
} } }
#endif
