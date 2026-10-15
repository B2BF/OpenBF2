// IStream identity descriptor-attested94EC20; path [INFERRED]. Complete41 slots.
// This BFMeme serialization visitor is distinct from hfe::io::IStream.
#ifndef BFMEME_ISTREAM_H
#define BFMEME_ISTREAM_H
#include "Stream.h"
#if !DEDICATED
namespace dice { namespace meme {
class IStream : public Stream
{
    friend class ForwardingIStream; // 83B2A0/83B0E0 rewrite child source links directly.
    friend class RelativeIStream;   // 83B3A0/83B3C0/83B3E0 reassert the child source link.
public:
    explicit IStream(IStream* source = nullptr);    // 83A1A0, null source means self
    ~IStream() override;
    TypeDescriptor* getType() override;             // 83A160 [FRONTIER]
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
    virtual int getAvailableBytes();               // +8C 80D430
    virtual int skip(int);
    virtual int read(void*, int bytes);
    virtual int getPosition();
    virtual bool setPosition(int);                 // +9C 455400
    virtual bool readShortString(System*, char*, int capacity, const char*); // +A0 83A0A0
    IStream* getSource();                          // 839B60, follows self-terminated chain
    static TypeDescriptor* getClassType();          // [FRONTIER] no partial publication
private:
    IStream* m_source;                             // +4 borrowed; destructor does not free
};
} }
#endif
#endif
