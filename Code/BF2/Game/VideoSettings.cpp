// [INFERRED] path. Client-only class read from BF2.exe (table 8A3910, 65 slots, factory 4105FB);
// the dedicated ELF names only the singleton.
#include "VideoSettings.h"
#if !DEDICATED
#include "IGame.h"                        // game - applyToOptions and hasChangesInGame test it
#include "Main/BF2ClientSeams.h"          // g_bf2Render - the renderer handle lives at its +10
#include "Main/IVideoOptions.h"
#include "../Core/Ids.h"
#include "../RendMisc/IRenderer.h"
#include "../Core/SettingsRepostitory.h"  // settings - "GSLoadLevel" gates the antialiasing push
#include <cstdio>                       // [CHOSEN] sscanf for the new-profile resolution pick
#include <new>
#include <ostream>
#include <sstream>

namespace dice { namespace hfe {

// (101 B): both strings constructed empty, the eight rows 2, then the two literals, 1.0f
// and scheme 1, in that order.
VideoSettingsData::VideoSettingsData()
    : terrainQuality(2), geometryQuality(2), lightingQuality(2), dynamicLightingQuality(2),
      dynamicShadowsQuality(2), effectsQuality(2), textureQuality(2), textureFilteringQuality(2),
      viewDistanceScale(1.0f), videoOptionScheme(1)
{
    resolution = "800x600@60Hz";
    antialiasing = "Off";
}

// (100 B): memberwise; revertChanges 4B2890 and apply 4B2810 test the two records for
// identity before calling it, which the assignment keeps as its own self test.
VideoSettingsData& VideoSettingsData::operator=(const VideoSettingsData& other)
{
    if (this != &other) {
        terrainQuality = other.terrainQuality;
        geometryQuality = other.geometryQuality;
        lightingQuality = other.lightingQuality;
        dynamicLightingQuality = other.dynamicLightingQuality;
        dynamicShadowsQuality = other.dynamicShadowsQuality;
        effectsQuality = other.effectsQuality;
        textureQuality = other.textureQuality;
        textureFilteringQuality = other.textureFilteringQuality;
        resolution = other.resolution;
        antialiasing = other.antialiasing;
        viewDistanceScale = other.viewDistanceScale;
        videoOptionScheme = other.videoOptionScheme;
    }
    return *this;
}

// (180 B): the eight rows, the resolution, the scale, the scheme and last the antialiasing.
bool VideoSettingsData::differsFrom(const VideoSettingsData& other) const
{
    return terrainQuality != other.terrainQuality
        || geometryQuality != other.geometryQuality
        || lightingQuality != other.lightingQuality
        || dynamicLightingQuality != other.dynamicLightingQuality
        || dynamicShadowsQuality != other.dynamicShadowsQuality
        || effectsQuality != other.effectsQuality
        || textureQuality != other.textureQuality
        || textureFilteringQuality != other.textureFilteringQuality
        || !(resolution == other.resolution)
        || viewDistanceScale != other.viewDistanceScale
        || videoOptionScheme != other.videoOptionScheme
        || !(antialiasing == other.antialiasing);
}

// (~500 B): twelve rows; the two strings insert c_str() unquoted, the scale prints as a float.
void VideoSettingsData::write(std::ostream& stream) const
{
    stream << "VideoSettings.setTerrainQuality " << terrainQuality << std::endl;
    stream << "VideoSettings.setGeometryQuality " << geometryQuality << std::endl;
    stream << "VideoSettings.setLightingQuality " << lightingQuality << std::endl;
    stream << "VideoSettings.setDynamicLightingQuality " << dynamicLightingQuality << std::endl;
    stream << "VideoSettings.setDynamicShadowsQuality " << dynamicShadowsQuality << std::endl;
    stream << "VideoSettings.setEffectsQuality " << effectsQuality << std::endl;
    stream << "VideoSettings.setTextureQuality " << textureQuality << std::endl;
    stream << "VideoSettings.setTextureFilteringQuality " << textureFilteringQuality << std::endl;
    stream << "VideoSettings.setResolution " << resolution.c_str() << std::endl;
    stream << "VideoSettings.setAntialiasing " << antialiasing.c_str() << std::endl;
    stream << "VideoSettings.setViewDistanceScale " << viewDistanceScale << std::endl;
    stream << "VideoSettings.setVideoOptionScheme " << videoOptionScheme << std::endl;
}

// (46 B): table, refcount 1, both records constructed, the restart flag cleared. The sixteen
// range values at +08..+44 are left to the allocator's contents by retail until setDefaults runs
// with a renderer; [CHOSEN, not recovered] they start at zero here so the getters are deterministic.
VideoSettings::VideoSettings() : m_refCount(1), m_optionRange(), m_restartRequired(false) {}

//: the four strings in reverse order (pending antialiasing, pending resolution, current
// antialiasing, current resolution), which the compiler reproduces, then the free.
VideoSettings::~VideoSettings() = default;

// (66 B): the settings-factory shape - aggregation refused, 268 retail bytes, construction
// only on a non-null allocation, unconditional queryInterface (the shipped null defect is kept).
IBase* VideoSettings::create(unsigned int iid, IBase* outer)
{
    if (outer) return nullptr;
    void* allocation = ::operator new(sizeof(VideoSettings));
    VideoSettings* owner = allocation ? new (allocation) VideoSettings : nullptr;
    IBase* result = static_cast<IBase*>(owner->queryInterface(iid));
    if (!result) owner->release();
    return result;
}
void VideoSettings::addRef() { ++m_refCount; }
int VideoSettings::getRef() { return m_refCount; }
// (26 B): decrement, destroy through the +F8 deleting destructor at zero.
int VideoSettings::release()
{
    const int references = --m_refCount;
    if (!references) delete this;
    return references;
}
// (36 B): IBase (dword_90D358 = 1), then IVideoSettings (dword_892C80 = 121041).
void* VideoSettings::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IVideoSettings)
        return const_cast<VideoSettings*>(this);
    return nullptr;
}

