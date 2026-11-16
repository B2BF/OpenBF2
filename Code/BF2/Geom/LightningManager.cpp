// LightningManager, recovered from RendDX9.dll 1.50 (0x10155CB0..0x101575F0 and its factory).
// [INFERRED] file placement: the image carries no __FILE__ string or symbol for the class.
// A strike is a camera-facing textured strip (Shaders/Lightning) of 101 vertex pairs whose x is
// jittered per row; the visible part is its lowest lightningSegments rows.

#ifdef BF2_IMAGE_RENDDX9

#include "../Platform/X87Compat.h" // [CHOSEN, not recovered] x87 conversion results, including invalid inputs.
#include "../RendDX9/RendDX9Math.h"   // rendmath::normalize (sub_100777A0)
#include "LightningManager.h"
#include "SkyDome.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../Physics/HeightmapCluster.h"
#include "../Sound/BfSound.h"
#include "../Sound/SoundObjectTemplate.h"
#include "../Math/Mat4.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"

#include <cmath>
#include <cstdlib>
#include <new>
#include <string>

namespace dice {
namespace hfe {

// PluginEntry.cpp - the cached host ObjectTemplateManager (0x10244ADC).
extern IBase* objectTemplateManager;

namespace dx9rend {
extern void* g_shaderManager;      // dword_1023B97C
// BufferManager.cpp / StateManager.cpp seams, declared as OverlayTexture.cpp and SkyDome.cpp do.
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);   // sub_10011FF0
void  vertexBufferRelease(void* wrapper);                           // inline --ref, sub_10014510, free
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);                         // sub_10014630
bool  vertexBufferUnlock(void* wrapper);                            // sub_10014690
void  vertexBufferBindStream(void* wrapper, unsigned int stream);   // sub_100146C0
void  vertexDeclarationBind(void* wrapper);                         // sub_100147C0
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);              // sub_10005870
} // namespace dx9rend

namespace geom {

// StaticMesh.cpp - the shared PE 0x10077A30 product (output = left * right).
void geometryRendererMatrixProduct(Mat4& output, const Mat4& left, const Mat4& right);

namespace {

const unsigned int kIID_IBase             = 1u;
const unsigned int kIID_ILightningManager = 0xB801DD06u;   // dword_1021AEA4
const unsigned int kCID_SoundObjectTemplate = 9986001u;    // 0x985FD1, the "Sound" template class

const unsigned int kBoltVertexCount = 0xCA;                 // 101 rows of two vertices
const unsigned int kBoltRowCount    = 100;

// flt_10239DF8, .data, 1000. Its only reader is drawStrike. [INFERRED] name.
float g_lightningFarPlane = 1000.0f;

// The bolt vertex, 24 bytes: the stride createVertexBuffer is asked for.
struct LightningVertex
{
    float        x, y, z;        // +0x00
    float        u, v;           // +0x0C
    unsigned int color;          // +0x14  the fade, replicated into all four bytes
};

// unk_1021E2F4: POSITION float3 at 0, TEXCOORD0 float2 at 12, COLOR D3DCOLOR at 20.
const D3DVERTEXELEMENT9 kLightningVertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3,   0, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT2,   0, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 20, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR,    0 },
    { 0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0 }
};

// x87 truncating conversions (fistp under a chop control word, and _ftol2).
inline int truncateToInt(float value)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return bf2x87::toIntTruncate(value);
#else
    return static_cast<int>(value);
#endif
}

inline long long truncateToInt64(float value)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    return bf2x87::toInt64Truncate(value);
#else
    return static_cast<long long>(value);
#endif
}

// Mat4 copy (0x10030DD0) followed by the product into the original: `m = m * rhs`.
void multiplyInPlace(Mat4& m, const Mat4& rhs)
{
    const Mat4 left = m;
    geometryRendererMatrixProduct(m, left, rhs);
}

world::IHeightmapCluster* heightmapCluster()
{
    IBase* const found = classManager->getSingleton(renddx9abi::HostString("HeightmapCluster"));
    return static_cast<world::IHeightmapCluster*>(static_cast<void*>(found));
}

