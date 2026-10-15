// Host view of renderer video options. Virtual order is fixed and has no destructor slot.
// The renderer shares the host's STL types (issue #7), so strings and vectors cross directly.
// [INFERRED] Interface/member names. [UNRESOLVED] Unnamed option slots retain provisional roles.
#ifndef GAME_MAIN_IVIDEOOPTIONS_H
#define GAME_MAIN_IVIDEOOPTIONS_H
#include <string>
#include <vector>
#if !DEDICATED
namespace dice { namespace hfe {

class IVideoOptions {
public:
    virtual bool setQuality(int index, int value) = 0;                          //+00 1000F620: clamps into [row C, maxQuality]
    virtual int getQuality(int index) = 0;
    virtual int getMinQuality(int index) = 0;                                   //+08 100112C0: row C, the per-setting floor
    virtual int getMaxQualityLevel(int index) = 0;                              //+0C 100112D0: row D (all 3) [UNRESOLVED] role
    virtual int getMaxQuality(int index) = 0;                                   //+10 100112E0: the hardware-capped ceiling
    virtual bool getRestartFlag(int index) = 0;                                 //+14 10011300: byte row; VideoSettings::needsRestart reads it
    virtual const std::vector<std::string>& getDynamicOptionList(int index) = 0; //+18 10011320: 0 antialiasing, 1 resolution
    virtual bool setDynamicOption(int index, const std::string& value) = 0;     //+1C 1000FB70
    virtual const std::string& getDynamicOption(int index) = 0;                 //+20 10011350: the selected string
    virtual void setViewDistanceScale(float value) = 0;                         //+24 1000ED70: clamped to [0.5, 1.0]; VideoSettings passes viewDistanceScale
    virtual float getViewDistanceScale() = 0;                                   //+28 10011340: x87 return, stored with fstp dword
    virtual void apply() = 0;                                                   //+2C 10010500
    virtual int applyQualityPreset(int scheme) = 0;                             //+30 1000F390: VideoSettings discards the result
    virtual int detectQualityLevel() = 0;
    virtual bool getPerGpuFlag() = 0;                                           //+38 1000F360: whitelist byte [UNRESOLVED] role
};

} }
#endif
#endif
