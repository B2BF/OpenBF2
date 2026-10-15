// Stream visitor interface. Names, pointer/reference spelling, and some scalar signedness are [INFERRED].
// Preserve the virtual declaration order.
#ifndef BFMEME_STREAM_H
#define BFMEME_STREAM_H
#include "Object.h"
#include "ObjectCollection.h"
#if !DEDICATED
namespace dice { namespace meme {
class IStream;
class OStream;
class Data;
class Event;
class Action;
class Effect;
class Function;
class Node;
class Style;
class Tree;

typedef int (__fastcall* EnumNext)(int);            // 839620, ECX value -> next integer
typedef const char* (__fastcall* EnumName)(int);    // 800AA0, ECX value -> text

class Stream : public RuntimeObject
{
public:
    // Inherits the six actual RuntimeObject slots, including its descriptor and
    // four-byte size. Generic visitors call this mode hook each time; they do
    // not contain unconditional true defaults. Name is deliberately neutral.
    virtual bool isPassive();                       // +18 false; DeletionStream true
    virtual bool streamUByte(unsigned char*, const char*); // +1C 837D10
    virtual bool streamByte(char*, const char*);     // +20
    virtual bool streamUShort(unsigned short*, const char*); // +24
    virtual bool streamShort(short*, const char*);   // +28
    virtual bool streamULong(unsigned long*, const char*); // +2C
    virtual bool streamLong(long*, const char*);     // +30
    virtual bool streamFloat(float*, const char*);   // +34
    virtual bool streamBool(bool*, const char*);     // +38
    virtual bool streamInt(int*, const char*);       // +3C
    virtual bool streamString(char**, const char*); // +40
    virtual bool streamWideString(wchar_t**, const char*); // +44
    virtual bool streamCharacter(wchar_t*, const char*); // +48
    virtual bool streamTexture(System*, TextureHandle*, const char*); // +4C 837D20
    virtual bool streamFont(System*, FontHandle*, const char*); // +50
    virtual bool streamSound(System*, int*, const char*); // +54
    virtual bool streamCollection(System*, ObjectCollection*, const char*); // +58
    virtual bool streamEnum(int*, EnumNext, EnumName, const char*); // +5C 837D30
    virtual bool streamObject(System*, Object*&, const char*, TypeDescriptor*); // +60
    virtual bool streamEvent(System*, Event*&, const char*, TypeDescriptor*); // +64
    virtual bool streamAction(System*, Action*&, const char*, TypeDescriptor*); // +68
    virtual bool streamData(System*, Data*&, const char*, TypeDescriptor*); // +6C
    virtual bool streamEffect(System*, Effect*&, const char*, TypeDescriptor*); // +70
    virtual bool streamFunction(System*, Function*&, const char*, TypeDescriptor*); // +74
    virtual bool streamNode(System*, Node*&, const char*, TypeDescriptor*); // +78
    virtual bool streamStyle(System*, Style*&, const char*, TypeDescriptor*); // +7C
    virtual bool streamTree(System*, Tree*&, const char*, TypeDescriptor*); // +80
    virtual bool streamNode0x84(System*, Node*&, const char*, TypeDescriptor*); // +84
    virtual bool streamNode0x88(System*, Node*&, const char*, TypeDescriptor*); // +88
};

class DeletionStream : public Stream
{
public:
    explicit DeletionStream(bool preserveNamed) : m_preserveNamed(preserveNamed) {}
    bool isPassive() override;                      // table949C70, 5A5E20
    bool streamCollection(System*, ObjectCollection*, const char*) override;
    bool streamObject(System*, Object*&, const char*, TypeDescriptor*) override;
    virtual bool destroyObject(Object*&);           // +8C 83BD70
private:
    bool m_preserveNamed;                           // +4; padding untouched
};

bool deleteObject(System*, Object*&);
bool createObject(System*, Object*&, TypeDescriptor*);
// Serialization helpers have distinct binary/class-stream dependencies.
bool readObject(System*, Object*&, IStream*);
bool readNamedObject(System*, Object*&, IStream*, const char*);
bool loadNamedObject(System*, Object*&, const char*); // 82AEC0, class/old-format retry
bool writeObject(System*, Object*&, OStream*);
bool saveNamedObject(System*, Object*&);

// The actual descriptor getters are known, but their incomplete object families
// must not be published just to resolve these null-type wrapper branches.
TypeDescriptor* getEventClassType();                 // 83B080, complete Event family
TypeDescriptor* getActionClassType();                // 83C320, complete Action family
TypeDescriptor* getEffectClassType();                // 82D860, complete Effect family
TypeDescriptor* getFunctionClassType();              // 82E510, complete Function family
TypeDescriptor* getNodeClassType();                  // 82C190, real native-prefix metadata/factory; suffix caller stops
TypeDescriptor* getTreeClassType();                  // 83D010 [FRONTIER]
} }
#endif
#endif
