// SwiffPlayer image interface, object state, and embedded gameswf renderer adapter.
// Preserve interface slot order across the host DLL boundary.

#ifndef BF2_SWIFFPLAYER_SWIFFPLAYER_H
#define BF2_SWIFFPLAYER_SWIFFPLAYER_H

#ifdef BF2_IMAGE_SWIFFPLAYER

// gameswf_types.h defines gameswf::rgba (passed BY value in several render_handler methods, so the
// overrides' definitions need the complete type) and transitively includes gameswf.h, which brings
// gameswf::render_handler - the +4 subobject's base. matrix/cxform/rect/bitmap_info/image::* are used
// only by pointer or const-ref, so their incomplete forward decls from gameswf.h suffice.
#include "gameswf/gameswf_types.h"
#include "../../../../RendDX9Abi/HfeAbi.h"  // dice::hfe::IBase
#include "../../../../RendDX9Abi/Frontier.h"

#include <vector>
#include <stdint.h>

namespace dice {
namespace hfe {
namespace ui {


// Ids, read from the shipped module and factory.
//   CID_Swiff       870002  - class "dice.hfe.ui.Swiff", published by SwiffModule::getClassList
//   IID_ISwiffPlayer 870001 - the singleton interface id; QueryInterface answers 1 and 870001

enum { CID_Swiff = 870002u, IID_ISwiffPlayer = 870001u };

// The frontier macro for this image's wrapper code. Uses the shared kit (Frontier.cpp is compiled
// per-image with SwiffPlayer's log name/env). Same shape as RD9_FRONTIER; renamed so a log line
// names the SwiffPlayer front.
#define SWF_FRONTIER(qualifiedName, slot) \
    ::renddx9abi::frontier((qualifiedName), this, (slot))
#define SWF_FRONTIER_AT(qualifiedName, self, slot) \
    ::renddx9abi::frontier((qualifiedName), (self), (slot))

// Interfaces supplied by the rebuilt renderer and client. Their pointers stay opaque in this
// layout declaration; SwiffPlayer.cpp provides dispatch and the remaining named frontiers.
struct ISwiffRender;
struct ISwiffHost;


// Embedded renderer adapter forwarding gameswf callbacks through SwiffRender's interface.
// Callback mapping accounts for the differing gameswf method sets.

struct SwiffRenderHandler : public gameswf::render_handler
{
    SwiffRenderHandler() {}

    // NO data members - the subobject is only its vtable ptr, as in the shipped layout. The handler
    // is always SwiffPlayer::render_handler (retail: this subobject at object+4, render_context at
    // object+12), so a forwarder reaches render_context through its owner, recovered from this
    // member's offset by renderContext(). No back-pointer member is stored.

    // The owning SwiffPlayer's render_context.
    ISwiffRender* renderContext();

    virtual gameswf::bitmap_info* create_bitmap_info_empty();
    virtual gameswf::bitmap_info* create_bitmap_info_alpha(int w, int h, unsigned char* data);
    virtual gameswf::bitmap_info* create_bitmap_info_rgb(image::rgb* im);
    virtual gameswf::bitmap_info* create_bitmap_info_rgba(image::rgba* im);
    // [DICE-MOD] the 2004 slot-5 by-name loader, restored on the 2006 base: -> ISwiffRender::loadTexture
    virtual gameswf::bitmap_info* create_bitmap_info_file(const char* path);
    virtual void delete_bitmap_info(gameswf::bitmap_info* bi);
    virtual void begin_display(gameswf::rgba background_color,
                               int viewport_x0, int viewport_y0,
                               int viewport_width, int viewport_height,
                               float x0, float x1, float y0, float y1);
    virtual void end_display();
    virtual void set_matrix(const gameswf::matrix& m);
    virtual void set_cxform(const gameswf::cxform& cx);
    virtual void draw_mesh_strip(const void* coords, int vertex_count);
    virtual void draw_line_strip(const void* coords, int vertex_count);
    virtual void fill_style_disable(int fill_side);
    virtual void fill_style_color(int fill_side, gameswf::rgba color);
    virtual void fill_style_bitmap(int fill_side, const gameswf::bitmap_info* bi,
                                   const gameswf::matrix& m, bitmap_wrap_mode wm);
    virtual void line_style_disable();
    virtual void line_style_color(gameswf::rgba color);
    virtual void line_style_width(float width);
    virtual void draw_bitmap(const gameswf::matrix& m, const gameswf::bitmap_info* bi,
                             const gameswf::rect& coords, const gameswf::rect& uv_coords,
                             gameswf::rgba color);
    virtual void set_antialiased(bool enable);
    virtual void begin_submit_mask();
    virtual void end_submit_mask();
    virtual void disable_mask();

