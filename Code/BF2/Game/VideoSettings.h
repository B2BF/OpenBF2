// Keeps current and pending video settings. Accessors edit pending values; scripts serialize current values.
#ifndef GAME_VIDEOSETTINGS_H
#define GAME_VIDEOSETTINGS_H
#include "../Core/IBase.h"
#include <iosfwd>
#include <string>
#include <vector>
#if !DEDICATED
namespace dice { namespace hfe {
class IVideoOptions;

// Video settings record; the eight quality fields correspond to renderer quality indices 0–7.
struct VideoSettingsData {
    VideoSettingsData();                                     //: rows 2, "800x600@60Hz", "Off", 1.0f, scheme 1
    VideoSettingsData& operator=(const VideoSettingsData&);  // with the callers' self-assignment test
    bool differsFrom(const VideoSettingsData&) const;        //: rows, resolution, scale, scheme, antialiasing
    void write(std::ostream&) const;                         //: the twelve script rows
    int terrainQuality;                                      // retail 00; index 0
    int geometryQuality;                                     //04; index 1
    int lightingQuality;                                     //08; index 2
    int dynamicLightingQuality;                              //0C; index 3
    int dynamicShadowsQuality;                               //10; index 4
    int effectsQuality;                                      //14; index 5
    int textureQuality;                                      //18; index 6
    int textureFilteringQuality;                             //1C; index 7
    std::string resolution;                                  //20; dynamic option 1
    std::string antialiasing;                                //3C; dynamic option 0
    float viewDistanceScale;                                 //58
    int videoOptionScheme;                                   //5C; 1 Medium by default, 3 Custom
};

// Keep the virtual destructor between its existing neighboring slots.
// String getters return copies and setters take values; non-accessor names are [INFERRED].
class IVideoSettings : public IBase {
public:
    virtual std::string getFileName() = 0;                   //+10 4B1D70 "Video.con"
    virtual std::string getSettingsScript() = 0;             //+14 4B25A0: writes the current record
    virtual bool hasChanges() = 0;                           //+18 4B25F0: pending differs from current
    virtual bool apply() = 0;                                //+1C 4B2810
    virtual void setDefaults() = 0;                          //+20 4B2600; LocalProfileManager::createDefaultProfile 4A8113
    virtual void revertChanges() = 0;                        //+24 4B2890: pending takes current
    virtual void applyToOptions(IVideoOptions* options) = 0; //+28 4B2040: pushes pending into the renderer's options unless a game is loaded
    virtual int detectVideoOptionScheme() = 0;               //+2C 4B1DB0: the renderer's detected level ([CHOSEN] High); 1 without a renderer
    virtual void applyVideoOptionScheme(int unused) = 0;     //+30 4B1F20: presets the renderer with the pending scheme, then reads back; the argument is not read
    virtual void setCustomVideoOptionScheme() = 0;           //+34 4B2160: pending scheme 3
    virtual bool hasChangesInGame() = 0;                     //+38 4B1DE0: game && hasChanges
    virtual bool needsRestart() = 0;                         //+3C 4B1E00: antialiasing change, or a flagged quality row change
    virtual int getTerrainQuality() = 0;                     //+40 pending+00
    virtual void setTerrainQuality(int) = 0;                 //+44
    virtual int getGeometryQuality() = 0;                    //+48 pending+04
    virtual void setGeometryQuality(int) = 0;                //+4C
    virtual int getLightingQuality() = 0;                    //+50 pending+08
    virtual void setLightingQuality(int) = 0;                //+54
    virtual int getDynamicLightingQuality() = 0;             //+58 pending+0C
    virtual void setDynamicLightingQuality(int) = 0;         //+5C
    virtual int getDynamicShadowsQuality() = 0;              //+60 pending+10
    virtual void setDynamicShadowsQuality(int) = 0;          //+64
    virtual int getEffectsQuality() = 0;                     //+68 pending+14
    virtual void setEffectsQuality(int) = 0;                 //+6C
    virtual int getTextureQuality() = 0;                     //+70 pending+18
    virtual void setTextureQuality(int) = 0;                 //+74
    virtual int getTextureFilteringQuality() = 0;            //+78 pending+1C
    virtual void setTextureFilteringQuality(int) = 0;        //+7C
    virtual std::string getResolution() = 0;                 //+80 pending+20
    virtual void setResolution(std::string) = 0;             //+84
    virtual std::string getAntialiasing() = 0;               //+88 pending+3C
    virtual void setAntialiasing(std::string) = 0;           //+8C
    virtual float getViewDistanceScale() = 0;                //+90 pending+58
    virtual void setViewDistanceScale(float) = 0;            //+94
    // The eight (min, max) pairs setDefaults copies from the renderer's rows +08/+0C; [INFERRED]
    // roles: +08 is the DLL's per-setting floor, +0C its row D (all 3, unresolved there).
    virtual int getTerrainQualityMin() = 0;                  //+98 +08
    virtual int getTerrainQualityMax() = 0;                  //+9C +0C
    virtual int getGeometryQualityMin() = 0;                 //+A0 +10
    virtual int getGeometryQualityMax() = 0;                 //+A4 +14
    virtual int getLightingQualityMin() = 0;                 //+A8 +18
    virtual int getLightingQualityMax() = 0;                 //+AC +1C
    virtual int getDynamicLightingQualityMin() = 0;          //+B0 +20
    virtual int getDynamicLightingQualityMax() = 0;          //+B4 +24
    virtual int getDynamicShadowsQualityMin() = 0;           //+B8 +28
    virtual int getDynamicShadowsQualityMax() = 0;           //+BC +2C
    virtual int getEffectsQualityMin() = 0;                  //+C0 +30
    virtual int getEffectsQualityMax() = 0;                  //+C4 +34
    virtual int getTextureQualityMin() = 0;                  //+C8 +38
    virtual int getTextureQualityMax() = 0;                  //+CC +3C
    virtual int getTextureFilteringQualityMin() = 0;         //+D0 +40
    virtual int getTextureFilteringQualityMax() = 0;         //+D4 +44
    virtual int getVideoOptionScheme() = 0;                  //+D8 pending+5C
    virtual void setVideoOptionScheme(int) = 0;              //+DC
    virtual bool optionRequiresRestart(int index) = 0;       //+E0 4B2170: the renderer's restart flag; false without a renderer
    virtual int getOptionMaxQuality(int index) = 0;          //+E4 4B21A0: the renderer's capped ceiling; 0 without a renderer
    virtual std::vector<std::string> getResolutionList() = 0;        //+E8 4B28B0: copy of dynamic-option list 1
    virtual std::vector<std::string> getAntialiasingList() = 0;      //+EC 4B28E0: copy of dynamic-option list 0
    virtual std::vector<std::string> getQualityLevelNames() = 0;     //+F0 4B2910: Off, Low, Medium, High, UltraHigh
    virtual std::vector<std::string> getVideoOptionSchemeNames() = 0; //+F4 4B29F0: Low, Medium, High, Custom
    virtual ~IVideoSettings() {}                             //+F8 4B2550: the deleting destructor's position
    virtual void readOptions(IVideoOptions* unused) = 0;     //+FC 4B1F60: pending takes the renderer's values; the argument is not read
    virtual bool optionsSlot0x38() = 0;                      //+100 4B1D90: the renderer's per-GPU byte; true without a renderer
};

struct VideoSettingsFixture;
class VideoSettings final : public IVideoSettings {
public:
    VideoSettings();                                         // retail 268 bytes
    ~VideoSettings() override;                               // body
    static IBase* create(unsigned int iid, IBase* outer);
    void addRef() override;                                  // shared
    int getRef() override;                                   // shared
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    std::string getFileName() override;
    std::string getSettingsScript() override;
    bool hasChanges() override;
    bool apply() override;
    void setDefaults() override;
    void revertChanges() override;
    void applyToOptions(IVideoOptions* options) override;
    int detectVideoOptionScheme() override;
    void applyVideoOptionScheme(int unused) override;
    void setCustomVideoOptionScheme() override;
    bool hasChangesInGame() override;
    bool needsRestart() override;
    int getTerrainQuality() override;                  void setTerrainQuality(int value) override;
    int getGeometryQuality() override;                 void setGeometryQuality(int value) override;
    int getLightingQuality() override;                 void setLightingQuality(int value) override;
    int getDynamicLightingQuality() override;          void setDynamicLightingQuality(int value) override;
    int getDynamicShadowsQuality() override;           void setDynamicShadowsQuality(int value) override;
    int getEffectsQuality() override;                  void setEffectsQuality(int value) override;
    int getTextureQuality() override;                  void setTextureQuality(int value) override;
    int getTextureFilteringQuality() override;         void setTextureFilteringQuality(int value) override;
    std::string getResolution() override;              void setResolution(std::string value) override;
    std::string getAntialiasing() override;            void setAntialiasing(std::string value) override;
    float getViewDistanceScale() override;             void setViewDistanceScale(float value) override;
    int getTerrainQualityMin() override;               int getTerrainQualityMax() override;
    int getGeometryQualityMin() override;              int getGeometryQualityMax() override;
    int getLightingQualityMin() override;              int getLightingQualityMax() override;
    int getDynamicLightingQualityMin() override;       int getDynamicLightingQualityMax() override;
    int getDynamicShadowsQualityMin() override;        int getDynamicShadowsQualityMax() override;
    int getEffectsQualityMin() override;               int getEffectsQualityMax() override;
    int getTextureQualityMin() override;               int getTextureQualityMax() override;
    int getTextureFilteringQualityMin() override;      int getTextureFilteringQualityMax() override;
    int getVideoOptionScheme() override;               void setVideoOptionScheme(int value) override;
    bool optionRequiresRestart(int index) override;
    int getOptionMaxQuality(int index) override;
    std::vector<std::string> getResolutionList() override;
    std::vector<std::string> getAntialiasingList() override;
    std::vector<std::string> getQualityLevelNames() override;
    std::vector<std::string> getVideoOptionSchemeNames() override;
    void readOptions(IVideoOptions* unused) override;
    bool optionsSlot0x38() override;
private:
    friend struct VideoSettingsFixture;
    // (63 B), non-virtual: apply's arm when the renderer hands back no options. Fetches the
    // options again without null tests, returns false when they are still absent, otherwise
    // applyToOptions / options apply / readOptions and true. The record argument is not read.
    bool applyToRenderer(const VideoSettingsData& record);
    struct OptionRange { int minimum; int maximum; };
    int m_refCount;                          // retail +04
    OptionRange m_optionRange[8];            //+08..+44: interleaved renderer rows +08/+0C per quality index
    VideoSettingsData m_current;             //+48: the applied record; getSettingsScript writes it
    VideoSettingsData m_pending;             //+A8: the edited record
    bool m_restartRequired;                  //+108: apply sets it when needsRestart()
};

} }
#endif
#endif
