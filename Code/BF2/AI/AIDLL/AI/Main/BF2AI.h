// AI library lifecycle implementation used by the engine-facing IAI interface.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_BF2AI_H        // [INFERRED] guard style
#define AI_AIDLL_AI_MAIN_BF2AI_H

namespace dice {
namespace hfe {
namespace ai {

// Engine-facing AI interface. Construction and destruction manage the simple-singleton
// registration; preserve its virtual method order.
class IAI
{
public:
    IAI();
    virtual ~IAI();

    virtual bool init()                             = 0;    // +0x08
    virtual bool clear()                            = 0;    // +0x0C
    virtual bool shutdown()                         = 0;    // +0x10
    virtual int  getSettingsFromMenu(bool fromMenu) = 0;     // +0x14
    virtual int  update(double dt)                  = 0;     // +0x18
    virtual bool isInitialized() const              = 0;    // +0x1C
    virtual bool isShuttingDown() const             = 0;    // +0x20
};

// Coordinates AI singleton initialization, cleanup, and ticks. Stores only status flags
// in addition to the interface vptr; subsystem pointers live in their respective registries.
class BF2AI : public IAI
{
public:
    BF2AI();
    virtual ~BF2AI();

    virtual bool init();
    virtual bool clear();
    virtual bool shutdown();
    virtual int  getSettingsFromMenu(bool fromMenu);
    virtual int  update(double dt);
    virtual bool isInitialized() const;
    virtual bool isShuttingDown() const;

private:
    /*  0 */ // vptr
    // Lifecycle flags returned by isInitialized and isShuttingDown.
    /*  4 */ bool m_initialized;
    // Also acts as the initialization busy flag. init clears it on success but leaves it
    // set when called on an already initialized instance.
    /*  5 */ bool m_shuttingDown;
    /*  6 */ // two bytes of tail padding to the 8 the allocation proves
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_BF2AI_H