inline SkyDome* skyDome()
{
    return static_cast<SkyDome*>(g_SkyDome);
}

} // anonymous namespace


// Construction - 0x10156A80. The resources are created at once; a failure leaves the rest null.
// PE 0x10156A80.
LightningManager::LightningManager()
    : m_refCount(1), m_effect(0), m_vertexDeclaration(0), m_paramWorldViewProj(0),
      m_paramColor(0), m_paramTexture(0), m_texture(0), m_duration(0.15f), m_sfxDelay(5.0f),
      m_strikes(), m_sfxName(), m_sfxTemplate(0), m_pertubation(10.0f), m_blendOutTime(0.1f),
      m_skyBlendOutTime(0.1f), m_segments(100), m_groundPenetration(0.0f),
      m_inFadeSegments(0.0f), m_outFadeSegments(0.0f), m_pending(), m_time(0.0),
      m_lastTime(0.0), m_nextBuffer(0)
{
    m_scale.x = 1.0f;
    m_scale.y = 300.0f;
    m_color.x = 1.0f;
    m_color.y = 1.0f;
    m_color.z = 1.0f;
    m_color.w = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        m_vertexBuffers[i] = 0;
        m_sfxSounds[i] = 0;
    }
    // [CHOSEN, not recovered] The shipped constructor leaves +0x44 unwritten; initResources sets it
    // once the buffers exist. Zeroing it keeps a failed init deterministic natively.
    m_sound = 0;
    initResources();
}

// 0x10156B80. The members (pending queue, thunder name, strikes) follow.
LightningManager::~LightningManager()
{
    releaseResources();
}

void LightningManager::operator delete(void* memory)
{
    ::renddx9abi::engineFree(memory);
}


// The IBase quartet.

void LightningManager::addRef()
{
    ++m_refCount;
}

int LightningManager::getRef()
{
    return m_refCount;
}

// sub_1015A4B0 (shared): at zero, the deleting destructor (slot +0x10, flag 1).
int LightningManager::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// IBase (1) and IID_ILightningManager answer `this`.
// PE 0x10157530.
void* LightningManager::queryInterface(unsigned int iid) const
{
    if (iid == kIID_IBase)
        return const_cast<LightningManager*>(this);
    return iid != kIID_ILightningManager ? 0 : const_cast<LightningManager*>(this);
}


// 0x101561F0. Stops at the first missing resource. The Sound singleton and the bolt texture are
// fetched only after all three buffers exist.
bool LightningManager::initResources()
{
    m_effect = static_cast<dx9rend::Effect*>(
        dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, "Shaders/Lightning"));
    if (!m_effect)
        return false;
    dx9rend::effectSetTechniqueByName(m_effect, std::string("Lightning"));   // sub_1001DC60
    lookupShaderParameters();

    m_vertexDeclaration = dx9rend::vertexDeclarationCreate(kLightningVertexElements);
    if (!m_vertexDeclaration)
        return false;
    for (int i = 0; i < 3; ++i)
    {
        m_vertexBuffers[i] = dx9rend::bufferManagerCreateVertexBuffer(
            kBoltVertexCount, sizeof(LightningVertex), D3DUSAGE_WRITEONLY, "LightningManager");
        if (!m_vertexBuffers[i])
            return false;
    }

    m_sound = static_cast<IBfSound*>(static_cast<void*>(
        classManager->getSingleton(renddx9abi::HostString("Sound"))));
    m_texture = static_cast<dx9rend::EngineTexture*>(
        dx9rend::textureManagerGetTexture(std::string("common/textures/lightning")));
    return true;
}

// 0x10155CB0, in this order.
void LightningManager::lookupShaderParameters()
{
    m_paramWorldViewProj = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("WORLDVIEWPROJ"));
    m_paramColor = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("LIGHTNINGCOLOR"));
    m_paramTexture = dx9rend::effectGetShaderParameterTyped(m_effect, std::string("TEXTURE"));
}