std::string VideoSettings::getFileName() { return std::string("Video.con"); }

// (77 B): the current record is written, not the pending one.
std::string VideoSettings::getSettingsScript()
{
    std::stringstream stream;
    m_current.write(stream);
    return stream.str();
}

// (16 B)
bool VideoSettings::hasChanges() { return m_pending.differsFrom(m_current); }

// Applies pending changes and latches restart requirements. Requires a live renderer.
// Missing renderer options return false but still invoke applyToRenderer.
bool VideoSettings::apply()
{
    bool result = true;
    if (!hasChanges())
        return result;
    if (needsRestart())
        m_restartRequired = true;
    IVideoOptions* options = g_bf2Render->getRenderer()->getVideoOptions();
    if (!options) {
        result = false;
        applyToRenderer(m_current);
        return result;
    }
    applyToOptions(options);
    options->apply();
    readOptions(options);
    m_current = m_pending;
    return true;
}

namespace {

// [CHOSEN, not recovered] The renderer's resolution entry ("%ix%i@%iHz") a new profile starts
// with: the largest that fits the renderer's desktop mode (IRenderer +0xDC, the adapter's
// GetAdapterDisplayMode; the Metal backend answers the main display's point size), preferring the
// desktop's refresh rate, then the highest. Empty when no entry fits or no mode is reported.
std::string desktopFitResolution(dx9rend::IRenderer* renderer, const std::vector<std::string>& list)
{
    unsigned int width = 0, height = 0, hz = 0;
    if (!renderer->getDesktopMode(width, height, hz))
        return std::string();
    const int desktopWidth = static_cast<int>(width), desktopHeight = static_cast<int>(height);
    const int desktopHz = static_cast<int>(hz);
    std::string best;
    long long bestArea = 0;
    int bestHz = 0;
    bool bestHzMatches = false;
    for (const std::string& entry : list) {
        int width = 0, height = 0, hz = 0;
        if (std::sscanf(entry.c_str(), "%dx%d@%d", &width, &height, &hz) != 3)
            continue;
        if (width > desktopWidth || height > desktopHeight)
            continue;
        const long long area = static_cast<long long>(width) * height;
        const bool hzMatches = hz == desktopHz;
        if (area > bestArea
            || (area == bestArea && hzMatches && !bestHzMatches)
            || (area == bestArea && hzMatches == bestHzMatches && hz > bestHz)) {
            best = entry;
            bestArea = area;
            bestHz = hz;
            bestHzMatches = hzMatches;
        }
    }
    return best;
}

// [CHOSEN, not recovered] Owner decision 2026-11-14: the High scheme also takes the highest
// antialiasing the renderer lists for the selected mode ("Off", then "%dSamples"); retail's High
// preset stages "Off". The list is rebuilt per resolution, so this runs after a resolution change.
void selectMaxAntialiasing(IVideoOptions* options)
{
    const std::vector<std::string>& list = options->getDynamicOptionList(0);
    std::string best;
    int bestSamples = 0;
    for (const std::string& entry : list) {
        int samples = 0;
        if (std::sscanf(entry.c_str(), "%dSamples", &samples) == 1 && samples > bestSamples) {
            best = entry;
            bestSamples = samples;
        }
    }
    if (!best.empty())
        options->setDynamicOption(0, best);
}

} // namespace

