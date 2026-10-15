// Tokenizer implementation. [CHOSEN, not recovered] Built only into the RendDX9 image.
// File buffers use the engine array allocator and must be released through its matching free function.
#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include <string>
#include "Tokenizer.h"

#include <cstddef>

namespace dice {
namespace hfe {

namespace {

// Select the file-info representation required by the host ABI.

// The native host's own record - this image shares the host's STL and CRT.
typedef ::dice::hfe::io::FileInfo TokenizerFileInfo;

inline ::dice::hfe::io::FileInfo& hostFileInfo(TokenizerFileInfo& info)
{
    return info;
}


// End-of-input token text: ASCII ETX followed by the string terminator.
const char kEndTokenText[] = "\x03";

} // anonymous namespace

// owner 0, index -1, the text constructed empty, line and column 0xFFFF, type 0, in
// declaration order. RendDX9 inlines it (0x1007688F.. in clean(), 0x10076913.. in the scan).
Token::Token()
    : owner(0),
      index(0xFFFFFFFFu),
      line(0xFFFF),
      column(0xFFFF),
      type(0)
{
}

// Advances to the next token and returns this cursor. Advancing the end token leaves it at the end.
Token& Token::operator++()
{
    Tokenizer* const tokenizer = owner;
    if (index < tokenizer->m_count)
        ++index;
    *this = tokenizer->m_tokens[index];
    return *this;
}

// ELF 0x8438C90 / sub_10076860, 27 B - see the header: the vector empty, the name empty, the count
// left unset.
Tokenizer::Tokenizer()
{
}

// Clears scanner state and releases token and filename storage.
Tokenizer::~Tokenizer()
{
    clean();
}

// Clears tokens and count, retaining capacity. The file name becomes a one-character NUL string.
void Tokenizer::clean()
{
    m_fileName = '\0';
    m_tokens.resize(0);
    m_count = 0;
}

// Loads and tokenizes fileName through FileManager. Returns the scan result.
// Requires FileManager; records the name after successful file access even when scanning fails.
bool Tokenizer::init(const char* fileName)
{
    TokenizerFileInfo info;
    if (!::dice::hfe::fileManager->getFileInfo(::renddx9abi::HostString(fileName), hostFileInfo(info)))
        return false;

    char* const buffer = static_cast<char*>(::renddx9abi::engineAlloc(info.size));
    unsigned int bytesRead;
    if (!::dice::hfe::fileManager->readFile(::renddx9abi::HostString(fileName), buffer, info.size,
                                            bytesRead))
    {
        ::renddx9abi::engineFree(buffer);
        return false;
    }

    const bool ok = init(buffer, bytesRead);
    ::renddx9abi::engineFree(buffer);
    m_fileName = fileName;
    return ok;
}

// Scans text into tokens and an end sentinel, preserving the retail lexical rules.
// Newlines inside quotes fail after retaining already emitted tokens; carriage returns are discarded.
// Slash operators append to the current word before flushing, and same-line comments can join words.
// Only the in-scan double-quote flush preserves empty text. The final flush does not advance the
// index, and the sentinel keeps the previous token type. Preserve quote and escape handling below.
bool Tokenizer::init(const char* data, unsigned int size)
{
    clean();                                                       // sub_10076880

    Token token;
    token.owner = this;
    unsigned int tokenIndex = 0;
    unsigned int line = 1;
    unsigned int column = 1;
    bool inQuote = false;
    bool startToken = true;
    bool escaped = false;
    bool isOperator = false;

    for (unsigned int i = 0; i < size; ++i)
    {
        const char c = data[i];
        bool append = false;
        const char next = (i + 1 == size) ? '\0' : data[i + 1];

        if (startToken)
        {
            startToken = false;
            if (token.text.size() != 0 || token.type == 3)         // "" is kept, other empties are not
            {
                m_tokens.push_back(token);
                ++m_count;
                ++tokenIndex;
                token.text.clear();                                // MSVCP71 clear(), IAT 0x101DB24C
            }
            token.line = static_cast<unsigned short>(line);
            token.index = tokenIndex;
            token.column = static_cast<unsigned short>(column);
            token.type = 1;
        }

        ++column;                                                  // before the switch

        switch (c)
        {
        case '\t':
        case ' ':
            if (inQuote)
                append = true;
            else
                startToken = true;
            break;

        case '\n':
            if (inQuote)
                return false;                                      // the scan fails
            column = 1;
            ++line;
            startToken = true;
            break;

        case '\r':                                                 // dropped
            break;

        case '!': case '%': case '&': case '*': case '+': case '-':
        case '<': case '=': case '>': case '^': case '|': case '~':
            isOperator = true;
            // fall through
        case '(': case ')': case ',': case '.': case ':':
        case ';': case '[': case ']': case '{': case '}':
            if (token.text.size() != 0)
            {
                if (!inQuote)
                {
                    // Close the word first: step back and scan this character again as the start
                    // of its own token (0x10076A21: `sub edi, 1; sub esi, 1`).
                    --column;
                    --i;
                    startToken = true;
                }
                else
                {
                    append = true;
                }
            }
            else
            {
                append = true;
                if (!inQuote)
                {
                    startToken = true;
                    token.type = isOperator ? 5 : 4;               // `setne dl; add edx, 4`
                }
            }
            isOperator = false;
            break;

        case '"':
        case '\'':
            if (c == '\'' && inQuote && token.type == 3)
            {
                append = true;                                     // a ' inside "..." is text
                break;
            }
            if (inQuote)
            {
                if (escaped)
                {
                    append = true;                                 // an escaped quote is text
                    break;
                }
                startToken = true;                                 // the closing quote ends the token
            }
            inQuote = !inQuote;                                    // `sete al` over the old flag
            token.type = (c == '\'') ? 2 : 3;                      // either quote closes either kind
            break;

        case '/':
            if (inQuote)
            {
                append = true;
                break;
            }
            if (next == '*')
            {
                // a block comment: past "/*", on to the "*/", counting its lines.
                i += 2;
                const unsigned int last = size - 1;
                while (i < last)
                {
                    const char ch = data[i];
                    if (ch == '\n')
                    {
                        column = 1;
                        ++line;
                        startToken = true;
                    }
                    if (ch == '*' && data[i + 1] == '/')
                    {
                        ++i;                                       // on the closing '/'
                        break;
                    }
                    ++i;
                    ++column;                                      // also right after a '\n'
                }
                break;
            }
            if (next == '/')
            {
                // a line comment: on to the '\n', which is then taken as the line
                // break (0x10076A00); at the end of the data the scan simply ends.
                while (i < size)
                {
                    if (data[i] == '\n')
                    {
                        column = 1;
                        ++line;
                        startToken = true;
                        break;
                    }
                    ++i;
                }
                break;
            }
            append = true;                                         // onto the current token,
            startToken = true;                                     //   which it then ends
            token.type = 5;
            break;

        case '\\':
            append = true;
            escaped = true;
            break;

        default:
            append = true;
            break;
        }

        // the tail every character takes.
        if (c != '\\')
            escaped = false;
        if (append)
            token.text += c;                                       // MSVCP71 operator+=(char), IAT 0x101DB1A4
    }

    // the last token (not an empty "", and the token counter is not advanced).
    if (token.text.size() != 0)
    {
        m_tokens.push_back(token);
        ++m_count;
    }

    // Append an uncounted end token on the next line, column one, retaining its current type.
    token.line = static_cast<unsigned short>(line + 1);
    token.index = tokenIndex;
    token.column = 1;
    token.text = kEndTokenText;
    m_tokens.push_back(token);
    return true;
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
