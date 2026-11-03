// Nonpolymorphic raii wrapper owning one reference to an IStream. [INFERRED] Source placement.

#ifndef IO_FILE_FILE_H     // [INFERRED] guard style
#define IO_FILE_FILE_H

#include <string>
#include <sstream>

#include "IFileSystem.h"     // IStream, IFileSystem::FileMode, IFileSystem::FileAccess

namespace dice {
namespace hfe {

// Forward declarations matching Math/Vec3.h and Core/SettingsRepostitory.h: the unqualified
// spellings are typedefs of Math:: structs in this tree, so the bare `struct Vec3;` this header
// used to carry collided (C2371) with any TU that also includes the real headers - which BF2Log.cpp
// now does. Redeclaring a typedef to the same type is legal, so both forms coexist.
namespace Math { struct Vec2; struct Vec3; struct Vec4; }
typedef Math::Vec2 Vec2;
typedef Math::Vec3 Vec3;
typedef Math::Vec4 Vec4;

namespace io {

// SeekOrigin is defined with the shared stream interface.

class File
{
public:
    File();
    File(const std::string& path, IFileSystem::FileMode mode, IFileSystem::FileAccess access,
         unsigned int flags);
    ~File();

    bool open(const std::string& path, IFileSystem::FileMode mode, IFileSystem::FileAccess access,
              unsigned int flags);
    bool isOpen();
    void close();

    // The raw pair everything else is built on
    unsigned int read(void* buffer, unsigned int size);         // > IStream +0x10
    unsigned int write(const void* buffer, unsigned int size);  // > IStream +0x14

    int  seek(SeekOrigin origin, int offset);                   // > INonlinearStream +0x30
    int  getPosition();                                         // > INonlinearStream +0x34
    int  getAvailableBytes();                                   // > IStream +0x1C
    int  getSize();                                             // > INonlinearStream +0x38

    // 10 bytes, `mov eax,[this]` - hands back the raw IStream*. Named getStream, not getFile.
    IStream* getStream();

    // Reads each value's native bytes and returns true only when the entire value was read.
    bool read(unsigned int& v);
    bool read(int& v);
    bool read(unsigned short& v);
    bool read(short& v);
    bool read(unsigned char& v);
    bool read(char& v);
    bool read(float& v);
    bool read(double& v);
    bool read(bool& v);
    bool read(Vec2& v);
    bool read(Vec3& v);
    bool read(Vec4& v);

    // Typed writes: the mirror set
    bool write(unsigned int v);
    bool write(int v);
    bool write(unsigned short v);
    bool write(short v);
    bool write(unsigned char v);
    bool write(char v);
    bool write(float v);
    bool write(double v);
    bool write(bool v);
    bool write(const Vec2& v);
    bool write(const Vec3& v);
    bool write(const Vec4& v);

    // Reads consume the delimiter. Narrow reads return false without assigning on empty input;
    // wide reads assign even empty input and return true. Both have an unchecked 4096-unit limit.
    // Native engine wide reads use the UTF-16 port adaptation; the AI DLL uses native wchar_t.
    bool read(std::string& out, char delimiter);
    bool read(std::wstring& out, wchar_t delimiter);
    bool write(const std::string& v);
    // Writes native character bytes without a length or terminator (not a POSIX UTF-16 encoder).
    bool write(const std::wstring& v);
    unsigned int write(std::ostringstream& s);

private:
    // [CHOSEN] Owned seekable-stream reference; filesystem open results must implement INonlinearStream.
    /* +0x00 */ INonlinearStream* m_file;    // the whole object. sizeof(File) == 4.
};

} // namespace io
} // namespace hfe
} // namespace dice

#endif // IO_FILE_FILE_H