// (359 B). The pending record takes the constructor defaults field by field, then, with a
// renderer, the sixteen range values are read interleaved (+08 then +0C per index), the detected
// scheme replaces the default one and the scheme is pushed to the renderer with argument 1.
// Only the two new-profile paths call this (LocalProfileManager::createDefaultProfile 4A8113 and
// createProfile 4AAAA8); existing profiles keep their Video.con.
void VideoSettings::setDefaults()
{
    m_pending.terrainQuality = 2;
    m_pending.geometryQuality = 2;
    m_pending.lightingQuality = 2;
    m_pending.dynamicLightingQuality = 2;
    m_pending.dynamicShadowsQuality = 2;
    m_pending.effectsQuality = 2;
    m_pending.textureQuality = 2;
    m_pending.textureFilteringQuality = 2;
    m_pending.resolution = "800x600@60Hz";
    m_pending.antialiasing = "Off";
    m_pending.viewDistanceScale = 1.0f;
    m_pending.videoOptionScheme = 1;
    if (!g_bf2Render)
        return;
    dx9rend::IRenderer* renderer = g_bf2Render->getRenderer();
    if (!renderer)
        return;
    IVideoOptions* options = renderer->getVideoOptions();
    for (int index = 0; index < 8; ++index) {
        m_optionRange[index].minimum = options->getMinQuality(index);
        m_optionRange[index].maximum = options->getMaxQualityLevel(index);
    }
    m_pending.videoOptionScheme = detectVideoOptionScheme();
    applyVideoOptionScheme(1);
    // [CHOSEN, not recovered] High stages "1024x768"; a new profile takes the largest listed
    // resolution that fits the desktop instead, when one does.
    const std::string resolution = desktopFitResolution(renderer, getResolutionList());
    if (!resolution.empty()) {
        options->setDynamicOption(1, resolution);
        if (m_pending.videoOptionScheme == 2)
            selectMaxAntialiasing(options);
        readOptions(options);
    }
    std::fprintf(stderr, "[CHOSEN, not recovered] VideoSettings::setDefaults: new profile at scheme %d, "
                         "resolution %s, antialiasing %s\n", m_pending.videoOptionScheme,
                 m_pending.resolution.c_str(), m_pending.antialiasing.c_str());
}

// (20 B)
void VideoSettings::revertChanges() { m_pending = m_current; }

