// An archive written again with some entries' content replaced (the HUD editor's save into Menu_client.zip): every
// other entry copied exactly as the archive stores it, in its order. Added: the retail editor had no HUD editor.
// Engine-free (minizip and zlib). Editor-only code (D3).
#ifndef BF2EDITOR_FRAMEWORK_ARCHIVEREWRITE_H
#define BF2EDITOR_FRAMEWORK_ARCHIVEREWRITE_H

#include "Framework/EditorConfig.h"

#include <map>
#include <string>
#include <vector>

namespace bf2editor
{

// `source` written to `destination` with the entries named in `replaced` (keys compared without regard to case or
// slash direction) given their new content, deflated anew; every other entry copied as stored - its compressed
// bytes, CRC-32 and time unchanged. A key no entry matches is listed in `missing` and fails the rewrite.
bool rewriteArchive(const std::string& source, const std::string& destination,
                    const std::map<std::string, std::string>& replaced, std::vector<std::string>& missing,
                    std::string& error);

// An entry of an archive, read whole. False when the archive or the entry cannot be read.
bool readArchiveEntry(const std::string& archive, const std::string& entry, std::string& bytes, std::string& error);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_ARCHIVEREWRITE_H
