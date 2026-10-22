// An archive written again with some entries replaced (added) - Framework/ArchiveRewrite.h.
#include "Framework/ArchiveRewrite.h"

#include "Framework/ZipWriter.h"

#include "unzip.h"

#include <cctype>
#include <cstdio>

namespace bf2editor
{

namespace
{

std::string entryKey(const std::string& name)
{
    std::string key(name);
    for (size_t i = 0; i < key.size(); ++i)
        key[i] = key[i] == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(key[i])));
    return key;
}

} // namespace

bool rewriteArchive(const std::string& source, const std::string& destination,
                    const std::map<std::string, std::string>& replaced, std::vector<std::string>& missing,
                    std::string& error)
{
    missing.clear();
    std::map<std::string, const std::string*> wanted;
    for (std::map<std::string, std::string>::const_iterator it = replaced.begin(); it != replaced.end(); ++it)
        wanted[entryKey(it->first)] = &it->second;
    std::map<std::string, bool> found;

    unzFile zip = unzOpen(source.c_str());
    if (!zip)
    {
        error = "cannot open " + source + " as an archive";
        return false;
    }
    ZipWriter writer;
    if (!writer.open(destination, error))
    {
        unzClose(zip);
        return false;
    }
    bool ok = true;
    for (int status = unzGoToFirstFile(zip); status == UNZ_OK && ok; status = unzGoToNextFile(zip))
    {
        unz_file_info info;
        char name[1024];
        if (unzGetCurrentFileInfo(zip, &info, name, sizeof(name), 0, 0, 0, 0) != UNZ_OK)
        {
            error = "cannot read an entry of " + source;
            ok = false;
            break;
        }
        const unsigned dosTime = static_cast<unsigned>(info.dosDate & 0xFFFF);
        const unsigned dosDate = static_cast<unsigned>((info.dosDate >> 16) & 0xFFFF);
        const std::map<std::string, const std::string*>::const_iterator change = wanted.find(entryKey(name));
        if (change != wanted.end())
        {
            found[change->first] = true;
            const std::string& bytes = *change->second;
            ok = writer.add(name, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), dosTime, dosDate,
                            error);
            continue;
        }
        // As stored: the raw bytes, which minizip hands over undecoded when asked for raw.
        int method = 0, level = 0;
        if (unzOpenCurrentFile2(zip, &method, &level, 1) != UNZ_OK)
        {
            error = std::string("cannot read ") + name + " in " + source;
            ok = false;
            break;
        }
        std::string packed(info.compressed_size, '\0');
        size_t got = 0;
        while (got < packed.size())
        {
            const int read = unzReadCurrentFile(zip, &packed[got], static_cast<unsigned>(packed.size() - got));
            if (read <= 0)
                break;
            got += static_cast<size_t>(read);
        }
        unzCloseCurrentFile(zip);
        if (got != packed.size())
        {
            error = std::string("cannot read all of ") + name + " in " + source;
            ok = false;
            break;
        }
        ok = writer.addRaw(name, static_cast<unsigned>(info.compression_method), info.crc, info.uncompressed_size,
                           reinterpret_cast<const unsigned char*>(packed.data()), packed.size(), dosTime, dosDate, error);
    }
    unzClose(zip);
    for (std::map<std::string, const std::string*>::const_iterator it = wanted.begin(); it != wanted.end(); ++it)
        if (!found.count(it->first))
            missing.push_back(it->first);
    if (ok && !missing.empty())
    {
        error = source + " has no entry " + missing[0];
        ok = false;
    }
    std::string closeError;
    const bool closed = writer.close(closeError);
    if (ok && !closed)
    {
        error = closeError;
        ok = false;
    }
    if (!ok)
        std::remove(destination.c_str());
    return ok;
}

bool readArchiveEntry(const std::string& archive, const std::string& entry, std::string& bytes, std::string& error)
{
    unzFile zip = unzOpen(archive.c_str());
    if (!zip)
    {
        error = "cannot open " + archive + " as an archive";
        return false;
    }
    const std::string wanted = entryKey(entry);
    bool ok = false;
    for (int status = unzGoToFirstFile(zip); status == UNZ_OK; status = unzGoToNextFile(zip))
    {
        unz_file_info info;
        char name[1024];
        if (unzGetCurrentFileInfo(zip, &info, name, sizeof(name), 0, 0, 0, 0) != UNZ_OK || entryKey(name) != wanted)
            continue;
        if (unzOpenCurrentFile(zip) != UNZ_OK)
            break;
        bytes.assign(info.uncompressed_size, '\0');
        size_t got = 0;
        while (got < bytes.size())
        {
            const int read = unzReadCurrentFile(zip, &bytes[got], static_cast<unsigned>(bytes.size() - got));
            if (read <= 0)
                break;
            got += static_cast<size_t>(read);
        }
        ok = unzCloseCurrentFile(zip) == UNZ_OK && got == bytes.size();
        break;
    }
    unzClose(zip);
    if (!ok)
        error = "cannot read " + entry + " in " + archive;
    return ok;
}

} // namespace bf2editor