// (278 B): nothing is pushed while a game is loaded. Otherwise the eight rows in index order,
// the resolution (dynamic option 1), the antialiasing (dynamic option 0) only while no level is
// named in the settings repository, and the view-distance scale.
void VideoSettings::applyToOptions(IVideoOptions* options)
{
    if (game)
        return;
    options->setQuality(0, m_pending.terrainQuality);
    options->setQuality(1, m_pending.geometryQuality);
    options->setQuality(2, m_pending.lightingQuality);
    options->setQuality(3, m_pending.dynamicLightingQuality);
    options->setQuality(4, m_pending.dynamicShadowsQuality);
    options->setQuality(5, m_pending.effectsQuality);
    options->setQuality(6, m_pending.textureQuality);
    options->setQuality(7, m_pending.textureFilteringQuality);
    options->setDynamicOption(1, m_pending.resolution);
    if (settings->stringGet(std::string("GSLoadLevel"), std::string("")).size() == 0)
        options->setDynamicOption(0, m_pending.antialiasing);
    options->setViewDistanceScale(m_pending.viewDistanceScale);
}

// (34 B). Retail answers the renderer's autodetect (RendDX9 0x1000F6E0: CPU benchmark x CPU-vendor
// multiplier, RAM, pixel-shader major and the 2005 ATI/NVIDIA device whitelist), which caps every
// GPU it does not list at Medium and a CPU it cannot identify at Low, so no current machine gets
// High. [CHOSEN, not recovered] Owner decision 2026-11-14: with a renderer the answer is the High
// scheme (2) on every platform; the renderer's heuristic still runs and is logged. Its callers are
// setDefaults (new profiles) and the options page's detectVideoScheme (reset to default).
int VideoSettings::detectVideoOptionScheme()
{
    if (!g_bf2Render)
        return 1;
    dx9rend::IRenderer* renderer = g_bf2Render->getRenderer();
    if (!renderer)
        return 1;
    const int detected = renderer->getVideoOptions()->detectQualityLevel();
    std::fprintf(stderr, "[CHOSEN, not recovered] VideoSettings::detectVideoOptionScheme: renderer "
                         "autodetect %d, answering High (2)\n", detected);
    return 2;
}

// (57 B): the argument is never read; the pending scheme is what reaches the renderer.
// [CHOSEN, not recovered] For the High scheme the highest listed antialiasing follows the preset.
void VideoSettings::applyVideoOptionScheme(int /*unused*/)
{
    if (!g_bf2Render)
        return;
    dx9rend::IRenderer* renderer = g_bf2Render->getRenderer();
    if (!renderer)
        return;
    IVideoOptions* options = renderer->getVideoOptions();
    options->applyQualityPreset(m_pending.videoOptionScheme);
    // [CHOSEN, not recovered] High (new profiles, reset to default, the High choice) adds the
    // highest listed antialiasing; see selectMaxAntialiasing.
    if (m_pending.videoOptionScheme == 2)
        selectMaxAntialiasing(options);
    readOptions(options);
}

// (11 B): 3 is "Custom" in getVideoOptionSchemeNames.
void VideoSettings::setCustomVideoOptionScheme() { m_pending.videoOptionScheme = 3; }

// (23 B)
bool VideoSettings::hasChangesInGame() { return game != nullptr && hasChanges(); }

// (283 B): no changes, no restart. An antialiasing change always needs one; a quality row
// change needs one when the renderer flags that row. The resolution and the scale never do.
bool VideoSettings::needsRestart()
{
    if (!hasChanges())
        return false;
    if (m_pending.antialiasing != m_current.antialiasing)
        return true;
    if (m_pending.terrainQuality != m_current.terrainQuality && optionRequiresRestart(0)) return true;
    if (m_pending.geometryQuality != m_current.geometryQuality && optionRequiresRestart(1)) return true;
    if (m_pending.lightingQuality != m_current.lightingQuality && optionRequiresRestart(2)) return true;
    if (m_pending.dynamicLightingQuality != m_current.dynamicLightingQuality && optionRequiresRestart(3)) return true;
    if (m_pending.dynamicShadowsQuality != m_current.dynamicShadowsQuality && optionRequiresRestart(4)) return true;
    if (m_pending.effectsQuality != m_current.effectsQuality && optionRequiresRestart(5)) return true;
    if (m_pending.textureQuality != m_current.textureQuality && optionRequiresRestart(6)) return true;
    if (m_pending.textureFilteringQuality != m_current.textureFilteringQuality && optionRequiresRestart(7)) return true;
    return false;
}

