// Path [INFERRED], class descriptor at BF2.exe 0x935348 attests
// dice::hfe::menu::Ref2System. Client-PE recovery; no dedicated ELF counterpart.
#include "Ref2System.h"
#if !DEDICATED
#include "../Math/Mat4.h"
#include "../Core/ClassManager.h"
#include "../Core/Ids.h"
#include "../Core/SettingsRepostitory.h"
#include "../GeomMisc/MenuRend/IQuadDrawer.h"
#include "../RendMisc/IRenderer.h"
#include "../RendMisc/IRenderView.h"
#include "../External/BFMeme/FileIStream.h"
#include "../External/BFMeme/FileOStream.h"
#include "../External/BFMeme/DynamicIndexPointer.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include "../IO/File/FileManager.h"
#include "Font.h"
#include <cstdlib>
#include <cstring>
#include <new>

namespace dice { namespace hfe {
extern ClassManager* classManager;
namespace menu {
namespace {
// PE table 0x93527C, storage A20F68, guard A20F78 and counter A20F64. The
// factory at 0x7AF110 supplies the two zero dimensions used by retail.
class Ref2SystemTypeDescriptor final : public meme::TypeDescriptor
{
public:
    Ref2SystemTypeDescriptor() : meme::TypeDescriptor("dice::hfe::menu::Ref2System") {}
    meme::TypeDescriptor* getBaseType() override { return meme::System::getClassType(); }
    meme::RuntimeObject* create() override
    {
        void* allocation = ::operator new(sizeof(Ref2System));
        return allocation ? new(allocation) Ref2System(0.0f, 0.0f) : nullptr;
    }
    const char* getDescription() override { return nullptr; }
    void addInstance() override { ++instanceCount; }
    void removeInstance() override { --instanceCount; }
    int getInstanceCount() override { return instanceCount; }
    unsigned int getInstanceSize() override { return sizeof(Ref2System); } // 7AD500, retail 0x690
private:
    static int instanceCount;
};
int Ref2SystemTypeDescriptor::instanceCount = 0;
#if defined(_M_IX86)
static_assert(sizeof(Ref2SystemTypeDescriptor) == 16,
              "Ref2System metadata stores its counter separately");
#endif

// A20F78 is a plain dword whose bit zero is set before construction. Retail has no lock and no
// exception rollback here; avoid the compiler's C++11 thread-safe function-static guard.
alignas(Ref2SystemTypeDescriptor) unsigned char ref2SystemTypeStorage[sizeof(Ref2SystemTypeDescriptor)] = {};
unsigned int ref2SystemTypeGuard = 0;

Ref2SystemTypeDescriptor* ref2SystemTypeDescriptor()
{
    return reinterpret_cast<Ref2SystemTypeDescriptor*>(ref2SystemTypeStorage);
}

void destroyRef2SystemTypeDescriptor()
{
    ref2SystemTypeDescriptor()->~Ref2SystemTypeDescriptor();
}

// Client A20F7C, native map. A local cache hit does not add a reference;
// first use by another Ref2System does. close7AEF10 must share this cache.
std::map<std::string, Font*> sharedFonts;
}

int Ref2OutputStream::write(const void* buffer, int count)
{
    // Retail constructs a NUL-terminated string, then destroys it. The count
    // does not bound that read, and is returned unchanged; no console call.
    const std::string text(static_cast<const char*>(buffer));
    return count;
}

Ref2System::Ref2System(float width, float height)
    : meme::System(width, height), m_textureList(nullptr)
{
    // The embedded OStream is default-constructed. Neither the blank texture,
    // input-state table nor renderView is initialized at 0x7AF010.
    for (unsigned int i = 0; i != 4; ++i)
    {
        io::GameInput& input = m_inputs[i];
        input.mappingFlags = 0;
        input.dword0 = 0;
        input.dword1 = 0;
        input.flag = 0;
        std::memset(input.inputs, 0, sizeof(input.inputs));
        // +139..+13F remain untouched, including alignment padding.
    }
    m_renderHeight = height;
    m_previousRenderView = nullptr;
    m_renderer = nullptr;
    m_renderWidth = width;
    m_field684 = nullptr;
    m_textureFlags = false;
    m_field68c = 0;
}

// only +684 is released here. The map destroys its strings/nodes,
// not its Font* payloads. Fonts/textures/renderView belong to separate close.
Ref2System::~Ref2System()
{
    if (m_field684)
        m_field684->release();
}

meme::TypeDescriptor* Ref2System::getType()
{
    if ((ref2SystemTypeGuard & 1u) == 0)
    {
        ref2SystemTypeGuard |= 1u;
        new(ref2SystemTypeStorage) Ref2SystemTypeDescriptor;
        std::atexit(&destroyRef2SystemTypeDescriptor);
    }
    return ref2SystemTypeDescriptor();
}

bool Ref2System::init() // 7AD030, retain the base result without early-out
{
    const bool result = System::init();
    loadFont("BF1942.font");
    m_renderView = classManager->createInstance(820301u, 820201u, nullptr);
    m_renderer = classManager->getSingleton(std::string("Renderer"));
    std::memset(m_inputStates, 0, sizeof(m_inputStates));
    meme::createObject(this, m_field020, meme::DynamicIndexPointer::getClassType());
    m_blankTexture = static_cast<geom::IMemeTexture*>(classManager->createInstance(
        geom::CID_MemeTexture, geom::IID_IMemeTexture, nullptr));
    m_blankTexture->setTexture("", "", 0); // No allocation-result guard in retail.
    return result;
}
bool Ref2System::close()
{
    const bool result = System::close();
    m_renderView->release();
    if (m_textureList) m_textureList->release(); // Only the head; no chain walk here.
    m_blankTexture->release();
    for (const auto& entry : m_fonts) {
        Font* font = entry.second;
        if (--font->referenceCount() == 0) delete font;
        else if (font->referenceCount() == 1) {
            sharedFonts.erase(entry.first); // Retail removes the equal-key range.
            if (--font->referenceCount() == 0) delete font;
        }
    }
    // Shipped code does not clear the local map or any released resource field.
    // The zero-on-first-decrement arm also leaves the global cache entry intact.
    return result;
}

unsigned int Ref2System::getObjectSize() { return sizeof(Ref2System); }
void Ref2System::setInput(const io::GameInput& input, int user)
{
    if (user >= 0 && user < 4)
        std::memcpy(&m_inputs[user], &input, sizeof(input)); // Includes native320-byte padding.
}
void Ref2System::setSize(float, float) {} // 5A59F0, shipped empty override
void Ref2System::sizeChanged(const float* oldSize, const float* newSize)
{
    if (newSize[0] != oldSize[0] || newSize[1] != oldSize[1]) {
        m_renderWidth = newSize[0];
        m_renderHeight = newSize[1];
    }
}
bool Ref2System::update(float delta) // only strictly greater than one resets
{
    if (delta > 1.0f) delta = 0.0f;
    return System::update(delta);
}

bool Ref2System::preUpdate(float delta) // 7AD8B0..7AE6A3, all instructions read
{
    // [INFERRED] Repeat-setting names. A long tick emits one event with the accumulated repeat count.
    // Intermediate states are rounded to float before conversion.
    const float repeatThreshold = 3.0f;
    const float repeatRate = 10.0f;
    for (int user = 0; user < 4; ++user) {
        io::GameInput& input = m_inputs[user];
        float* states = m_inputStates[user];
        const auto updateInput = [&](io::GameInputMap mapping, int stateIndex,
                                     int eventCode, bool repeats) {
            float& state = states[stateIndex];
            if ((input.mappingFlags & (1ULL << mapping)) && input.inputs[mapping] > 0.5f) {
                if (state == 0.0f) {
                    state = 1.0f;
                    // System82A2D0 loads+20 then tail-calls this guarded wrapper.
                    // Reload the pointer for every event; return values are ignored.
                    meme::pointerEvent1(this, static_cast<meme::Pointer*>(m_field020),
                                        eventCode, user + 1, 1);
                } else if (repeats) {
                    const int previous = static_cast<int>(state);
                    state = repeatRate * delta + state;
                    const int current = static_cast<int>(state);
                    if (current > repeatThreshold && previous != current) {
                        const int count = static_cast<int>(state - (repeatThreshold - 1.0f));
                        meme::pointerEvent1(this, static_cast<meme::Pointer*>(m_field020),
                                            eventCode, user + 1, count);
                    }
                }
            } else if (state != 0.0f) {
                state = 0.0f;
                // System82A2E0 has the matching pointerEvent2 tail-call.
                meme::pointerEvent2(this, static_cast<meme::Pointer*>(m_field020),
                                    eventCode, user + 1);
            }
        };
        // Retail order is neither input-enum nor state-index order. Slot9 is
        // untouched. Back and Cancel deliberately share event code3.
        updateInput(io::GIOk,             0,  1, false);
        updateInput(io::GIAltOk,          1, 21, false);
        updateInput(io::GIEnter,          2, 18, true);
        updateInput(io::GITab,            3, 16, true);
        updateInput(io::GICancel,         4,  3, true);
        updateInput(io::GIUp,            8,  6, true);
        updateInput(io::GIDown,          7,  8, true);
        updateInput(io::GIPageUp,       14, 11, true);
        updateInput(io::GIPageDown,     15, 12, true);
        updateInput(io::GIMouseWheelUp, 10, 19, true);
        updateInput(io::GIMouseWheelDown,11,20, true);
        updateInput(io::GILeft,          5,  5, true);
        updateInput(io::GIRight,         6,  7, true);
        updateInput(io::GIDelete,       12, 14, true);
        updateInput(io::GIBack,         13,  3, true);
    }
    return true; // Explicit mov al,1 at7AE69A; no InputManager poll occurs here.
}

bool Ref2System::paint() { return System::paint(); }
meme::OStream* Ref2System::getOutputStream() { return &m_output; }
meme::IStream* Ref2System::createInputStream(const char* name)
{
    std::string path;
    if (io::fileManager->fileExists(std::string(name))) path = name;
    else {
        path = std::string("menu/") + name;
        if (!io::fileManager->fileExists(path)) return nullptr;
    }
    void* storage = ::operator new(sizeof(meme::FileIStream));
    return storage ? new(storage) meme::FileIStream(path.c_str()) : nullptr;
}

meme::OStream* Ref2System::createOutputStream(const char* name)
{
    const std::string path = std::string("menu/") + name;
    void* storage = ::operator new(sizeof(meme::FileOStream));
    return storage ? new(storage) meme::FileOStream(path.c_str()) : nullptr;
}

bool Ref2System::drawCharacter(int character, meme::FontHandle handle, meme::Quad* quad)
{
    Font* font = reinterpret_cast<Font*>(handle);
    if (!font) {
        if (m_fonts.empty()) return false;
        font = m_fonts.begin()->second;
    }
    return font->drawCharacter(character, quad);
}
bool Ref2System::getGlyphMetrics(int character, meme::FontHandle handle, meme::GlyphMetrics* out)
{
    Font* font = reinterpret_cast<Font*>(handle);
    if (!font) font = m_fonts.begin()->second; // Shipped missing empty-map guard.
    return font->getGlyphMetrics(character, out);
}
float Ref2System::getKerning(int previous, int current, meme::FontHandle handle)
{
    Font* font = reinterpret_cast<Font*>(handle);
    if (!font) font = m_fonts.begin()->second;
    return font->getKerning(previous, current);
}
float Ref2System::getFontHeight(meme::FontHandle handle)
{
    Font* font = reinterpret_cast<Font*>(handle);
    if (!font) font = m_fonts.begin()->second;
    return font->getHeight();
}
const char* Ref2System::getFontName(meme::FontHandle handle)
{ return handle ? reinterpret_cast<Font*>(handle)->getName() : nullptr; }

meme::FontHandle Ref2System::loadFont(const char* name)
{
    const auto own = m_fonts.find(std::string(name));
    if (own != m_fonts.end()) return reinterpret_cast<meme::FontHandle>(own->second);
    const auto shared = sharedFonts.find(std::string(name));
    Font* font;
    if (shared != sharedFonts.end()) {
        font = shared->second;
        m_fonts.insert(std::make_pair(shared->first, font));
    } else {
        font = loadFontFile(name);
        if (!font) {
            char filename[260], extension[32];
            _splitpath(name, nullptr, nullptr, filename, extension);
            std::string path("Fonts\\");
            path += settings->stringGet("GSLanguage", "") + "\\";
            path += filename;
            path += extension;
            font = loadFontFile(path.c_str());
            if (!font) return 0;
        }
        m_fonts.insert(std::make_pair(std::string(name), font));
        sharedFonts.insert(std::make_pair(std::string(name), font));
    }
    ++font->referenceCount();
    return reinterpret_cast<meme::FontHandle>(font);
}

bool Ref2System::drawTexture(meme::TextureHandle texture, meme::Quad* request)
{
    if (texture) {
        geom::IMemeTexture* object = reinterpret_cast<geom::IMemeTexture*>(texture);
        return object->drawOther(object, request);
    }
    if (m_field054) {
        geom::IMemeTexture* object = reinterpret_cast<geom::IMemeTexture*>(m_field054);
        return object->drawOther(object, request);
    }
    return m_blankTexture->drawOther(nullptr, request);
}
meme::TextureHandle Ref2System::loadTexture(const char* name, bool menuPath)
{
    geom::IMemeTexture* texture = m_textureList;
    while (texture) {
        if (!_stricmp(texture->getName(), name)) break;
        texture = static_cast<geom::IMemeTexture*>(texture->getUserObject());
    }
    if (!texture) {
        texture = static_cast<geom::IMemeTexture*>(classManager->createInstance(
            geom::CID_MemeTexture, geom::IID_IMemeTexture, nullptr));
        texture->setTexture(name, menuPath ? "Menu/HUD/Texture/" : "", m_textureFlags);
        texture->setUserObject(m_textureList);
        m_textureList = texture;
    }
    return reinterpret_cast<meme::TextureHandle>(texture);
}
void Ref2System::releaseTexture(meme::TextureHandle handle)
{
    if (!handle) return;
    geom::IMemeTexture* texture = reinterpret_cast<geom::IMemeTexture*>(handle);
    geom::IMemeTexture* previous = nullptr;
    geom::IMemeTexture* current = m_textureList;
    while (current && current != texture) {
        previous = current;
        current = static_cast<geom::IMemeTexture*>(current->getUserObject());
    }
    if (!current) return;
    if (previous) previous->setUserObject(texture->getUserObject());
    else m_textureList = static_cast<geom::IMemeTexture*>(texture->getUserObject());
    texture->releaseTexture();
    texture->setUserObject(nullptr);
    texture->release();
}
const char* Ref2System::getTextureName(meme::TextureHandle handle)
{
    return handle ? reinterpret_cast<geom::IMemeTexture*>(handle)->getName() : nullptr;
}
float Ref2System::getTextureWidth(meme::TextureHandle handle) // 7AD330, signed integer conversion
{
    return handle ? static_cast<float>(reinterpret_cast<geom::IMemeTexture*>(handle)->getPixelWidth()) : 0.0f;
}
float Ref2System::getTextureHeight(meme::TextureHandle handle)
{
    return handle ? static_cast<float>(reinterpret_cast<geom::IMemeTexture*>(handle)->getPixelHeight()) : 0.0f;
}

// repeated object reads/calls and fixed 800x600 size are intentional.
void Ref2System::prepareRenderView()
{
    static_cast<dx9rend::IRenderView*>(m_renderView)->setProjectionMode(1);
    static_cast<dx9rend::IRenderView*>(m_renderView)->setNearPlane(0.01f);
    static_cast<dx9rend::IRenderView*>(m_renderView)->setFarPlane(1.0f);
    static_cast<dx9rend::IRenderView*>(m_renderView)->setOrthoWidth(800.0f);
    static_cast<dx9rend::IRenderView*>(m_renderView)->setOrthoHeight(600.0f);
    m_previousRenderView = static_cast<dx9rend::IRenderer*>(m_renderer)->getRenderView();
}

// 0x7AD6D0 calls 0x6D9990, the client's setRotateXDeg. The complete identity
// setup precedes that helper even though the helper writes identity again.
void Ref2System::prepareViewMatrix()
{
    Mat4 matrix = {{ 1, 0, 0, 0, 0, 1, 0, 0,
                     0, 0, 1, 0, 0, 0, 0, 1 }};
    setRotateXDeg(matrix, 180.0f);
    matrix.m[14] += 0.02f;
    static_cast<dx9rend::IRenderView*>(m_renderView)->setViewMatrix(matrix.m);
}

void Ref2System::beginPaint()
{
    prepareRenderView();
    prepareViewMatrix();
    static_cast<dx9rend::IRenderer*>(m_renderer)->setRenderView(m_renderView);
    geom::IQuadDrawer* drawer = static_cast<geom::IQuadDrawer*>(
        classManager->getSingleton(std::string("QuadDrawer")));
    drawer->beginFrame();
    drawer->resetState();
}
void Ref2System::endPaint()
{
    geom::IQuadDrawer* drawer = static_cast<geom::IQuadDrawer*>(
        classManager->getSingleton(std::string("QuadDrawer")));
    drawer->flush();
    drawer->endFrame();
    static_cast<dx9rend::IRenderer*>(m_renderer)->setRenderView(m_previousRenderView);
}

static_assert(sizeof(io::GameInput) == 0x140, "BF2 client GameInput stride");

} } }
#endif
