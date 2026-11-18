// Menu logic for the between-rounds screen.
#ifndef MENU_BETWEENROUNDSLOGIC_H
#define MENU_BETWEENROUNDSLOGIC_H
#include "IMenuLogic.h"
#include <string>
#if !DEDICATED
namespace dice { namespace hfe {

class BetweenRoundsLogic : public IMenuLogic
{
public:
    BetweenRoundsLogic();
    static IBase* create(unsigned int iid, IBase* outer);         // 0x4102C5 (__fastcall)

    // IBase - all four are the same folded bodies MainMenuLogic uses.
    void  addRef() override;                                      // 0x6D0970 (the shared body)
    int   getRef() override;
    int   release() override;                                     // slot +0x5C(1) at zero
    void* queryInterface(unsigned int iid) const override;        // shared with the other
                                                                  //   menu logics - 1 and 121012

    unsigned int getClassId() const override;                     // `return 121014`
    bool         init() override;                                 // loads endOfRound.swf
    void         deactivate() override;
    ui::ISwiffPlayer* getSwiffPlayer() const override;            // +0x1C  0x68AD50 (shared)
    ui::ISwiffHost*   getSwiffHost() const override;              // +0x20  0x68ABC0 (shared)
    ui::ISwiffRender* getSwiffRender() const override;            // +0x24  0x67D080 (shared)
    void         update(double wallDelta) override;
    bool         beginFrame() override;
    void         display() override;                              // 0x4A2C50 (shared)
    void         endFrame() override;                             // 0x4A4530 (shared)
    bool         handleInput(io::GameInput& input) override;
    void         setConsole(BF2Console* console) override;        // 0x4A2C60 (shared)
    BF2Console*  getConsole() const override;                     // 0x80D430 - `return 0`, see .cpp
    void         onResize(unsigned int width, unsigned int height) override;
    void         onEscape() override;
    void         onMouseMove(int x, int y) override;
    void         onMouseButtonDown() override;
    void         onMouseButtonUp() override;
    void         onMouseDoubleClick() override;
    ~BetweenRoundsLogic() override;

private:
    friend struct BetweenRoundsLogicFixture;                      // [TEMP] the fixtures' member view

    int               m_refCount;             // +0x04  1
    bool              m_swiffActive;          // +0x08  set by init; deactivate posts (10, 2) when set
    bool              m_frameActive;          // +0x09  set by beginFrame, cleared by endFrame
    ui::ISwiffPlayer* m_player;               // +0x0C  the "SwiffPlayer" singleton
    ui::ISwiffHost*   m_host;                 // +0x10  the "SwiffHost" singleton
    ui::ISwiffRender* m_render;               // +0x14  the "SwiffRender" singleton
    BF2Console*       m_console;              // +0x18  stored by setConsole, never handed back
    std::string       m_activeControlMapName; // +0x1C  the ControlMapManager's active map when init ran
    bool              m_mouseDown;            // +0x38  byte: 1 between button-down and button-up
    int               m_mouseX;               // +0x3C
    int               m_mouseY;               // +0x40
    bool              m_escapeHandled;        // +0x44  byte: onEscape's one-shot latch, never cleared
};

// Translates a DirectInput scan code to a character. [INFERRED] Declaration and source placement.
int menuScanCodeToChar(unsigned int scanCode, unsigned short* character);

} }
#endif
#endif
