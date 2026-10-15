// [TEMP] [CHOSEN, not recovered]
// Tests the production .dif tokenizer through DifFont::load.
// File, texture, and device observers stop after header parsing; font rendering is not exercised.
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "../BF2/GeomMisc/FontSystem/SwiffFont.cpp"

namespace {

const char* g_difText = 0;
std::string g_textureRequest;
int g_checks = 0;
int g_failures = 0;

bool check(bool condition, const char* label)
{
    ++g_checks;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition)
        ++g_failures;
    return condition;
}

bool loadAndCheck(const char* label,
                  const char* difText,
                  unsigned int expectedWidth,
                  unsigned int expectedHeight,
                  float expectedSize)
{
    g_difText = difText;
    g_textureRequest.clear();

    dice::hfe::geom::DifFont font;
    const bool loaded = font.load("Fonts/header_fixture");

    char message[160];
    std::snprintf(message, sizeof(message), "%s loads through DifFont::load", label);
    check(loaded, message);
    std::snprintf(message, sizeof(message), "%s preserves atlas width", label);
    check(font.atlasW() == expectedWidth, message);
    std::snprintf(message, sizeof(message), "%s preserves atlas height", label);
    check(font.atlasH() == expectedHeight, message);
    std::snprintf(message, sizeof(message), "%s preserves authored size", label);
    check(std::fabs(font.size() - expectedSize) < 0.0001f, message);
    std::snprintf(message, sizeof(message), "%s reaches the matching DDS path", label);
    check(g_textureRequest == "Fonts/header_fixture.dds", message);
    return loaded && font.atlasW() == expectedWidth && font.atlasH() == expectedHeight
        && std::fabs(font.size() - expectedSize) < 0.0001f;
}

} // anonymous namespace

// Explicit fixture collaborators for the production load body included above.
namespace renddx9abi {

EngineFileData readEngineFile(const char*)
{
    EngineFileData result;
    if (g_difText)
    {
        result.data = reinterpret_cast<unsigned char*>(const_cast<char*>(g_difText));
        result.size = static_cast<unsigned int>(std::strlen(g_difText));
    }
    return result;
}

void freeEngineFile(EngineFileData& file)
{
    // The bytes are string literals owned by this fixture, not an engine allocation.
    file.data = 0;
    file.size = 0;
}

void logf(const char*, ...)
{
}

unsigned int addr(const void* value)
{
    return static_cast<unsigned int>(reinterpret_cast<uintptr_t>(value));
}

} // namespace renddx9abi

namespace dice {
namespace hfe {

IClassManager* classManager = 0;

namespace dx9rend {

// Unreached renderer collaborators retained by the production TU's hand-built vtable arrays.
// They are explicit fixture-only null implementations; the two assertions above exercise only
// DifFont::load and would fail before any of these rendering paths could be entered.
void* g_shaderManager = 0;

int readIntSetting(const char*, int fallback)
{
    return fallback;
}

void readStringSetting(const char*, std::string& value)
{
    value = "english";
}

void* textureManagerGetTexture(const std::string& name)
{
    g_textureRequest = name.c_str();
    return reinterpret_cast<void*>(1);
}

void textureManagerEvictFromCache(void*)
{
}

void* renderDevice()
{
    return 0;
}

void* shaderManagerGetEffect(void*, const char*) { return 0; }
void* effectGetShaderParameter(Effect*, const std::string&) { return 0; }
void* effectFindTechniqueRecord(void*, const char*) { return 0; }
void effectSetTechniqueRecord(void*, void*) {}
unsigned int effectBegin(void*) { return 0; }
void effectBeginPass(void*, unsigned int) {}
void effectEnd(void*) {}
void* effectD3dxHandle(void*) { return 0; }
int bufferManagerSwapPool(int value) { return value; }
void* bufferManagerCreateVertexBuffer(unsigned int, unsigned int, unsigned int, const char*)
{
    return 0;
}
void* vertexBufferLock(void*, unsigned int, unsigned int, unsigned int) { return 0; }
bool vertexBufferUnlock(void*) { return false; }
void vertexBufferBindStream(void*, unsigned int) {}
void* vertexDeclarationCreate(const void*) { return 0; }
void vertexDeclarationBind(void*) {}
void rendererDrawPrimitive(unsigned int, unsigned int, unsigned int) {}

void* ShaderParameter::getD3DXParam() const
{
    return 0;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

int main()
{
    // Stock dynamicText_13.dif: the embedded spaces used to shift LT/CondensedBold into the
    // numeric fields, yielding atlas 0x0 and authored size 256.
    const char multiword[] =
        "header\n"
        "2\n"
        "Helvetica LT CondensedBold\n"
        "256\n"
        "128\n"
        "17.000000\n";

    // Stock 800/digitalNumbers.dif shape: its version, name, dimensions and size are padded with
    // tabs.  This catches a line reader that consumes TABs and CR/LF in two non-repeating passes.
    const char paddedSingleWord[] =
        "header\r\n"
        "2\t\t\t\t\t\t\t\t\r\n"
        "Arial\t\t\t\t\t\t\t\t\r\n"
        "64\t\t\t\t\t\t\t\t\r\n"
        "22\t\t\t\t\t\t\t\t\r\n"
        "10\t\t\t\t\t\t\t\t\r\n";

    loadAndCheck("multi-word header", multiword, 256, 128, 17.0f);
    loadAndCheck("TAB-padded single-word header", paddedSingleWord, 64, 22, 10.0f);

    std::printf("swiff font header runtime: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