// PE 0x10155D30: releases the three shader parameters (TEXTURE, LIGHTNINGCOLOR, WORLDVIEWPROJ, in that
// order), each only when set, nulling the slot after its release. The same sequence runs inline in
// releaseResources. Unreachable in 1.50: no code or data references.
void LightningManager::releaseShaderParameters()
{
    if (m_paramTexture)
        m_paramTexture->release();
    m_paramTexture = 0;
    if (m_paramColor)
        m_paramColor->release();
    m_paramColor = 0;
    if (m_paramWorldViewProj)
        m_paramWorldViewProj->release();
    m_paramWorldViewProj = 0;
}

// PE 0x10156310: drops the bolt texture reference (the last one destroys and frees it) and nulls the
// slot. The same sequence runs inline in releaseResources. Unreachable in 1.50: no code or data references.
void LightningManager::releaseTexture()
{
    if (m_texture)
        dx9rend::textureReleaseRef(m_texture);
    m_texture = 0;
}

// 0x10156340. The thunder first, then the texture, the three parameters in reverse, the buffers,
// the declaration and the effect; every slot is nulled.
void LightningManager::releaseResources()
{
    releaseSoundEffect();
    if (m_texture)
        dx9rend::textureReleaseRef(m_texture);
    m_texture = 0;
    if (m_paramTexture)
        m_paramTexture->release();
    m_paramTexture = 0;
    if (m_paramColor)
        m_paramColor->release();
    m_paramColor = 0;
    if (m_paramWorldViewProj)
        m_paramWorldViewProj->release();
    m_paramWorldViewProj = 0;
    for (int i = 0; i < 3; ++i)
    {
        if (m_vertexBuffers[i])
            dx9rend::vertexBufferRelease(m_vertexBuffers[i]);
        m_vertexBuffers[i] = 0;
    }
    if (m_vertexDeclaration)
        dx9rend::vertexDeclarationRelease(m_vertexDeclaration);
    m_vertexDeclaration = 0;
    if (m_effect)
        dx9rend::effectRelease(m_effect);
    m_effect = 0;
}

// 0x10156060. The three sound instances (ISound +0x04), the template (+0x08), then the name.
void LightningManager::releaseSoundEffect()
{
    for (int i = 0; i < 3; ++i)
    {
        if (m_sfxSounds[i])
        {
            m_sfxSounds[i]->release();
            m_sfxSounds[i] = 0;
        }
    }
    if (m_sfxTemplate)
    {
        m_sfxTemplate->release();
        m_sfxTemplate = 0;
    }
    m_sfxName.clear();
}


// 0x10156BF0. Unlike update(), this body tests the sky dome for null.
void LightningManager::reset()
{
    if (g_SkyDome)
        skyDome()->setLightingBlend(0.0f);
    // VC7.1's clear() frees the storage (the listing's free plus three zeroed pointers).
    m_pending.clear();
    m_strikes.clear();
    m_nextBuffer = 0;
}

// 0x10156DB0. The sky dome is used without a null test, as shipped.
void LightningManager::update(double deltaTime)
{
    m_time = deltaTime + m_time;
    processPendingLightning();
    const double frameTime = m_time - m_lastTime;
    m_lastTime = m_time;

    // Fade the sky flash by frameTime / skyBlendOutTime (at most 1), never below zero.
    const double blend = skyDome()->getLightingBlend();
    const double step = frameTime / m_skyBlendOutTime;
    const double clamped = (1.0 < step) ? 1.0 : step;
    const float faded = (blend - clamped < 0.0) ? 0.0f : static_cast<float>(blend - clamped);
    skyDome()->setLightingBlend(faded);

    for (unsigned int i = 0; i < m_strikes.size(); ++i)
    {
        LightningStrike& strike = m_strikes[i];
        if (m_time <= strike.flashEndTime)
        {
            if (!strike.built)
            {
                buildBoltBuffer(strike.buffer);
                strike.built = true;
            }
            skyDome()->setLightingBlend(1.0f);
        }
        if (strike.soundTime <= m_time)
        {
            // No null test on the sound, as shipped: a strike needs lightningSFXName first.
            m_sfxSounds[strike.buffer]->play(1);                              // ISound +0x10
            m_strikes[i] = m_strikes.back();
            m_strikes.pop_back();
            --i;
        }
    }
}

