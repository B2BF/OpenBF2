// [INFERRED] class/file names. Complete40-slot tables94F0B0/94F150/94F510.
#ifndef BFMEME_CLASSOSTREAM_H
#define BFMEME_CLASSOSTREAM_H
#include "OStream.h"
#if !DEDICATED
namespace dice { namespace meme {
class ForwardingOStream : public OStream
{
public:
    explicit ForwardingOStream(OStream* output) : m_destination(output) {} // inlined83BFB0
    ~ForwardingOStream() override;                  // 83BEE0, owns destination
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
    bool streamEnum(int*, EnumNext, EnumName, const char*) override;
    int write(const void*, int) override;
    void flush() override;
    int getPosition() override;
    bool setPosition(int) override;
    bool writeShortString(System*, const char*, const char*) override;
    // Collection/object visitors intentionally remain OStream's own methods.
protected:
    OStream* m_destination;                        // +4, owned
};
class MemoryOStream : public OStream
{
    friend class ClassOStream;
public:
    explicit MemoryOStream(int capacity);
    ~MemoryOStream() override;
    int write(const void*, int) override;
    int getPosition() override;
    bool setPosition(int) override;                // 83D280, end position excluded
private:
    char* m_bytes;                                 // +4
    int m_capacity;                                // +8
    int m_position;                                // +C, also length emitted by finish
};
class ClassOStream : public ForwardingOStream
{
public:
    // Full table94F150 inherits OStream::getType and its four-byte size;
    // there is no distinct ClassOStream descriptor/factory in this table.
    explicit ClassOStream(OStream* output);         // 83BFB0, takes ownership
    ~ClassOStream() override;
    bool writeShortString(System*, const char*, const char*) override;
    bool finish(System*);
private:
    char* m_strings[512];                          // +8, only first null initialized
    OStream* m_output;                             // +808, original destination
    MemoryOStream m_buffer;                        // +80C, 512 KiB initial allocation
};
} }
#endif
#endif
