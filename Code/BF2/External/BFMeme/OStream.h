// OStream identity descriptor-attested94E974; path [INFERRED]. Complete40 slots.
#ifndef BFMEME_OSTREAM_H
#define BFMEME_OSTREAM_H
#include "Stream.h"
#if !DEDICATED
namespace dice { namespace meme {
class OStream : public Stream
{
public:
    ~OStream() override;                            // 794CA0 -> RuntimeObject dtor
    TypeDescriptor* getType() override;             // 8383E0 [FRONTIER] publication
    unsigned int getObjectSize() override;
    bool streamUByte(unsigned char*, const char*) override;
    bool streamByte(char*, const char*) override;
    bool streamUShort(unsigned short*, const char*) override;
    bool streamShort(short*, const char*) override;
    bool streamULong(unsigned long*, const char*) override;
    bool streamLong(long*, const char*) override;
    bool streamFloat(float*, const char*) override;
    bool streamBool(bool*, const char*) override;
    bool streamInt(int*, const char*) override;
    bool streamString(char**, const char*) override;
    bool streamWideString(wchar_t**, const char*) override;
    bool streamCharacter(wchar_t*, const char*) override;
    bool streamTexture(System*, TextureHandle*, const char*) override;
    bool streamFont(System*, FontHandle*, const char*) override;
    bool streamSound(System*, int*, const char*) override;
    bool streamCollection(System*, ObjectCollection*, const char*) override;
    bool streamEnum(int*, EnumNext, EnumName, const char*) override;
    bool streamObject(System*, Object*&, const char*, TypeDescriptor*) override;
    virtual int write(const void*, int bytes);      // +8C 6212F0, byte count
    virtual void flush();
    virtual int getPosition();                     // +94 80D430; serializer82AA00
    virtual bool setPosition(int);
    virtual bool writeShortString(System*, const char*, const char* label); // +9C 838330
    static TypeDescriptor* getClassType();          // [FRONTIER], no partial factory
};
} }
#endif
#endif
