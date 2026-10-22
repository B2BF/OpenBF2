// The editable-road draw queue (IID 820020, class 821913, the "RoadEditableRenderer" singleton).
// [CHOSEN, not recovered] shared header usable by the renderer and editor images; the slot order is
// the 1.50 table's. The queue holds no references: a queued road removes itself when destroyed.
#ifndef BF2_GEOM_IROADEDITABLERENDERER_H
#define BF2_GEOM_IROADEDITABLERENDERER_H

#include "../Core/IBase.h"

namespace dice {
namespace hfe {
namespace geom {

class IRoadEditable;

class IRoadEditableRenderer : public IBase
{
public:
    virtual void add(IRoadEditable* road) = 0;                                  // +0x10 no duplicates
    virtual void remove(IRoadEditable* road) = 0;                               // +0x14
    // Every queued road, lowest template priority first, then lowest road priority within a template.
    virtual void drawAll() = 0;                                                 // +0x18
    virtual void setFlag08(bool value) = 0;                                     // +0x1C [UNRESOLVED] role
    virtual bool getFlag08() = 0;                                               // +0x20
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_GEOM_IROADEDITABLERENDERER_H
