// [CHOSEN, not recovered] The redirects of the online service names, read from the provider file
// of +onlineProviderConfig; see OnlineProvider.h. This file has no link dependency on the GameSpy
// SDK; the redirect is installed into the SDK in OnlineProviderGameSpy.cpp.
#include "OnlineProvider.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace dice {
namespace hfe {
namespace onlineProvider {

namespace {

// The retail domains of the names the file covers.
const char kGameSpyDomain[] = "gamespy.com";
const char kDiceDomain[] = "dice.se";

// The labels a "services" key can name: each exactly as the retail name spells it, and the retail
// domain it stands before.
struct LabelEntry
{
    const char* label;
    const char* domain;
};

const LabelEntry kLabels[] = {
    { "available", kGameSpyDomain },     // battlefield2.available.gamespy.com (availability check)
    { "ms14",      kGameSpyDomain },     // battlefield2.ms14.gamespy.com (server list)
    { "master",    kGameSpyDomain },     // battlefield2.master.gamespy.com (heartbeats, CD keys)
    { "gpcm",      kGameSpyDomain },     // gpcm.gamespy.com (login)
    { "gpsp",      kGameSpyDomain },     // gpsp.gamespy.com (account search)
    { "gamestats", kGameSpyDomain },     // battlefield2.gamestats.gamespy.com (game statistics)
    { "BF2Web",    kGameSpyDomain },     // BF2Web.gamespy.com (stats web requests)
    { "stage-net", kGameSpyDomain },     // stage-net.gamespy.com (player info request)
    { "eapusher",  kDiceDomain },        // eapusher.dice.se (menu images)
    { "natneg1",   kGameSpyDomain },     // battlefield2.natneg1.gamespy.com (NAT negotiation)
    { "natneg2",   kGameSpyDomain },
    { "natneg3",   kGameSpyDomain },
};
const int kLabelCount = sizeof(kLabels) / sizeof(kLabels[0]);

// The longest DNS name.
const std::string::size_type kMaxName = 253;

// The longest prefix a "*" domain keeps: "battlefield2.available." (as long as the statistics one).
const std::string::size_type kLongestPrefix = sizeof("battlefield2.available.") - 1;

// The file's limits.
const std::string::size_type kMaxFileSize = 64 * 1024;
const std::string::size_type kMaxProviderName = 64;
const int kMaxDepth = 64;                   // nesting of the ignored values

// The provider file in effect.
struct Config
{
    std::string path;                       // as given on the command line; empty: no file
    std::string provider;                   // "provider", empty without one
    std::string fallback;                   // services["*"], empty without one
    std::string labels[kLabelCount];        // services[<label>], empty without one
};

Config& config()
{
    static Config value;
    return value;
}

// Labels of ASCII letters, digits and '-' separated by single dots, as in a DNS name.
bool isDomainName(const std::string& text)
{
    bool labelStart = true;
    for (std::string::size_type i = 0; i < text.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '.')
        {
            if (labelStart)
                return false;
            labelStart = true;
        }
        else if ((c < 0x80 && std::isalnum(c)) || c == '-')
            labelStart = false;
        else
            return false;
    }
    return !labelStart;
}

bool isNumericAddress(const std::string& text)
{
    for (std::string::size_type i = 0; i < text.size(); ++i)
        if (text[i] != '.' && !std::isdigit(static_cast<unsigned char>(text[i])))
            return false;
    return true;
}

// Four decimal numbers from 0 to 255 separated by dots.
bool isIPv4Address(const std::string& text)
{
    int parts = 0;
    std::string::size_type i = 0;
    while (i < text.size())
    {
        int value = 0;
        std::string::size_type digits = 0;
        for (; i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])); ++i, ++digits)
            value = value * 10 + (text[i] - '0');
        if (digits == 0 || digits > 3 || value > 255)
            return false;
        ++parts;
        if (i == text.size())
            break;
        if (text[i] != '.' || ++i == text.size())
            return false;
    }
    return parts == 4;
}