    // [dice-MOD SwiffPlayer #36: the twelve text slots dice appended to render_handler (shipped slots
    // 21-32, the forwarders 0x10005DC0..0x10005EC0), each a thin call into render_context at the
    // ISwiffRender offset the shipped forwarder jumps to (+0x68..+0x94, our SwiffRender slots 26-37:
    // a "font" wrapper there IS one text line here). Declared after the stock members: this vtable is
    // internal to the DLL, nothing dispatches into render_handler by raw offset.]
    virtual void* text_create(const char* fontName, int heightPx);                          // 21 -> +0x68
    virtual void  text_release(void* line);                                                 // 22 -> +0x6C
    virtual void  text_set(void* line, const wchar_t* text);                                // 23 -> +0x70
    virtual void  text_detach(void* line);                                                  // 24 -> +0x74
    virtual void  text_draw(void* line, const gameswf::matrix* m, unsigned color, int flags); // 25 -> +0x78
    virtual bool  text_extent(void* line, int count, float* w, float* h);                   // 26 -> +0x7C
    virtual bool  text_hit(void* line, int* outPos, int x, int y);                          // 27 -> +0x80
    virtual bool  text_selection(void* line, int* a, int* b);                               // 28 -> +0x84 (AL = has selection)
    virtual bool  text_set_selection(void* line, int a, int b, int c, int d);               // 29 -> +0x88
    virtual void  text_blur(void* line);                                                    // 30 -> +0x8C
    virtual void  text_set_size(void* line, float size, int flag);                          // 31 -> +0x90
    virtual void  text_caret(void* line, int pos);                                          // 32 -> +0x94
};


// ISwiffPlayer - 37 pure virtuals in the shipped vtable order (0x100D7CC0). The IBase quartet is the
// first four; QueryInterface answers iids 1 (IBase) and 870001. This order is a frozen ABI surface -
// BF2.exe dispatches into it by raw byte offset (initMainMenuSwiff), so the declaration order here
// must never change.

class ISwiffPlayer : public IBase
{
public:
    // 4  +0x10
    virtual void  setScale(float xScale, float yScale) = 0;
    // 5  +0x14
    virtual bool  loadMovie(const char* movieName) = 0;
    // 6  +0x18
    virtual void  update(float deltaTime) = 0;
    // 7  +0x1C
    virtual void  display() = 0;
    // 8  +0x20
    virtual bool  isMovieLoaded() = 0;
    // 9  +0x24
    virtual void  setRender(ISwiffRender* render) = 0;
    // 10 +0x28
    virtual ISwiffRender* getRender() = 0;
    // 11 +0x2C
    virtual void  setMousePositionDeferred(int x, int y) = 0;
    // 12 +0x30
    virtual void  queueMouseState(int x, int y, int buttons) = 0;
    // 13 +0x34
    virtual void  notifyKey(int code, int down) = 0;
    // 14 +0x38  reserved (shipped nullsub_20, one int arg, empty)
    virtual void  reserved14(int a) = 0;
    // 15 +0x3C  reserved (shipped nullsub_21, one int arg, empty)
    virtual void  reserved15(int a) = 0;
    // 16 +0x40
    virtual void  notifyChar(int character) = 0;
    // 17 +0x44
    virtual void  notifyKeyState(int code, int down) = 0;
    // 18 +0x48
    virtual void  resetModifierState() = 0;
    // 19 +0x4C
    virtual void  setTextInput(const char* text) = 0;
    // 20 +0x50 writes the current text-input into a preconstructed host string.
    // The caller supplies a live native HostString using the shared CRT and release STL layout.
    virtual void  getTextInput0(void* outStdString) = 0;
    // 21 +0x54
    virtual void  getTextInput1(void* outStdString) = 0;
    // 22 +0x58
    virtual void  setSwiffHost(ISwiffHost* host) = 0;
    // 23 +0x5C
    virtual ISwiffHost* getSwiffHost() = 0;
    // 24 +0x60
    virtual char  renderCapability0(intptr_t a) = 0;
    // 25 +0x64
    virtual char  renderCapability1(intptr_t a) = 0;
    // 26 +0x68
    virtual int   getMovieWidth() = 0;
    // 27 +0x6C
    virtual int   getMovieHeight() = 0;
    // 28 +0x70
    virtual void  renderSetPairState(int a, int b) = 0;
    // 29 +0x74
    virtual void  legacyModeOff() = 0;
    // 30 +0x78
    virtual void  legacyModeOn() = 0;
    // 31 +0x7C
    virtual char  getModeFlag() = 0;
    // 32 +0x80
    virtual void  setModeFlag(char v) = 0;
    // 33 +0x84
    virtual void  notifyMouse(int x, int y, int buttons) = 0;
    // 34 +0x88
    virtual void  processQueuedMouseState() = 0;
    // 35 +0x8C
    virtual void* getBf2VoipClient() = 0;
    // 36 +0x90
    virtual void  setVoipUpdateFlag(char v) = 0;

protected:
    ~ISwiffPlayer() {}
};


// SwiffPlayer - member order matches the IDB local type dice_hfe_SwiffPlayer_BF2 (the retail object
// is 1120 bytes; the +N comments are its offsets). The queued-mouse storage is a std::vector
// (https://github.com/B2BF/OpenBF2/issues/7), so the native object's size differs from retail. The
// ISwiffPlayer base supplies the +0 vtable; the SwiffRenderHandler member supplies the +4 one.

class SwiffPlayer : public ISwiffPlayer
{
public:
    SwiffPlayer();                 // SwiffPlayer_ctor 0x10006EF0
    ~SwiffPlayer();                // SwiffPlayer_dtor 0x10006FD0 (non-virtual: see release())