// 0x10156A30.
void LightningManager::render()
{
    for (unsigned int i = 0; i < m_strikes.size(); ++i)
        drawStrike(m_strikes[i]);
}

// 0x10156C50 - the queue's push_back (sub_1013AC80 on +0xA8).
void LightningManager::addLightning(const Vec2& offset)
{
    m_pending.push_back(offset);
}

// 0x10156D40.
void LightningManager::processPendingLightning()
{
    for (unsigned int i = 0; i < m_pending.size(); ++i)
        createStrike(m_pending[i]);
    m_pending.clear();
}

// 0x10156C60. The strike stands on the terrain under (x, z) when a heightmap cluster exists,
// else at height 0; the camera position is added only when it is drawn.
void LightningManager::createStrike(const Vec2& offset)
{
    world::IHeightmapCluster* const cluster = heightmapCluster();
    float height = 0.0f;
    if (cluster)
    {
        Vec3 probe;
        probe.y = cluster->getSeaWaterLevel();                               // +0x60
        probe.x = offset.x;
        probe.z = offset.y;
        height = cluster->getHeightInWorldCoords(probe);                     // +0x68
    }

    LightningStrike strike;
    strike.position.x = offset.x;
    strike.position.y = height - m_groundPenetration;
    strike.position.z = offset.y;
    strike.flashEndTime = m_duration + m_time;
    strike.soundTime = m_sfxDelay + m_time;
    strike.built = false;
    strike.buffer = m_nextBuffer;
    ++m_nextBuffer;
    if (m_nextBuffer >= 3)
        m_nextBuffer = 0;
    m_strikes.push_back(strike);
}

// 0x10155DA0. Rewrites one bolt buffer: rows 0..100 from y = 1 down to y = 0, the left vertex of
// each row moving the bolt's x by a random step of up to +-pertubation/2 (in units of scale.x).
// The alpha fades in over inFadeSegments rows from the first drawn row and out over
// outFadeSegments rows towards row 100. A pertubation below 0.001 * scale.x divides by zero, as
// shipped.
void LightningManager::buildBoltBuffer(int buffer)
{
    LightningVertex* vertex = static_cast<LightningVertex*>(
        dx9rend::vertexBufferLock(m_vertexBuffers[buffer], 0, 0, D3DLOCK_DISCARD));
    const float pertubation = m_pertubation / m_scale.x;
    float offset = 0.0f;
    for (unsigned int i = 0; i < kBoltVertexCount; ++i, ++vertex)
    {
        const unsigned int side = i % 2;
        const unsigned int row = i / 2;
        if (side == 0)
        {
            const int r = std::rand();
            const int range = truncateToInt(pertubation * 1000.0f);
            offset = static_cast<float>(r % range) * 0.001f - pertubation * 0.5f + offset;
        }
        vertex->z = 0.0f;
        vertex->x = static_cast<float>(static_cast<int>(side)) * 2.0f - 1.0f + offset;
        const float rowF = static_cast<float>(static_cast<int>(row));
        vertex->y = 1.0f - rowF * 0.01f;
        vertex->u = (side == 1) ? 1.0f : 0.0f;
        vertex->v = static_cast<float>(static_cast<int>(row) % 2);

        float fromStart = rowF - (100.0f - static_cast<float>(m_segments));
        if (fromStart < 0.0f)
            fromStart = 0.0f;
        const float toEnd = 100.0f - rowF;

        float fadeIn;
        if (m_inFadeSegments != 0.0f)
        {
            const float a = fromStart / m_inFadeSegments;
            fadeIn = (1.0f < a) ? 1.0f : a;
        }
        else
            fadeIn = 1.0f;

        float fadeOut;
        if (m_outFadeSegments != 0.0f)
        {
            const float b = toEnd / m_outFadeSegments;
            fadeOut = (1.0f < b) ? 1.0f : b;
        }
        else
            fadeOut = 1.0f;

        const float fade = (fadeIn < fadeOut) ? fadeIn : fadeOut;
        const unsigned int level = static_cast<unsigned int>(truncateToInt64(fade * 255.0f));
        const unsigned int low = level & 0xFFu;
        vertex->color = (((((level << 8) | low) << 8) | low) << 8) | low;
    }
    dx9rend::vertexBufferUnlock(m_vertexBuffers[buffer]);
}

