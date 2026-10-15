// Client menu system. Member and method names are [INFERRED].
#ifndef BFMEME_SYSTEM_H
#define BFMEME_SYSTEM_H

#include "../../Core/BuildConfig.h"
#if !DEDICATED
#include "Object.h"
#include "OStream.h"
#include <cstdint>

namespace dice { namespace meme {
class TypeDescriptor;
class Data;
class NameTreeNode;
class IStream;
class EventQueue;
class Node;
class Pointer;
struct GlyphMetrics;
struct Quad;
struct Rect;
struct NodeState;

class System : public Object
{
    friend class Pointer;
    friend bool writeObject(System*, Object*&, OStream*);
    friend bool saveNamedObject(System*, Object*&);
public:
    System(float width, float height);
    ~System() override;
    TypeDescriptor* getType() override;            // 0x82A9B0, shared with getClassType
    static TypeDescriptor* getClassType();         // 0x82A9B0, not Object's descriptor
    unsigned int getObjectSize() override;
    bool visit0x3c(System*, Stream*) override;
    virtual bool init();                           // +40 82A5C0; collaborators still frontier
    virtual bool close();
    virtual void setSize(float width, float height);
    virtual bool update(float delta);              // +4C 82A1A0
    virtual bool paint();
    virtual bool setPointerPosition(float x, float y);
    virtual OStream* getOutputStream();
    virtual IStream* createInputStream(const char*); // +5C 4553E0
    virtual OStream* createOutputStream(const char*);
    virtual void deleteInputStream(IStream*);
    virtual void deleteOutputStream(OStream*);
    // +6C: Pointer829000 passes addresses of its two float arguments. Pointer
    // versus reference source spelling and method name are [INFERRED].
    virtual bool pointerPositionChanged(float* x, float* y);
    virtual bool drawCharacter(int character, FontHandle font, Quad*);
    virtual bool getGlyphMetrics(int character, FontHandle font, GlyphMetrics*);
    // Source character spelling [INFERRED]; full dwords cross Font+10 and the
    // .dif backend deliberately keeps only the low sixteen bits of each key.
    virtual float getKerning(int previous, int current, FontHandle font);
    virtual float getFontHeight(FontHandle font);         // +7C 829A30
    virtual FontHandle loadFont(const char* name);
    virtual const char* getFontName(FontHandle font);
    virtual bool drawTexture(TextureHandle texture, Quad*);
    virtual TextureHandle loadTexture(const char* name, bool menuPath); // +8C 6212F0
    virtual void releaseTexture(TextureHandle texture);
    virtual const char* getTextureName(TextureHandle texture);
    virtual float getTextureWidth(TextureHandle texture);
    virtual float getTextureHeight(TextureHandle texture);  // +9C 829A30
    // Setter return spelling is [INFERRED]: HUD7583D5 supplies loadTexture's
    // int handle and discards EAX.5C9490 leaves the stored argument in EAX;
    // that alone does not prove a source-level return value.
    virtual void setDefaultTexture(TextureHandle texture);  // +A0 5C9490
    virtual bool playSound(int sound, float volume); // +A4 455420
    virtual int loadSound(const char* name);       // +A8 4553E0
    virtual const char* getSoundName(int sound);   // +AC 4553E0
    virtual void beginPaint();                    // +B0 83D270
    virtual void endPaint();                      // +B4 83D270
    virtual bool preUpdate(float delta);          // +B8 5A1860
    virtual bool postUpdate(float delta);         // +BC 5A1860
    // Both pointers address two floats. Source constness and pointer spelling
    // are [INFERRED]; newSize points into this System, not a temporary copy.
    virtual void sizeChanged(const float* oldSize, const float* newSize); // +C0 5A59F0
    // [INFERRED] names and unused-setter void return. Widget update/event
    // callers pass Node-derived this and compare the getter result by identity.
    virtual void setPointerOwner(Node* owner);     // +C4 6CBA10
    virtual Node* getPointerOwner();              // +C8 689460
    Data* registerData(Data*, const char* container);
    Object* registerObject(Object*, const char* container);
    Object* findNamedObject(const char* path);      // 82A300, no registry-null guard
    bool assignObjectPath(Object*, const char* path);
    IStream* openNamedInputStream(const char* path); // 82A480, recording/replay decorators
    bool loadMenu(const char* mainPath, const char* cachePath);
    // [INFERRED] names; direct LEA/MOV helpers in the client image.
    Node*& getRootNode() { return m_field018; }
    Object*& getPointerObject() { return m_field020; }  // 6F2880, LEA ECX+20 / RET
    float getWidth() { return m_width; }                // 7E38A0, FLD ECX+0C / RET
    float getHeight() { return m_height; }              // 6893E0, FLD ECX+10 / RET
    Rect* getBounds() { return reinterpret_cast<Rect*>(&m_originX); }
    NodeState* getNodeState() { return reinterpret_cast<NodeState*>(&m_field028); }
    int getLockCount() { return m_lockCount; }
    void lockPointer() { ++m_lockCount; }               // 82A3B0, no upper bound
    void unlockPointer() { --m_lockCount; }             // 82A3C0, no zero guard
    float* getPointerPosition() { return &m_pointerX; } // 682BA0; HUD752CED writes both floats
    const float* getPreviousPointerPosition() { return &m_previousPointerX; }
    // 433140 on a System: +24 is the default Style the text helper falls back
    // to when a node has none. Modelled here as the generic Object it already
    // is; name [INFERRED].
    Object* getDefaultStyleObject() { return m_field024; }
    void enqueueEvent(int type, int index, float first, float second, std::uint32_t field14);
    // All51 virtual slots and the native System descriptor/factory are recovered.

protected:
    float m_originX, m_originY;                    // retail +04/+08
    float m_width, m_height;                       // +0C/+10
    std::uint32_t m_field014;
    Node* m_field018;
    Node* m_field01c;                             // +1C, borrowed pointer owner
    Object* m_field020;
    Object* m_field024;
    float m_field028, m_field02c;
    int m_lockCount;                              // +30, "Lock count" 0x82A410
    NameTreeNode* m_registry;                     // +34
    float m_pointerX, m_pointerY;                  // +38/+3C, helper682BA0
    float m_previousPointerX, m_previousPointerY; // +40/+44, helper82A3A0
    EventQueue* m_eventQueue;                     // +48
    IStream* m_field04c;                          // +4C, cache replay input; wrappers borrow it
    OStream* m_field050;                          // +50, cache recording output; wrappers borrow it
    TextureHandle m_field054;                     // +54, default texture handle
};

// 0x82A340; exact return is unused by recovered callers, source name [INFERRED].
void systemPrintf(System*, const char* format, ...);
} }
#endif
#endif