std::string lowerCase(std::string text)
{
    for (std::string::size_type i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

// Text for a message, in double quotes: printable ASCII as it is, every other byte (and '"' and
// '\') as \xNN, at most 80 bytes.
std::string quoted(const std::string& text)
{
    std::string out("\"");
    for (std::string::size_type i = 0; i < text.size() && i < 80; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= 0x20 && c < 0x7F && c != '"' && c != '\\')
            out += static_cast<char>(c);
        else
        {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\x%02X", c);
            out += escaped;
        }
    }
    if (text.size() > 80)
        out += "...";
    return out + "\"";
}

// A value: a dotted IPv4 address or a host/domain name no longer than longest.
bool checkValue(const std::string& value, std::string::size_type longest, std::string& error)
{
    if (value.empty())
    {
        error = "an IPv4 address or host name is required";
        return false;
    }
    if (!isIPv4Address(value) && (!isDomainName(value) || isNumericAddress(value)))
    {
        error = quoted(value) + " is not an IPv4 address or host name";
        return false;
    }
    if (value.size() > longest)
    {
        error = quoted(value) + " is too long";
        return false;
    }
    return true;
}

// The length of name's part before "." + domain (lower case) when name (lower case) is under
// domain; npos otherwise. A name equal to domain has a prefix of length 0.
std::string::size_type prefixLength(const std::string& name, const char* domain)
{
    const std::string::size_type n = std::strlen(domain);
    if (name.size() == n)
        return name == domain ? 0 : std::string::npos;
    if (name.size() <= n + 1 || name[name.size() - n - 1] != '.' || name.compare(name.size() - n, n, domain) != 0)
        return std::string::npos;
    return name.size() - n - 1;
}

// The retail domain name is under, or 0.
const char* retailDomain(const std::string& lower)
{
    if (prefixLength(lower, kGameSpyDomain) != std::string::npos)
        return kGameSpyDomain;
    if (prefixLength(lower, kDiceDomain) != std::string::npos)
        return kDiceDomain;
    return 0;
}

// ---- the JSON reader ----------------------------------------------------------------------------

enum JsonType { JsonNull, JsonBoolean, JsonNumber, JsonString, JsonArray, JsonObject };

const char* typeName(JsonType type)
{
    switch (type)
    {
    case JsonNull:    return "null";
    case JsonBoolean: return "a boolean";
    case JsonNumber:  return "a number";
    case JsonString:  return "a string";
    case JsonArray:   return "an array";
    case JsonObject:  return "an object";
    }
    return "a value";
}

// A member of the top-level object, or of an object directly inside it.
struct JsonEntry
{
    int parent;                             // index of the member whose object holds it; -1: top level
    std::string key;
    JsonType type;
    std::string text;                       // a string's value (UTF-8) or a number's token
};

// A strict reader for RFC 8259 JSON. It keeps the members of the top-level object and of the
// objects directly inside it, which is all the provider file needs; deeper values are checked and
// skipped.
class JsonReader
{
public:
    explicit JsonReader(const std::string& text) : m_text(text), m_pos(0), m_rootType(JsonNull) {}

    // The whole text: an optional UTF-8 byte order mark, one value, white space around it.
    bool readDocument(std::string& error)
    {
        if (m_text.compare(0, 3, "\xEF\xBB\xBF") == 0)
            m_pos = 3;
        skipSpace();
        if (readValue(0, kRoot))
        {
            skipSpace();
            if (m_pos == m_text.size())
                return true;
            fail("text after the end of the JSON value");
        }
        error = m_error;
        return false;
    }

    JsonType rootType() const { return m_rootType; }
    const std::vector<JsonEntry>& entries() const { return m_entries; }

private:
    // The slot a value is read into: an index into m_entries, the root, or nowhere.
    enum { kRoot = -1, kIgnored = -2 };

    bool atEnd() const { return m_pos >= m_text.size(); }
    unsigned char peek() const { return atEnd() ? 0 : static_cast<unsigned char>(m_text[m_pos]); }
    bool isDigit() const { return peek() >= '0' && peek() <= '9'; }

    bool fail(const std::string& what)
    {
        if (!m_error.empty())
            return false;
        unsigned line = 1, column = 1;
        for (std::string::size_type i = 0; i < m_pos && i < m_text.size(); ++i)
        {
            if (m_text[i] == '\n')
            {
                ++line;
                column = 1;
            }
            else
                ++column;
        }
        char where[64];
        std::snprintf(where, sizeof(where), "not valid JSON at line %u, column %u: ", line, column);
        m_error = where + what;
        return false;
    }

    void skipSpace()
    {
        while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r'))
            ++m_pos;
    }

    void setType(int slot, JsonType type)
    {
        if (slot == kRoot)
            m_rootType = type;
        else if (slot >= 0)
            m_entries[slot].type = type;
    }

    // One value at nesting depth, recorded in slot.
    bool readValue(int depth, int slot)
    {
        if (depth > kMaxDepth)
            return fail("values nested too deeply");
        const unsigned char c = peek();
        if (c == '{')
        {
            setType(slot, JsonObject);
            return readObject(depth, slot);
        }
        if (c == '[')
        {
            setType(slot, JsonArray);
            return readArray(depth);
        }
        if (c == '"')
        {
            setType(slot, JsonString);
            return readString(slot >= 0 ? &m_entries[slot].text : 0);
        }
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            setType(slot, JsonNumber);
            const std::string::size_type start = m_pos;
            if (!readNumber())
                return false;
            if (slot >= 0)
                m_entries[slot].text.assign(m_text, start, m_pos - start);
            return true;
        }
        if (literal("true") || literal("false"))
        {
            setType(slot, JsonBoolean);
            return true;
        }
        if (literal("null"))
        {
            setType(slot, JsonNull);
            return true;
        }
        if (atEnd())
            return fail("unexpected end of the text");
        return fail("unexpected character");
    }

    bool readObject(int depth, int slot)
    {
        const bool keep = slot != kIgnored && depth < 2;
        ++m_pos;                                            // '{'
        skipSpace();
        if (peek() == '}')
        {
            ++m_pos;
            return true;
        }
        for (;;)
        {
            skipSpace();
            if (peek() != '"')
                return fail("expected a key in double quotes");
            std::string key;
            if (!readString(&key))
                return false;
            skipSpace();
            if (peek() != ':')
                return fail("expected ':'");
            ++m_pos;
            skipSpace();
            int child = kIgnored;
            if (keep)
            {
                JsonEntry entry;
                entry.parent = slot;
                entry.key = key;
                entry.type = JsonNull;
                m_entries.push_back(entry);
                child = static_cast<int>(m_entries.size()) - 1;
            }
            if (!readValue(depth + 1, child))
                return false;
            skipSpace();
            if (peek() == ',')
                ++m_pos;
            else if (peek() == '}')
            {
                ++m_pos;
                return true;
            }
            else
                return fail("expected ',' or '}'");
        }
    }

    bool readArray(int depth)
    {
        ++m_pos;                                            // '['
        skipSpace();
        if (peek() == ']')
        {
            ++m_pos;
            return true;
        }
        for (;;)
        {
            skipSpace();
            if (!readValue(depth + 1, kIgnored))
                return false;
            skipSpace();
            if (peek() == ',')
                ++m_pos;
            else if (peek() == ']')
            {
                ++m_pos;
                return true;
            }
            else
                return fail("expected ',' or ']'");
        }
    }

    bool literal(const char* word)
    {
        const std::string::size_type n = std::strlen(word);
        if (m_text.compare(m_pos, n, word) != 0)
            return false;
        m_pos += n;
        return true;
    }

    // -?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)?
    bool readNumber()
    {
        if (peek() == '-')
            ++m_pos;
        if (peek() == '0')
            ++m_pos;
        else if (peek() >= '1' && peek() <= '9')
            while (isDigit())
                ++m_pos;
        else
            return fail("malformed number");
        if (peek() == '.')
        {
            ++m_pos;
            if (!isDigit())
                return fail("malformed number");
            while (isDigit())
                ++m_pos;
        }
        if (peek() == 'e' || peek() == 'E')
        {
            ++m_pos;
            if (peek() == '+' || peek() == '-')
                ++m_pos;
            if (!isDigit())
                return fail("malformed number");
            while (isDigit())
                ++m_pos;
        }
        return true;
    }

    // The four hex digits of a \u escape, at m_pos.
    bool readHex4(unsigned& value)
    {
        value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const unsigned char c = peek();
            unsigned digit;
            if (c >= '0' && c <= '9')
                digit = c - '0';
            else if (c >= 'a' && c <= 'f')
                digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                digit = c - 'A' + 10;
            else
                return fail("malformed \\u escape");
            value = value * 16 + digit;
            ++m_pos;
        }
        return true;
    }

    static void appendUtf8(std::string& out, unsigned code)
    {
        if (code < 0x80)
            out += static_cast<char>(code);
        else if (code < 0x800)
        {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else if (code < 0x10000)
        {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    // One UTF-8 character of two to four bytes at m_pos: no overlong form, no surrogate, nothing
    // above U+10FFFF.
    bool readUtf8(std::string* out)
    {
        const unsigned char lead = peek();
        std::string::size_type length;
        unsigned code;
        if (lead >= 0xC2 && lead <= 0xDF)      { length = 2; code = lead & 0x1Fu; }
        else if (lead >= 0xE0 && lead <= 0xEF) { length = 3; code = lead & 0x0Fu; }
        else if (lead >= 0xF0 && lead <= 0xF4) { length = 4; code = lead & 0x07u; }
        else
            return fail("malformed UTF-8");
        if (m_text.size() - m_pos < length)
            return fail("malformed UTF-8");
        for (std::string::size_type i = 1; i < length; ++i)
        {
            const unsigned char next = static_cast<unsigned char>(m_text[m_pos + i]);
            if ((next & 0xC0) != 0x80)
                return fail("malformed UTF-8");
            code = (code << 6) | (next & 0x3Fu);
        }
        if ((length == 3 && code < 0x800) || (length == 4 && (code < 0x10000 || code > 0x10FFFF))
            || (code >= 0xD800 && code <= 0xDFFF))
            return fail("malformed UTF-8");
        if (out)
            out->append(m_text, m_pos, length);
        m_pos += length;
        return true;
    }

    // A string at m_pos ('"'); out, when given, receives its value in UTF-8.
    bool readString(std::string* out)
    {
        ++m_pos;                                            // '"'
        for (;;)
        {
            if (atEnd())
                return fail("unterminated string");
            const unsigned char c = peek();
            if (c == '"')
            {
                ++m_pos;
                return true;
            }
            if (c < 0x20)
                return fail("control character in a string");
            if (c >= 0x80)
            {
                if (!readUtf8(out))
                    return false;
                continue;
            }
            if (c != '\\')
            {
                if (out)
                    *out += static_cast<char>(c);
                ++m_pos;
                continue;
            }
            ++m_pos;                                        // '\'
            char plain;
            switch (peek())
            {
            case '"':  plain = '"'; break;
            case '\\': plain = '\\'; break;
            case '/':  plain = '/'; break;
            case 'b':  plain = '\b'; break;
            case 'f':  plain = '\f'; break;
            case 'n':  plain = '\n'; break;
            case 'r':  plain = '\r'; break;
            case 't':  plain = '\t'; break;
            case 'u':
            {
                ++m_pos;
                unsigned code;
                if (!readHex4(code))
                    return false;
                if (code >= 0xDC00 && code <= 0xDFFF)
                    return fail("unpaired surrogate in a \\u escape");
                if (code >= 0xD800 && code <= 0xDBFF)
                {
                    unsigned low;
                    if (m_text.compare(m_pos, 2, "\\u") != 0)
                        return fail("unpaired surrogate in a \\u escape");
                    m_pos += 2;
                    if (!readHex4(low))
                        return false;
                    if (low < 0xDC00 || low > 0xDFFF)
                        return fail("unpaired surrogate in a \\u escape");
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                if (out)
                    appendUtf8(*out, code);
                continue;
            }
            default:
                return fail("unknown escape in a string");
            }
            if (out)
                *out += plain;
            ++m_pos;
        }
    }

    const std::string& m_text;
    std::string::size_type m_pos;
    std::string m_error;
    JsonType m_rootType;
    std::vector<JsonEntry> m_entries;
};

// The members of the object at index parent (-1: the top-level object).
std::vector<const JsonEntry*> membersOf(const std::vector<JsonEntry>& entries, int parent)
{
    std::vector<const JsonEntry*> members;
    for (std::size_t i = 0; i < entries.size(); ++i)
        if (entries[i].parent == parent)
            members.push_back(&entries[i]);
    return members;
}

// The first key that occurs a second time among members.
bool findDuplicate(const std::vector<const JsonEntry*>& members, std::string& key)
{
    std::set<std::string> seen;
    for (std::size_t i = 0; i < members.size(); ++i)
        if (!seen.insert(members[i]->key).second)
        {
            key = members[i]->key;
            return true;
        }
    return false;
}

const JsonEntry* findMember(const std::vector<const JsonEntry*>& members, const char* key)
{
    for (std::size_t i = 0; i < members.size(); ++i)
        if (members[i]->key == key)
            return members[i];
    return 0;
}

std::string labelList()
{
    std::string text;
    for (int i = 0; i < kLabelCount; ++i)
        text += std::string(i ? ", " : "") + kLabels[i].label;
    return text;
}

// The provider file's rules (OnlineProvider.h), checked in a fixed order: the JSON first, then
// "schema", so that a file of another schema is refused for its schema whatever else it holds.
bool parseConfig(const std::string& text, Config& result, std::string& error)
{
    if (text.size() > kMaxFileSize)
    {
        error = "larger than 64 KiB";
        return false;
    }
    JsonReader reader(text);
    if (!reader.readDocument(error))
        return false;
    if (reader.rootType() != JsonObject)
    {
        error = std::string("the file holds ") + typeName(reader.rootType()) + ", not a JSON object";
        return false;
    }
    const std::vector<JsonEntry>& entries = reader.entries();
    const std::vector<const JsonEntry*> top = membersOf(entries, -1);
    std::string duplicate;
    if (findDuplicate(top, duplicate))
    {
        error = "duplicate key " + quoted(duplicate);
        return false;
    }

    const JsonEntry* schema = findMember(top, "schema");
    if (!schema)
    {
        error = "\"schema\" is required";
        return false;
    }
    if (schema->type != JsonNumber || schema->text != "1")
    {
        error = "\"schema\" must be the number 1, this file has "
              + (schema->type == JsonNumber ? schema->text
                 : schema->type == JsonString ? "the string " + quoted(schema->text)
                 : std::string(typeName(schema->type)));
        return false;
    }

    if (const JsonEntry* provider = findMember(top, "provider"))
    {
        if (provider->type != JsonString)
        {
            error = std::string("\"provider\" must be a string, not ") + typeName(provider->type);
            return false;
        }
        for (std::string::size_type i = 0; i < provider->text.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(provider->text[i]);
            if (!((c < 0x80 && std::isalnum(c)) || c == ' ' || c == '-' || c == '.'))
            {
                error = "\"provider\" " + quoted(provider->text) + " may hold only letters, digits, spaces, '-' and '.'";
                return false;
            }
        }
        if (provider->text.size() > kMaxProviderName)
        {
            error = "\"provider\" is longer than 64 characters";
            return false;
        }
        result.provider = provider->text;
    }

    const JsonEntry* services = findMember(top, "services");
    if (!services)
    {
        error = "\"services\" is required";
        return false;
    }
    if (services->type != JsonObject)
    {
        error = std::string("\"services\" must be an object, not ") + typeName(services->type);
        return false;
    }
    const std::vector<const JsonEntry*> entriesOfServices = membersOf(entries, static_cast<int>(services - &entries[0]));
    if (entriesOfServices.empty())
    {
        error = "\"services\" has no entry";
        return false;
    }
    if (findDuplicate(entriesOfServices, duplicate))
    {
        error = "duplicate key " + quoted(duplicate) + " in \"services\"";
        return false;
    }
    for (std::size_t i = 0; i < entriesOfServices.size(); ++i)
    {
        const JsonEntry& entry = *entriesOfServices[i];
        int index = -1;
        if (entry.key != "*")
        {
            for (int l = 0; l < kLabelCount && index < 0; ++l)
                if (entry.key == kLabels[l].label)
                    index = l;
            if (index < 0)
            {
                error = "unknown key " + quoted(entry.key) + " in \"services\" (the keys are \"*\" and the labels "
                      + labelList() + ", spelled exactly)";
                return false;
            }
        }
        const std::string where = "services." + entry.key;
        if (entry.type != JsonString)
        {
            error = where + " must be a string, not " + typeName(entry.type);
            return false;
        }
        std::string reason;
        // A "*" domain must leave room for the longest name formed with it.
        const std::string::size_type longest =
            index < 0 && !isIPv4Address(entry.text) ? kMaxName - kLongestPrefix : kMaxName;
        if (!checkValue(entry.text, longest, reason))
        {
            error = where + ": " + reason;
            return false;
        }
        if (index < 0)
            result.fallback = entry.text;
        else
            result.labels[index] = entry.text;
    }
    return true;
}

} // namespace

bool loadConfigText(const std::string& text, const std::string& path, std::string& error)
{
    Config loaded;
    if (!parseConfig(text, loaded, error))
        return false;
    loaded.path = path;
    config() = loaded;
    return true;
}

bool loadConfigFile(const std::string& path, std::string& error)
{
    if (path.empty())
    {
        error = "a file path is required";
        return false;
    }
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
    {
        error = std::string("cannot be opened (") + std::strerror(errno) + ")";
        return false;
    }
    // Reading stops one block past the limit; parseConfig then refuses the size.
    std::string text;
    char buffer[4096];
    std::size_t n;
    while (text.size() <= kMaxFileSize && (n = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        text.append(buffer, n);
    const bool failed = std::ferror(file) != 0;
    const int readError = errno;
    std::fclose(file);
    if (failed)
    {
        error = std::string("cannot be read (") + std::strerror(readError) + ")";
        return false;
    }
    return loadConfigText(text, path, error);
}

int labelCount()
{
    return kLabelCount;
}

const char* label(int index)
{
    return index >= 0 && index < kLabelCount ? kLabels[index].label : "";
}

int labelOf(const std::string& name)
{
    const std::string lower = lowerCase(name);
    const char* domain = retailDomain(lower);
    if (!domain)
        return -1;
    const std::string::size_type end = prefixLength(lower, domain);
    if (end == 0 || end == std::string::npos)
        return -1;
    const std::string::size_type dot = lower.rfind('.', end - 1);
    const std::string::size_type start = dot == std::string::npos ? 0 : dot + 1;
    const std::string labelText = lower.substr(start, end - start);
    for (int i = 0; i < kLabelCount; ++i)
        if (kLabels[i].domain == domain && labelText == lowerCase(kLabels[i].label))
            return i;
    return -1;
}

bool hasRedirect()
{
    return !config().path.empty();
}

std::string redirectLookup(const std::string& name, std::string* source)
{
    if (source)
        source->clear();
    const Config& current = config();
    const int index = labelOf(name);
    if (index >= 0 && !current.labels[index].empty())
    {
        if (source)
            *source = std::string("services.") + kLabels[index].label;
        return current.labels[index];
    }
    const std::string& fallback = current.fallback;
    const std::string lower = lowerCase(name);
    const char* domain = retailDomain(lower);
    if (fallback.empty() || (lower.find("gamespy") == std::string::npos && domain != kDiceDomain))
        return std::string();
    std::string lookup;
    if (isIPv4Address(fallback))
        lookup = fallback;
    else if (domain)
    {
        // The name as formed, with its retail domain replaced: gpcm.gamespy.com -> gpcm.<D>.
        const std::string::size_type prefix = prefixLength(lower, domain);
        lookup = prefix ? name.substr(0, prefix) + "." + fallback : fallback;
    }
    else
        return std::string();       // contains "gamespy" but has no retail domain to replace
    if (source)
        *source = "services.*";
    return lookup;
}

std::string describe()
{
    if (!hasRedirect())
        return "online services: retail names, resolved unchanged";
    const Config& current = config();
    std::string line("online services: ");
    if (!current.provider.empty())
        line += "provider " + current.provider + ", ";
    line += "file " + current.path + "; retail names";
    for (int i = 0; i < kLabelCount; ++i)
        if (!current.labels[i].empty())
            line += std::string("; ") + kLabels[i].label + " names resolve as "
                  + current.labels[i] + " (services." + kLabels[i].label + ")";
    if (!current.fallback.empty())
    {
        line += "; other names containing gamespy or under dice.se resolve ";
        line += isIPv4Address(current.fallback) ? "to " + current.fallback
                                                : "with their retail domain replaced by " + current.fallback;
        line += " (services.*)";
    }
    return line;
}

std::string commandLineArguments()
{
    const std::string& path = config().path;
    if (path.empty())
        return std::string();
    // SimpleParser::getKey ends an unquoted value at white space, and reads a value that starts
    // with '"' up to the next '"'.
    bool quote = path[0] == '"';
    for (std::string::size_type i = 0; i < path.size() && !quote; ++i)
        quote = std::isspace(static_cast<unsigned char>(path[i])) != 0;
    return quote ? " +onlineProviderConfig \"" + path + "\"" : " +onlineProviderConfig " + path;
}

} // namespace onlineProvider
} // namespace hfe
} // namespace dice
