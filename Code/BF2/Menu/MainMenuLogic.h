// Main front-end menu logic.
#ifndef MENU_MAINMENULOGIC_H
#define MENU_MAINMENULOGIC_H
#include "IMenuLogic.h"
#include <ctime>
#include <string>
#if !DEDICATED
namespace dice { namespace hfe {

class MainMenuLogic : public IMenuLogic
{
public:
    MainMenuLogic();
    static IBase* create(unsigned int iid, IBase* outer);         // 0x410286 (__fastcall)

    void  addRef() override;                                      // 0x6D0970 (the shared body)
    int   getRef() override;
    int   release() override;                                     // slot +0x5C(1) at zero
    void* queryInterface(unsigned int iid) const override;        // 0x4A4550: 1 and 121012

    unsigned int getClassId() const override;
    bool         init() override;
    void         deactivate() override;
    ui::ISwiffPlayer* getSwiffPlayer() const override;            // +0x0C
    ui::ISwiffHost*   getSwiffHost() const override;              // +0x10
    ui::ISwiffRender* getSwiffRender() const override;            // +0x14
    void         update(double wallDelta) override;
    bool         beginFrame() override;
    void         display() override;
    void         endFrame() override;
    bool         handleInput(io::GameInput& input) override;
    void         setConsole(BF2Console* console) override;
    BF2Console*  getConsole() const override;
    void         onResize(unsigned int width, unsigned int height) override;
    void         onEscape() override;
    void         onMouseMove(int x, int y) override;
    void         onMouseButtonDown() override;
    void         onMouseButtonUp() override;
    void         onMouseDoubleClick() override;
    ~MainMenuLogic() override;                                    // deactivate, then the string
    // not an override: +0x60 is MainMenuLogic's OWN slot. The other three menu logics' vtables
    // stop at the destructor (+0x5C), so this is not part of IMenuLogic - see IMenuLogic.h.
    // Declared after the destructor so it keeps its shipped position.
    virtual void nullsub60();                                     // +0x60  nullsub_2

private:
    void scaleMovie(unsigned int width, unsigned int height);
    friend struct MainMenuLogicFixture;                           // [TEMP] the fixtures' member view
    bool handleClipboardShortcut(char character);                 // Ctrl-V / Ctrl-C / Ctrl-X

    int               m_refCount;            // +0x04  1
    bool              m_swiffActive;         // +0x08  set by init; deactivate posts (10, 2) when set
    bool              m_frameActive;         // +0x09  between beginFrame and endFrame
    ui::ISwiffPlayer* m_player;              // +0x0C  the "SwiffPlayer" singleton
    ui::ISwiffHost*   m_host;                // +0x10  the "SwiffHost" singleton
    ui::ISwiffRender* m_render;              // +0x14  the "SwiffRender" singleton
    BF2Console*       m_console;             // +0x18
    std::string       m_activeControlMapName;// +0x1C  the ControlMapManager's active map when init ran
    int               m_mouseX;              // +0x38
    int               m_mouseY;              // +0x3C
    float             m_scaleX;              // +0x40  movie width / viewport width
    float             m_scaleY;              // +0x44  movie height / viewport height
    int               m_mouseButtonState;    // +0x48  0 up, 1 down, 2 double
    time_t            m_activatedAt;         // +0x4C  onEscape's three-second guard
};

} }
#endif
#endif
