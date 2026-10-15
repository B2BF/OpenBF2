// World-to-HUD projection. [INFERRED] Source split from Menu/BF2HudManager.cpp.
#include "BF2HudManager.h"
#if !DEDICATED
#include "Ref2System.h"
#include "../RendMisc/IRenderView.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Scene/Object/PlayerManager.h"
namespace dice { namespace hfe { namespace hud {
namespace {
// Keep the PE's unrounded x87 multiply/add chain up to its final float store.
// Mat4's existing affine inverse cannot invert a general projection matrix.
float affineComponent(float a, float ma, float b, float mb,
                      float c, float mc, float translation)
{
#if defined(_M_IX86)
    float result;
    __asm {
        fld a
        fmul ma
        fld b
        fmul mb
        faddp st(1), st(0)
        fld c
        fmul mc
        faddp st(1), st(0)
        fadd translation
        fstp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    return static_cast<float>(static_cast<double>(a) * ma + static_cast<double>(b) * mb
                              + static_cast<double>(c) * mc + translation);
#endif
}

Vec3 affineInOrder(const Mat4& matrix, const Vec3& point, int first, int second, int third)
{
    const float p[3] = {point.x, point.y, point.z};
    Vec3 result;
    result.z = affineComponent(p[first], matrix.m[first * 4 + 2],
        p[second], matrix.m[second * 4 + 2], p[third], matrix.m[third * 4 + 2], matrix.m[14]);
    result.y = affineComponent(p[first], matrix.m[first * 4 + 1],
        p[second], matrix.m[second * 4 + 1], p[third], matrix.m[third * 4 + 1], matrix.m[13]);
    result.x = affineComponent(p[first], matrix.m[first * 4],
        p[second], matrix.m[second * 4], p[third], matrix.m[third * 4], matrix.m[12]);
    return result;
}

float screenCoordinate(float numerator, float denominator)
{
#if defined(_M_IX86)
    float result;
    __asm {
        fld numerator
        fdiv denominator
        fstp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    return static_cast<float>(static_cast<double>(numerator) / denominator);
#endif
}

void projectToScreen(const Vec3& clip, float halfWidth, float height, Vec3& result)
{
    const float half = 0.5f;
    // The PE divides by transformed Z, not by a homogeneous W. Unordered,
    // zero and negative Z take the same hidden-point arm; X/Y stay zero.
    if (!(clip.z > 0.0f)) {
        result.z = -1.0f;
        return;
    }
    const float x = clip.x, y = clip.y, z = clip.z;
    float outputX, outputY, outputZ;
#if defined(_M_IX86)
    __asm {
        fld height
        fmul half
        fld1
        fdiv z
        fld x
        fmul st(0), st(1)
        fmul halfWidth
        fstp outputX
        fxch st(1)
        fchs
        fmul y
        fmul st(0), st(1)
        fstp outputY
        fstp outputZ
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    const double halfHeight = static_cast<double>(height) * half;
    const double inverse = 1.0 / z;
    outputX = static_cast<float>(x * inverse * halfWidth);
    outputY = static_cast<float>(-halfHeight * y * inverse);
    outputZ = static_cast<float>(inverse);
#endif
    result.x = outputX;
    result.y = outputY;
    result.z = outputZ;
}
}

Vec3 BF2HudManager::screenToWorld(Vec2 position)
{
    Vec3 result(Vec3::getZero());
    if (!ref2System) return result;
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (!local) return result;
    auto* ui = static_cast<dx9rend::IRenderView*>(ref2System->getRenderView());
    auto* scene = static_cast<dx9rend::IRenderView*>(ref2System->getPreviousRenderView());
    const float halfWidth = ui->getOrthoWidth() * 0.5f;
    const float halfHeight = ui->getOrthoHeight() * 0.5f;
    const float nearPlane = scene->getNearPlane();
    const Vec3 normalized = {screenCoordinate(position.x, halfWidth),
                            screenCoordinate(position.y, -halfHeight), nearPlane};
    Mat4 projection(*scene->getProjectionMatrixPtr());
    const Mat4 source(projection);
    fullInvert(source, projection);
    const Vec3 cameraPoint = affineInOrder(projection, normalized, 2, 1, 0);
    result = affineInOrder(local->getCamera()->getTransformation(0.0f), cameraPoint, 1, 2, 0);
    return result;
}

Vec3 BF2HudManager::worldToScreen(Vec3 position)
{
    Vec3 result(Vec3::getZero());
    if (!ref2System) return result;
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    auto* ui = static_cast<dx9rend::IRenderView*>(ref2System->getRenderView());
    auto* scene = static_cast<dx9rend::IRenderView*>(ref2System->getPreviousRenderView());
    // Unlike screenToWorld, the shipped body does not guard the local player.
    Mat4 camera(local->getCamera()->getTransformation(0.0f));
    const Mat4 source(camera);
    fullInvert(source, camera);
    const Vec3 cameraPoint = affineInOrder(camera, position, 0, 1, 2);
    const Vec3 clip = affineInOrder(*scene->getProjectionMatrixPtr(), cameraPoint, 1, 2, 0);
    const float halfWidth = ui->getOrthoWidth() * 0.5f;
    const float height = ui->getOrthoHeight();
    projectToScreen(clip, halfWidth, height, result);
    return result;
}
} } }
#endif