// +40..+7C: the eight pending quality rows.
int VideoSettings::getTerrainQuality() { return m_pending.terrainQuality; }
void VideoSettings::setTerrainQuality(int value) { m_pending.terrainQuality = value; }
int VideoSettings::getGeometryQuality() { return m_pending.geometryQuality; }
void VideoSettings::setGeometryQuality(int value) { m_pending.geometryQuality = value; }
int VideoSettings::getLightingQuality() { return m_pending.lightingQuality; }
void VideoSettings::setLightingQuality(int value) { m_pending.lightingQuality = value; }
int VideoSettings::getDynamicLightingQuality() { return m_pending.dynamicLightingQuality; }
void VideoSettings::setDynamicLightingQuality(int value) { m_pending.dynamicLightingQuality = value; }
int VideoSettings::getDynamicShadowsQuality() { return m_pending.dynamicShadowsQuality; }
void VideoSettings::setDynamicShadowsQuality(int value) { m_pending.dynamicShadowsQuality = value; }
int VideoSettings::getEffectsQuality() { return m_pending.effectsQuality; }
void VideoSettings::setEffectsQuality(int value) { m_pending.effectsQuality = value; }
int VideoSettings::getTextureQuality() { return m_pending.textureQuality; }
void VideoSettings::setTextureQuality(int value) { m_pending.textureQuality = value; }
int VideoSettings::getTextureFilteringQuality() { return m_pending.textureFilteringQuality; }
void VideoSettings::setTextureFilteringQuality(int value) { m_pending.textureFilteringQuality = value; }
// +80..+94: the two pending strings (copied out, assigned from the by-value argument) and the scale.
std::string VideoSettings::getResolution() { return m_pending.resolution; }
void VideoSettings::setResolution(std::string value) { m_pending.resolution = value; }
std::string VideoSettings::getAntialiasing() { return m_pending.antialiasing; }
void VideoSettings::setAntialiasing(std::string value) { m_pending.antialiasing = value; }
float VideoSettings::getViewDistanceScale() { return m_pending.viewDistanceScale; }
void VideoSettings::setViewDistanceScale(float value) { m_pending.viewDistanceScale = value; }
// +98..+D4: the sixteen range values in storage order.
int VideoSettings::getTerrainQualityMin() { return m_optionRange[0].minimum; }
int VideoSettings::getTerrainQualityMax() { return m_optionRange[0].maximum; }
int VideoSettings::getGeometryQualityMin() { return m_optionRange[1].minimum; }
int VideoSettings::getGeometryQualityMax() { return m_optionRange[1].maximum; }
int VideoSettings::getLightingQualityMin() { return m_optionRange[2].minimum; }
int VideoSettings::getLightingQualityMax() { return m_optionRange[2].maximum; }
int VideoSettings::getDynamicLightingQualityMin() { return m_optionRange[3].minimum; }
int VideoSettings::getDynamicLightingQualityMax() { return m_optionRange[3].maximum; }
int VideoSettings::getDynamicShadowsQualityMin() { return m_optionRange[4].minimum; }
int VideoSettings::getDynamicShadowsQualityMax() { return m_optionRange[4].maximum; }
int VideoSettings::getEffectsQualityMin() { return m_optionRange[5].minimum; }
int VideoSettings::getEffectsQualityMax() { return m_optionRange[5].maximum; }
int VideoSettings::getTextureQualityMin() { return m_optionRange[6].minimum; }
int VideoSettings::getTextureQualityMax() { return m_optionRange[6].maximum; }
int VideoSettings::getTextureFilteringQualityMin() { return m_optionRange[7].minimum; }
int VideoSettings::getTextureFilteringQualityMax() { return m_optionRange[7].maximum; }
// +D8/+DC
int VideoSettings::getVideoOptionScheme() { return m_pending.videoOptionScheme; }
void VideoSettings::setVideoOptionScheme(int value) { m_pending.videoOptionScheme = value; }

