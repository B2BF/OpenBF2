// [CHOSEN, not recovered] Helper for the engine's open/size/read/release sequence.
// Reads through FileManager so mounted archives and mod layering apply to shaders, fonts and textures.
// Use the host-compatible typed stream interface and release each opened stream.

#ifndef RENDDX9ABI_ENGINEFILE_H
#define RENDDX9ABI_ENGINEFILE_H

#include <string>
#include "HostFileAbi.h"

namespace renddx9abi {

// The bytes of one engine file, plus whether the read actually happened. `data` is allocated with
// the engine allocator (bf_malloc) and must be released with free(), below - not with delete, and
// not with the CRT's free.
struct EngineFileData
{
    unsigned char* data;
    unsigned int   size;

    EngineFileData() : data(0), size(0) {}
    bool ok() const { return data != 0; }
};

// Opens `path` through the host FileManager, reads it whole, releases the stream, and returns the
// buffer. Returns an empty result if the FileManager is absent or the file is not found.

// the original does not fail quietly here. sub_1001E010 loops on a MessageBox reading
// "... not found!!! _DO_ check your working directory, _AND_ sync your shaders folder before
// calling upon your local rendering programmer/GP. (really!)" and calls exit(99) if the user
// answers Cancel. That dialog is not reproduced - a modal box in a rebuild under test is worse than
// a logged failure - and the divergence is recorded here rather than hidden.
EngineFileData readEngineFile(const char* path);

// Releases what readEngineFile returned.
void freeEngineFile(EngineFileData& file);

// HemiMapManager's resource/output helpers. These preserve the FileManager calls the retail body
// makes through HostFileAbi's typed IFileManager: +0x1C fileExists, +0x18 open with the editor
// write pair (3,2,0), and +0xA4 getWritePath.
bool engineFileExists(const std::string& path);
void engineFilePrepareWrite(const std::string& path);
void engineFileGetWritePath(const std::string& path, std::string& out);

// Owned engine stream for incremental reads and absolute/relative seeks, used by texture mip loading.
// Destruction closes the stream.
class EngineStream
{
public:
    EngineStream() : m_stream(0) {}
    ~EngineStream() { close(); }

    // FileManager +0x18, open(path, 2, 0, 0) - the same mode/access pair every recovered call uses.
    bool open(const std::string& path);
    void close();                                        // stream +0x08 release
    bool isOpen() const { return m_stream != 0; }

    unsigned int read(void* buffer, unsigned int size);  // stream +0x10
    int          seek(int origin, int offset);           // stream +0x30 - 0 = absolute, 1 = relative
    int          getSize();                              // stream +0x38

private:
    EngineStream(const EngineStream&);
    EngineStream& operator=(const EngineStream&);

    HostNonlinearStream* m_stream;
};

} // namespace renddx9abi

#endif // RENDDX9ABI_ENGINEFILE_H
