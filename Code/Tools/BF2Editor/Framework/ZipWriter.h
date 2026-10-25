// Code/Tools/BF2Editor/Framework/ZipWriter.h

// A small pkzip writer for the level packer (Framework/LevelPacker): each entry deflated with zlib's raw
// deflate, or stored when deflating does not make it smaller (the stock level archives store their few
// incompressible files the same way), with its CRC-32 and DOS time; then one central directory. That is
// the form the stock archives have and the game's reader takes (minizip's unzip behind
// IO/File/ZipArchive): no ZIP64, no data descriptors, no encryption, so an entry and the archive stay
// under 4 GB and the entries under 65,535. Engine-free (zlib only). Editor-only code (D3).
#ifndef BF2EDITOR_FRAMEWORK_ZIPWRITER_H
#define BF2EDITOR_FRAMEWORK_ZIPWRITER_H

#include "Framework/EditorConfig.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace bf2editor
{

class ZipWriter
{
public:
    ZipWriter();
    ~ZipWriter();

    bool open(const std::string& path, std::string& error);
    // `name` as the archive stores it ('/' separators); `dosTime`/`dosDate` the entry's time (0 for none).
    bool add(const std::string& name, const unsigned char* data, size_t size, unsigned dosTime, unsigned dosDate,
             std::string& error);
    // An entry copied as another archive stores it: its compressed bytes (`method` 0 stored or 8 deflated), CRC-32,
    // size and time unchanged - an archive written again keeps the entries it does not change byte for byte.
    bool addRaw(const std::string& name, unsigned method, unsigned long crc, unsigned long size,
                const unsigned char* packed, size_t packedSize, unsigned dosTime, unsigned dosDate, std::string& error);
    // Writes the central directory and closes the file; false when anything failed on the way.
    bool close(std::string& error);
    size_t entryCount() const { return m_entries.size(); }
    unsigned long long bytesWritten() const { return m_offset; }

private:
    ZipWriter(const ZipWriter&);
    ZipWriter& operator=(const ZipWriter&);

    struct Entry
    {
        std::string   name;
        unsigned      method;       // 0 stored, 8 deflated
        unsigned      dosTime;
        unsigned      dosDate;
        unsigned long crc;
        unsigned long compressedSize;
        unsigned long size;
        unsigned long offset;       // of its local header
    };
    bool write(const void* data, size_t size);
    // The entry's local header, then its stored bytes; recorded for the central directory.
    bool writeEntry(const Entry& entry, const void* payload, std::string& error);

    std::FILE*          m_file;
    std::vector<Entry>  m_entries;
    unsigned long long  m_offset;
    bool                m_failed;
};

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_ZIPWRITER_H