    // IBase (slots 0-3)
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    // ISwiffPlayer (slots 4-36)
    virtual void  setScale(float xScale, float yScale);
    virtual bool  loadMovie(const char* movieName);
    virtual void  update(float deltaTime);
    virtual void  display();
    virtual bool  isMovieLoaded();
    virtual void  setRender(ISwiffRender* render);
    virtual ISwiffRender* getRender();
    virtual void  setMousePositionDeferred(int x, int y);
    virtual void  queueMouseState(int x, int y, int buttons);
    virtual void  notifyKey(int code, int down);
    virtual void  reserved14(int a);
    virtual void  reserved15(int a);
    virtual void  notifyChar(int character);
    virtual void  notifyKeyState(int code, int down);
    virtual void  resetModifierState();
    virtual void  setTextInput(const char* text);
    virtual void  getTextInput0(void* outStdString);
    virtual void  getTextInput1(void* outStdString);
    virtual void  setSwiffHost(ISwiffHost* host);
    virtual ISwiffHost* getSwiffHost();
    virtual char  renderCapability0(intptr_t a);
    virtual char  renderCapability1(intptr_t a);
    virtual int   getMovieWidth();
    virtual int   getMovieHeight();
    virtual void  renderSetPairState(int a, int b);
    virtual void  legacyModeOff();
    virtual void  legacyModeOn();
    virtual char  getModeFlag();
    virtual void  setModeFlag(char v);
    virtual void  notifyMouse(int x, int y, int buttons);
    virtual void  processQueuedMouseState();
    virtual void* getBf2VoipClient();
    virtual void  setVoipUpdateFlag(char v);

private:
    // A queued mouse sample (12 bytes), the element of the queued-mouse vector.
    struct MouseState { int x; int y; int buttons; };

    void initKeyTranslation();     // SwiffPlayer_initKeyTranslation 0x10005410

    // +4 - the embedded render handler. gameswf holds a pointer to it via set_render_handler.
    SwiffRenderHandler render_handler;
    // +8
    int           ref_count;
    // +12
    ISwiffRender* render_context;
    // +16 / +20 / +24 - opaque gameswf pointers + the BF2.exe VoIP client.
    void*         movie_definition;
    void*         movie_instance;
    void*         bf2_voip_client;
    // +28 / +32
    int           movie_width;
    int           movie_height;
    // +36 / +40 / +44
    int           mouse_x;
    int           mouse_y;
    int           mouse_buttons;
    // +48..+54 - modifier-key edge state; +54 unknown_36
    unsigned char left_shift_down;
    unsigned char right_shift_down;
    unsigned char left_alt_down;
    unsigned char right_alt_down;
    unsigned char left_ctrl_down;
    unsigned char right_ctrl_down;
    unsigned char unknown_36;
    // +55
    unsigned char voip_update_flag;
    // +56 / +60 / +64
    int           deferred_mouse_dirty;
    int           deferred_mouse_x;
    int           deferred_mouse_y;
    // +68 (+3 pad)
    unsigned char mode_flag;
    unsigned char pad_45[3];
    // +72
    ISwiffHost*   swiff_host;
    // +76..+1095 - the scancode->vkey table (int[255]).
    int           key_translation[255];
    // +1096
    float         elapsed_time;
    // +1100 (+3 pad)
    unsigned char legacy_flag_44c;
    unsigned char pad_44d[3];
    // +1104 - the queued mouse samples (retail: a 16-byte VC7.1 vector).
    std::vector<MouseState> queued_mouse;

    friend struct SwiffRenderHandler;   // renderContext() reaches render_context
    friend IBase* __fastcall RefFactory_ui_SwiffPlayer(unsigned int iid, IBase* parent);
};

// The factory retains its recovered __fastcall convention. Module registration wraps it with
// RD9_CLASS_FACTORY to provide the native host's cdecl ClassRegInfo::create callback.
IBase* __fastcall RefFactory_ui_SwiffPlayer(unsigned int iid, IBase* parent);

} // namespace ui
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_SWIFFPLAYER
#endif // BF2_SWIFFPLAYER_SWIFFPLAYER_H
