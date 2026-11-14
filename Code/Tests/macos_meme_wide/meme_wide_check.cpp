// [TEMP] [CHOSEN, not recovered] Engine-free check of BFMeme wide text on 32-bit wchar_t hosts.
// Allocation sizes are measured with malloc_size, so the check
// fails without a sanitizer too; build.sh also runs it under AddressSanitizer.
#include "../../BF2/External/BFMeme/Data.h"
#include "../../BF2/External/BFMeme/IStream.h"
#include "../../BF2/External/BFMeme/OStream.h"
#include "../../BF2/External/BFMeme/Action.h"
#include "../../BF2/External/BFMeme/Event.h"
#include "../../BF2/External/BFMeme/Function.h"
#include "../../BF2/External/BFMeme/ObjectCollection.h"
#include "../../BF2/External/BFMeme/Style.h"
#include "../../BF2/External/BFMeme/System.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <malloc/malloc.h>
#include <vector>

using namespace dice::meme;

// Stream dependencies outside the wide-text path. Reaching one is a check failure.
namespace dice { namespace meme {
[[noreturn]] static void unreached(const char* what)
{ std::printf("FAIL unexpected call: %s\n", what); std::abort(); }
bool readObject(System*, Object*&, IStream*) { unreached("readObject"); }
void systemPrintf(System*, const char*, ...) { unreached("systemPrintf"); }
bool ObjectCollection::read(System*, IStream*) { unreached("ObjectCollection::read"); }
bool ObjectCollection::write(System*, OStream*) { unreached("ObjectCollection::write"); }
TypeDescriptor* getNodeClassType() { unreached("getNodeClassType"); }
TypeDescriptor* getEffectClassType() { unreached("getEffectClassType"); }
TypeDescriptor* Event::getClassType() { unreached("Event::getClassType"); }
TypeDescriptor* Style::getClassType() { unreached("Style::getClassType"); }
TypeDescriptor* Action::getClassType() { unreached("Action::getClassType"); }
TypeDescriptor* Function::getClassType() { unreached("Function::getClassType"); }
} }

namespace {
int failures;
void check(bool good, const char* label)
{
    if (!good) { ++failures; std::printf("FAIL %s\n", label); }
}

// Reads the private owned pointer through the one accessor the class has: getCharacter, plus
// the heap block it lives in, found by scanning the object for a pointer to a live block.
const wchar_t* ownedText(WstringData& data)
{
    const wchar_t* found = nullptr;
    unsigned char* bytes = reinterpret_cast<unsigned char*>(&data);
    for (unsigned int offset = sizeof(void*); offset + sizeof(void*) <= sizeof(WstringData);
         offset += sizeof(void*)) {
        const wchar_t* candidate;
        std::memcpy(&candidate, bytes + offset, sizeof(candidate));
        if (candidate && malloc_zone_from_ptr(candidate)) found = candidate;
    }
    return found;
}

struct MemoryOut : OStream {
    std::vector<unsigned char> bytes;
    int write(const void* data, int count) override
    {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        bytes.insert(bytes.end(), p, p + count);
        return count;
    }
};

struct MemoryIn : IStream {
    std::vector<unsigned char> bytes;
    std::size_t position = 0;
    int read(void* data, int count) override
    {
        const std::size_t n = std::min<std::size_t>(count, bytes.size() - position);
        std::memcpy(data, bytes.data() + position, n);
        position += n;
        return static_cast<int>(n);
    }
};
}

int main()
{
    static_assert(sizeof(wchar_t) == 4, "this check models the POSIX width");
    const wchar_t* text = L"Fonts/vehicleHudFont_6 \x00E4\x20AC";
    const std::size_t length = std::wcslen(text);

    WstringData cloned(text, "");
    const wchar_t* copy = ownedText(cloned);
    check(copy && std::wcscmp(copy, text) == 0, "constructor clone keeps the text");
    check(copy && malloc_size(copy) >= (length + 1) * sizeof(wchar_t),
          "constructor clone holds length + 1 native characters");

    cloned.setString(nullptr, "narrow widened");
    copy = ownedText(cloned);
    check(copy && std::wcscmp(copy, L"narrow widened") == 0, "setString widens");
    check(copy && malloc_size(copy) >= (std::strlen("narrow widened") + 1) * sizeof(wchar_t),
          "setString allocation holds native characters");

    // The wire format is the Windows one: an Int count and count UTF-16LE units.
    MemoryOut out;
    wchar_t* value = const_cast<wchar_t*>(text);
    check(out.streamWideString(&value, "Wstring"), "OStream::streamWideString succeeds");
    check(out.bytes.size() == 4 + 2 * length, "wire is a count plus 16-bit units");
    check(out.bytes.size() >= 4 + 2 * length && out.bytes[4] == 'F' && out.bytes[5] == 0
          && out.bytes[4 + 2 * (length - 1)] == 0xAC && out.bytes[5 + 2 * (length - 1)] == 0x20,
          "units are little-endian UTF-16");

    MemoryIn in;
    in.bytes = out.bytes;
    wchar_t* read = nullptr;
    check(in.streamWideString(&read, "Wstring"), "IStream::streamWideString succeeds");
    check(read && std::wcscmp(read, text) == 0, "round trip restores the text");
    check(read && malloc_size(read) >= (length + 1) * sizeof(wchar_t),
          "IStream allocation holds length + 1 native characters");
    ::operator delete(read);

    // Long and Ulong are 32 bits on the wire; LP64 long must not keep stale upper bits.
    MemoryIn longs;
    longs.bytes = {0xFE, 0xFF, 0xFF, 0xFF, 0x10, 0x00, 0x00, 0x80};
    long signedValue = 0x5A5A5A5A5A5A5A5AL;
    unsigned long unsignedValue = 0x5A5A5A5A5A5A5A5AUL;
    check(longs.streamLong(&signedValue, "Long") && signedValue == -2, "streamLong sign-extends");
    check(longs.streamULong(&unsignedValue, "Ulong") && unsignedValue == 0x80000010UL,
          "streamULong zero-extends");

    std::printf(failures ? "meme_wide_check: %d failure(s)\n" : "meme_wide_check: all passed\n",
                failures);
    return failures ? 1 : 0;
}