// 0x10156920. The far plane is restored only when the constants were set, as shipped.
void LightningManager::drawStrike(const LightningStrike& strike)
{
    dx9rend::stateManagerPush();
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();   // Renderer +0x50
    const float savedFarPlane = view->getFarPlane();
    view->setFarPlane(g_lightningFarPlane);
    dx9rend::vertexDeclarationBind(m_vertexDeclaration);
    dx9rend::vertexBufferBindStream(m_vertexBuffers[strike.buffer], 0);
    if (setStrikeShaderConstants(strike))
    {
        dx9rend::effectBegin(m_effect);
        dx9rend::effectBeginPass(m_effect, 0);
        // D3DPT_TRIANGLESTRIP over the last lightningSegments rows of the strip.
        dx9rend::rendererDrawPrimitive(static_cast<unsigned int>(m_segments + m_segments), 5,
                                       static_cast<unsigned int>((100 - m_segments) * 2));
        dx9rend::effectEnd(m_effect);
        view->setFarPlane(savedFarPlane);
    }
    dx9rend::stateManagerPop();
}

// 0x10156420. Faces the strip towards the camera around the vertical axis, scales it by
// lightningScale and places it at camera + strike offset. A camera looking straight up or down
// has no horizontal direction, and the strike is skipped.
bool LightningManager::setStrikeShaderConstants(const LightningStrike& strike)
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();   // Renderer +0x50
    const Mat4* const camera = view->getViewMatrixPtr();                       // +0x64
    Vec3 facing;
    facing.x = -camera->m[8];
    facing.y = 0.0f;
    facing.z = -camera->m[10];
    if (facing.z * facing.z + facing.x * facing.x == 0.0f)
        return false;
    rendmath::normalize(facing);

    // The basis: right = up x facing, up, facing. The zero products are the shipped ones.
    Mat4 rotation;
    rotation.m[0]  = facing.z - facing.y * 0.0f;
    rotation.m[1]  = facing.x * 0.0f - facing.z * 0.0f;
    rotation.m[2]  = facing.y * 0.0f - facing.x;
    rotation.m[3]  = 0.0f;
    rotation.m[4]  = 0.0f;
    rotation.m[5]  = 1.0f;
    rotation.m[6]  = 0.0f;
    rotation.m[7]  = 0.0f;
    rotation.m[8]  = facing.x;
    rotation.m[9]  = facing.y;
    rotation.m[10] = facing.z;
    rotation.m[11] = 0.0f;
    rotation.m[12] = 0.0f;
    rotation.m[13] = 0.0f;
    rotation.m[14] = 0.0f;
    rotation.m[15] = 1.0f;

    Mat4 scale = Mat4::getIdentity();
    Vec3 scaleFactors;
    scaleFactors.x = m_scale.x;
    scaleFactors.y = m_scale.y;
    scaleFactors.z = 1.0f;
    rendmath::scaleColumns(scale, scaleFactors);

    const Mat4* const cameraAgain = view->getViewMatrixPtr();                  // +0x64 again
    Mat4 translation = Mat4::getIdentity();
    translation.m[12] = cameraAgain->m[12] + strike.position.x;
    translation.m[13] = strike.position.y;
    translation.m[14] = cameraAgain->m[14] + strike.position.z;

    Mat4 world = Mat4::getIdentity();
    multiplyInPlace(world, scale);
    multiplyInPlace(world, rotation);
    multiplyInPlace(world, translation);
    Mat4 worldViewProj = Mat4::getIdentity();
    multiplyInPlace(worldViewProj, world);
    multiplyInPlace(worldViewProj, *view->getViewProjectionMatrixPtr());       // +0x84
    m_paramWorldViewProj->setValue(&worldViewProj, 0);

    // Alpha: the time left in the flash over lightningBlendOutTime, clamped to [0, 1].
    double remaining = strike.flashEndTime - m_time;
    if (remaining < 0.0)
        remaining = 0.0;
    double alpha = remaining / m_blendOutTime;
    if (1.0 < alpha)
        alpha = 1.0;
    Vec4 color;
    color.x = m_color.x;
    color.y = m_color.y;
    color.z = m_color.z;
    color.w = static_cast<float>(alpha);
    m_paramColor->setValue(&color, 0);
    m_paramTexture->setValue(m_texture, 0);
    return true;
}