// (38 B)
bool VideoSettings::optionRequiresRestart(int index)
{
    if (!g_bf2Render)
        return false;
    dx9rend::IRenderer* renderer = g_bf2Render->getRenderer();
    if (!renderer)
        return false;
    return renderer->getVideoOptions()->getRestartFlag(index);
}

// (38 B)
int VideoSettings::getOptionMaxQuality(int index)
{
    if (!g_bf2Render)
        return 0;
    dx9rend::IRenderer* renderer = g_bf2Render->getRenderer();
    if (!renderer)
        return 0;
    return renderer->getVideoOptions()->getMaxQuality(index);
}

// / 4B28E0 (44 B each): the renderer's list is copied into the result; neither body tests
// g_bf2Render or the renderer.
std::vector<std::string> VideoSettings::getResolutionList()
{
    return std::vector<std::string>(g_bf2Render->getRenderer()->getVideoOptions()->getDynamicOptionList(1));
}
std::vector<std::string> VideoSettings::getAntialiasingList()
{
    return std::vector<std::string>(g_bf2Render->getRenderer()->getVideoOptions()->getDynamicOptionList(0));
}

// (224 B): five level names pushed from temporaries, then copied out.
std::vector<std::string> VideoSettings::getQualityLevelNames()
{
    std::vector<std::string> names;
    names.push_back(std::string("Off"));
    names.push_back(std::string("Low"));
    names.push_back(std::string("Medium"));
    names.push_back(std::string("High"));
    names.push_back(std::string("UltraHigh"));
    return names;
}

// (189 B): the four scheme names; index 3 is the one setCustomVideoOptionScheme selects.
std::vector<std::string> VideoSettings::getVideoOptionSchemeNames()
{
    std::vector<std::string> names;
    names.push_back(std::string("Low"));
    names.push_back(std::string("Medium"));
    names.push_back(std::string("High"));
    names.push_back(std::string("Custom"));
    return names;
}

// (217 B): the argument is never read; the options are fetched again behind both null tests.
// Pending takes the eight rows, the selected resolution (1) and antialiasing (0) strings and the
// renderer's view-distance scale.
void VideoSettings::readOptions(IVideoOptions* /*unused*/)
{
    if (!g_bf2Render)
        return;
    dx9rend::IRenderer* renderer = g_bf2Render->getRenderer();
    if (!renderer)
        return;
    IVideoOptions* options = renderer->getVideoOptions();
    m_pending.terrainQuality = options->getQuality(0);
    m_pending.geometryQuality = options->getQuality(1);
    m_pending.lightingQuality = options->getQuality(2);
    m_pending.dynamicLightingQuality = options->getQuality(3);
    m_pending.dynamicShadowsQuality = options->getQuality(4);
    m_pending.effectsQuality = options->getQuality(5);
    m_pending.textureQuality = options->getQuality(6);
    m_pending.textureFilteringQuality = options->getQuality(7);
    m_pending.resolution = options->getDynamicOption(1);
    m_pending.antialiasing = options->getDynamicOption(0);
    m_pending.viewDistanceScale = options->getViewDistanceScale();
}

// (31 B)
bool VideoSettings::optionsSlot0x38()
{
    if (!g_bf2Render)
        return true;
    dx9rend::IRenderer* renderer = g_bf2Render->getRenderer();
    if (!renderer)
        return true;
    return renderer->getVideoOptions()->getPerGpuFlag();
}

// (63 B)
bool VideoSettings::applyToRenderer(const VideoSettingsData& /*record, not read*/)
{
    IVideoOptions* options = g_bf2Render->getRenderer()->getVideoOptions();
    if (!options)
        return false;
    applyToOptions(options);
    options->apply();
    readOptions(options);
    return true;
}

} }
#endif
