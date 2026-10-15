// Path [INFERRED] in attested External/BFMeme; names RuntimeObject/Object
// are descriptor-attested at PE 0x94A0AC/0x949DCC.
#ifndef BFMEME_OBJECT_H
#define BFMEME_OBJECT_H
#include "../../Core/BuildConfig.h"
#if !DEDICATED
#include <cstdint>
#include <type_traits>
namespace dice { namespace meme {
// Font and texture handles carry the Font* / IMemeTexture* that the menu System returns, so
// they are pointer-sized. MSVC x86 defines intptr_t as int: retail layouts and x86 signatures
// are unchanged. [CHOSEN, not recovered] x64 port, issue #7.
typedef std::intptr_t FontHandle;
typedef std::intptr_t TextureHandle;
#if defined(_M_IX86)
static_assert(std::is_same<FontHandle, int>::value && std::is_same<TextureHandle, int>::value,
              "x86 meme handles keep the retail int type");
static_assert(std::is_same<std::uintptr_t, std::uint32_t>::value,
              "x86 unsigned handle words keep the retail uint32_t type");
#endif

class TypeDescriptor;
class System;
class Stream; // [INFERRED] common stream/visitor identity, not Object-derived.

// Complete six-slot PE table 0x9327A0.
class RuntimeObject
{
public:
    virtual ~RuntimeObject();                         // +00
    virtual TypeDescriptor* getType();                 // +04
    virtual unsigned int getObjectSize();              // +08
    virtual void destroy();                           // +0C
    virtual bool createRuntime();                     // +10 [INFERRED] name
    virtual bool destroyRuntime();                    // +14 [INFERRED] name
    static TypeDescriptor* getClassType();
};

// Complete sixteen-slot PE table 0x932B18. Slot names/constness are inferred;
// pointer types follow registration, cloning and concrete property visitors.
class Object : public RuntimeObject
{
public:
    ~Object() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    virtual const char* getName();                     // +18
    virtual bool setName(const char*);                 // +1C
    virtual bool slot_0x20();                          // identity [UNRESOLVED]
    virtual bool slot_0x24();                          // identity [UNRESOLVED]
    virtual bool onCreate(System*);                   // +28: register diagnostic
    virtual bool visit0x2c(System*, Stream*);           // +2C -> +30
    virtual bool streamProperties(System*, Stream*);   // +30
    virtual bool visit0x34(System*, Stream*);           // +34 -> +30
    virtual bool visit0x38(System*, Stream*);           // +38 -> +30
    virtual bool visit0x3c(System*, Stream*);           // +3C -> +30
    static TypeDescriptor* getClassType();
};

// 0x82BA00/0x82BA40; names [INFERRED]. Replacement retains retail loss of the
// old pointer when its destroyRuntime hook rejects deletion.
bool destroyRuntimeObject(RuntimeObject*& object);
bool createRuntimeObject(RuntimeObject*& object, TypeDescriptor* type);
} }
#endif
#endif