// The console properties.

void LightningManager::setLightningDuration(float seconds) { m_duration = seconds; }
float LightningManager::getLightningDuration() const { return m_duration; }

void LightningManager::setLightningScale(const Vec2& scale)
{
    m_scale.y = scale.y;
    m_scale.x = scale.x;
}
const Vec2& LightningManager::getLightningScale() const { return m_scale; }

void LightningManager::setLightningPertubation(float amount) { m_pertubation = amount; }
float LightningManager::getLightningPertubation() const { return m_pertubation; }

void LightningManager::setLightningColor(const Vec4& color)
{
    m_color.y = color.y;
    m_color.x = color.x;
    m_color.z = color.z;
    m_color.w = color.w;
}
const Vec4& LightningManager::getLightningColor() const { return m_color; }

void LightningManager::setLightningBlendOutTime(float seconds) { m_blendOutTime = seconds; }
float LightningManager::getLightningBlendOutTime() const { return m_blendOutTime; }

void LightningManager::setSkyBlendOutTime(float seconds) { m_skyBlendOutTime = seconds; }
float LightningManager::getSkyBlendOutTime() const { return m_skyBlendOutTime; }

void LightningManager::setLightningSFXDelay(float seconds) { m_sfxDelay = seconds; }
float LightningManager::getLightningSFXDelay() const { return m_sfxDelay; }

// 0x101560B0. The template is looked up by name and the Sound class (ObjectTemplateManager PE
// +0x30); a miss leaves only the name. Each instance comes from the Sound singleton (PE +0x1C),
// which is used without a null test, as shipped.
void LightningManager::setLightningSFXName(const std::string& name)
{
    releaseSoundEffect();
    m_sfxName = name;
    m_sfxTemplate = reinterpret_cast<world::ObjectTemplateManager*>(objectTemplateManager)
        ->getTemplate(renddx9abi::toHostString(name), kCID_SoundObjectTemplate);
    if (!m_sfxTemplate)
        return;
    m_sfxTemplate->addRef();
    ISoundObjectTemplate* const sound = static_cast<SoundObjectTemplate*>(m_sfxTemplate);
    sound->setIsUsed();                                                       // +0x10
    sound->impl()->setFlags(1);                                               // +0x78, +0xC4
    for (int i = 0; i < 3; ++i)
        m_sfxSounds[i] = m_sound->createSound(sound->impl(), 0);
}
const std::string& LightningManager::getLightningSFXName() const { return m_sfxName; }

void LightningManager::setLightningSegments(int segments)
{
    if (segments >= 100)
        segments = 100;
    m_segments = segments;
}
int LightningManager::getLightningSegments() const { return m_segments; }

void LightningManager::setLightningGroundPenetration(float depth) { m_groundPenetration = depth; }
float LightningManager::getLightningGroundPenetration() const { return m_groundPenetration; }

void LightningManager::setInFadeSegments(float segments) { m_inFadeSegments = segments; }
float LightningManager::getInFadeSegments() const { return m_inFadeSegments; }

void LightningManager::setOutFadeSegments(float segments) { m_outFadeSegments = segments; }
float LightningManager::getOutFadeSegments() const { return m_outFadeSegments; }


// 0x100B0BA4: an aggregate outer is refused; a failed query releases the new object.
IBase* __fastcall createLightningManager(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(LightningManager));
    LightningManager* const self = storage ? new (storage) LightningManager() : 0;
    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
