// ClassIStream identity descriptor-attested83C2B0; other names/path [INFERRED].
#ifndef BFMEME_CLASSISTREAM_H
#define BFMEME_CLASSISTREAM_H
#include "IStream.h"
#if !DEDICATED
namespace dice { namespace meme {
class ForwardingIStream : public IStream
{
public:
    ForwardingIStream(IStream*, bool ownsInput);     // 83B2A0, complete41-slot table94EF00
    ~ForwardingIStream() override;
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
    int getAvailableBytes() override;
    int skip(int) override;
    int read(void*, int) override;
    int getPosition() override;
    bool setPosition(int) override;
    bool readShortString(System*, char*, int, const char*) override;
protected:
    IStream* m_input;                               // +8
    bool m_ownsInput;                               // +C, padding untouched
};
class ClassIStream : public ForwardingIStream
{
public:
    explicit ClassIStream(IStream* input);          // 83BFF0, owns input
    ~ClassIStream() override;
    TypeDescriptor* getType() override;             // 83C2B0 [FRONTIER] factory closure
    unsigned int getObjectSize() override;
    int getPosition() override;                    // 83C140, relative to payload
    bool setPosition(int) override;
    bool readShortString(System*, char*, int, const char*) override;
    bool init(System*);
    static TypeDescriptor* getClassType();          // [FRONTIER] no partial publication
private:
    int m_payloadStart;                            // +10
    char* m_strings[512];                          // +14, only first null initialized
};
} }
#endif
#endif
