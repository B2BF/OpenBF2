// All instructions read from BF2.exe. No matching dedicated ELF Data class.
#include "Data.h"
#if !DEDICATED
#include "DataFormatting.h"
#include "TypeDescriptor.h"
#include "System.h"
#include "Stream.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#if !defined(_M_IX86)
#include <emmintrin.h>
#endif
namespace dice { namespace meme {
namespace {
char* cloneName(const char* name)
{
    if (!name) return 0;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
class DataTypeDescriptor : public ClassTypeDescriptor<Data, Object>
{
public:
    DataTypeDescriptor() : ClassTypeDescriptor<Data, Object>("dice::meme::Data") {}
    RuntimeObject* create() override // 0x829CB0 -> 0x737D60 (ordinary operator new)
    {
        void* allocation = ::operator new(sizeof(Data));
        return allocation ? new(allocation) Data(0) : 0;
    }
};
class StringDataTypeDescriptor : public ClassTypeDescriptor<StringData, Data>
{
public:
    StringDataTypeDescriptor() : ClassTypeDescriptor<StringData, Data>("dice::meme::StringData") {}
};
}

// Formats by truncation, preserving the x87 remainder across fractional digits.
// Whole and fractional conversions use different integer widths and overflow behavior.
void formatStringDataFloat(float value, char* output, int digits)
{
    double magnitude = value;
    char* cursor = output;
    if (value < 0.0f) {
        *cursor++ = '-';
        magnitude = -magnitude;
    }
#if defined(_M_IX86)
    unsigned short oldControl, truncControl;
    __int64 whole;
    int integral;
    __asm {
        fnstcw oldControl
        mov ax, oldControl
        or ax, 0C00h
        mov truncControl, ax
        fld magnitude
        fldcw truncControl
        fistp whole
        fldcw oldControl
        // Keep the low-dword store visible to the following x87 block. MSVC
        // /O2 can elide a C++ memcpy into this asm-only memory operand.
        mov eax, dword ptr whole
        mov integral, eax
    }
#else
    // cvttsd2si truncates like the chopped fistp and yields the same integer-indefinite
    // value for NaN and out-of-range input; the low dword is kept, as above.
    const int integral = static_cast<int>(_mm_cvttsd_si64(_mm_set_sd(magnitude)));
#endif
    int remaining = integral;
    if (remaining == 0) {
        *cursor++ = '0';
    } else {
        char reverse[32];
        int length = 0;
        do {
            reverse[length++] = static_cast<char>(remaining % 10 + '0');
            remaining /= 10; // signed idiv; overflow-derived negative digits stay negative
        } while (remaining != 0);
        while (length != 0) *cursor++ = reverse[--length];
    }
    *cursor++ = '.';
#if defined(_M_IX86)
    const double ten = 10.0;
    int digit;
    __asm {
        fld magnitude
        fisub integral
        mov edi, cursor
        mov ecx, digits
    string_data_digit:
        fmul ten
        fld st(0)
        fldcw truncControl
        fistp digit
        fldcw oldControl
        mov eax, digit
        add al, 30h
        mov byte ptr [edi], al
        inc edi
        fisub digit
        dec ecx
        jnz string_data_digit
        fstp st(0)
        mov cursor, edi
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // The dec/jnz counter is unsigned: a nonpositive digit count runs 2^32 times, as shipped.
    {
        double fraction = magnitude - static_cast<double>(integral);
        unsigned int count = static_cast<unsigned int>(digits);
        do {
            fraction *= 10.0;
            const int digit = _mm_cvttsd_si32(_mm_set_sd(fraction));
            *cursor++ = static_cast<char>(static_cast<unsigned char>(digit) + 0x30);
            fraction -= static_cast<double>(digit);
        } while (--count != 0);
    }
#endif
    *cursor = 0;
}

// Plain sprintf through literal89E620 ("%i"); no width, precision or
// buffer bound. Every caller reserves twelve bytes, which INT_MIN fills exactly.
void formatStringDataInt(int value, char* output)
{
    std::sprintf(output, "%i", value);
}
Data::Data(const char* name) : m_name(cloneName(name)) {}
Data::~Data() { ::operator delete(m_name); m_name = 0; }
TypeDescriptor* Data::getClassType()
{
    static DataTypeDescriptor type;
    return &type;
}
TypeDescriptor* Data::getType() { return getClassType(); }
unsigned int Data::getObjectSize() { return sizeof(Data); }
const char* Data::getName() { return m_name; }
bool Data::setName(const char* name)
{
    ::operator delete(m_name);
    m_name = 0;
    m_name = cloneName(name); // Original frees first; self-aliasing is unsafe.
    return true;
}
bool Data::slot_0x20() { return false; }
bool Data::slot_0x24() { return false; }
bool Data::onCreate(System*) { return true; }
bool Data::streamProperties(System*, Stream*) { return true; }
bool Data::setFloatElement(System* system, int, float value)
{ setFloat(system, value); return true; }
bool Data::setWideStringElement(System* system, int, const wchar_t* value)
{ setWideString(system, value); return true; }
bool Data::setStringElement(System* system, int, const char* value)
{ setString(system, value); return true; }
bool Data::setFloat(System*, float) { return true; } // Exact 0x4553F0 default.
bool Data::setWideString(System* system, const wchar_t* value)
{
    char buffer[512];
    int length = 0;
    while (value[length]) {
        if (length == 511) {
            systemPrintf(system, "Meme: Warning: Data::set, string is longer than buffer\r\n");
            break;
        }
        buffer[length] = static_cast<char>(value[length]);
        ++length;
    }
    buffer[length] = 0;
    setString(system, buffer);
    return true;
}
bool Data::setString(System*, const char*) { return true; } // Exact 0x4553F0 default.
float Data::getFloatElement(System* system, int) { return getFloat(system); }
float Data::getFloat(System*) { return 0.0f; } // Exact 0x829A30 default.
int Data::getCharacterElement(System* system, int, int character)
{ return getCharacter(system, character); }
int Data::getCharacter(System*, int) { return 0; } // Exact 0x6212F0 default.
int Data::getElementCount(System*) { return 1; }
Data* Data::getElement(int index) { return index == 0 ? this : 0; }
int Data::getString(System* system, char* buffer, int capacity)
{
    for (int i = 0; i < capacity; ++i) {
        const char value = static_cast<char>(getCharacter(system, i));
        buffer[i] = value;
        if (!value) return i;
    }
    buffer[capacity - 1] = 0; // Shipped missing guard for nonpositive capacity.
    return capacity;
}
int Data::getWideString(System* system, wchar_t* buffer, int capacity)
{
    for (int i = 0; i < capacity; ++i) {
        const wchar_t value = static_cast<wchar_t>(getCharacter(system, i));
        buffer[i] = value;
        if (!value) return i;
    }
    buffer[capacity - 1] = 0;
    return capacity;
}
int Data::getStringLength(System* system) // 829BA0; no character narrowing or bound
{
    int length = 0;
    while (getCharacter(system, length) != 0) ++length;
    return length;
}

StringData::StringData(const char* value, const char* name)
    : Data(name), m_value(cloneName(value)) {}
StringData::~StringData() // 8297C0; scalar delete, then clear, then base destruction
{ ::operator delete(m_value); m_value = 0; }
TypeDescriptor* StringData::getClassType() // 8298A0 / descriptor949628
{
    static StringDataTypeDescriptor type;
    return &type;
}
TypeDescriptor* StringData::getType() { return getClassType(); }
unsigned int StringData::getObjectSize() { return sizeof(StringData); }
bool StringData::streamProperties(System* system, Stream* stream)
{
    if (!Data::streamProperties(system, stream)) return false;
    return stream->streamString(&m_value, "String <do not edit>") != 0;
}
bool StringData::setString(System*, const char* value)
{
    ::operator delete(m_value);
    m_value = 0;
    m_value = cloneName(value); // Retail releases before cloning; aliasing remains unsafe.
    return true;
}
bool StringData::setFloat(System* system, float value)
{
    char buffer[32];
    formatStringDataFloat(value, buffer);
    setString(system, buffer); // The overridden setter's result is discarded.
    return true;
}
float StringData::getFloat(System*) // 829270 ->737F70 ->CRT atof
{ return m_value ? static_cast<float>(std::atof(m_value)) : 0.0f; }
int StringData::getCharacter(System*, int character) // movzx, not movsx
{ return m_value ? static_cast<unsigned char>(m_value[character]) : 0; }

FloatRefData::FloatRefData(float* value, const char* name)
    : Data(name), m_value(value), m_cachedValue(0), m_text(nullptr)
{
    // Retail aliases the value to its formatting cache when no external value
    // is supplied. After first formatting, setters can leave the text stale.
    if (!m_value) m_value = &m_cachedValue;
}
FloatRefData::~FloatRefData() { ::operator delete(m_text); m_text = nullptr; }
TypeDescriptor* FloatRefData::getClassType()
{
    static ClassTypeDescriptor<FloatRefData, Data> type("dice::meme::FloatRefData");
    return &type;
}
TypeDescriptor* FloatRefData::getType() { return getClassType(); }
unsigned int FloatRefData::getObjectSize() { return sizeof(FloatRefData); }
bool FloatRefData::setFloat(System*, float value) { *m_value = value; return true; }
bool FloatRefData::setString(System* system, const char* value)
{ return setFloat(system, static_cast<float>(std::atof(value))); }
float FloatRefData::getFloat(System*) { return *m_value; }
int FloatRefData::getCharacter(System*, int character)
{
    if (!m_text) {
        // Retail allocates only18 bytes and has no output or index bound.
        m_text = static_cast<char*>(::operator new(18));
        formatStringDataFloat(*m_value, m_text, 9);
        m_cachedValue = *m_value;
    } else if (*m_value != m_cachedValue) {
        m_cachedValue = *m_value;
        formatStringDataFloat(*m_value, m_text, 9);
    }
    return static_cast<signed char>(m_text[character]);
}
BoolRefData::BoolRefData(bool* value, const char* name)
    : Data(name), m_value(value), m_fallback(false)
{ if (!value) m_value = &m_fallback; }
bool BoolRefData::setFloat(System*, float value)
{ *m_value = value == 1.0f; return true; }
bool BoolRefData::setString(System*, const char* value)
{ *m_value = _stricmp(value, "true") == 0 || std::strcmp(value, "1") == 0; return true; }
float BoolRefData::getFloat(System*)
{
    // PE compares the pointed byte to exactly one. A noncanonical bool byte may
    // exist in retail HUD storage; don't replace this with a nonzero test.
    return *reinterpret_cast<unsigned char*>(m_value) == 1 ? 1.0f : 0.0f;
}
int BoolRefData::getCharacter(System*, int index)
{
    // Helper 0x82DD30 uses a nonzero test, unlike getFloat's exact-one test.
    const bool value = *reinterpret_cast<unsigned char*>(m_value) != 0;
    if (index < 0 || index >= (value ? 4 : 5)) return 0;
    return (value ? L"true" : L"false")[index];
}
bool BoolRefData::bind(System*, bool* value)
{ m_value = value ? value : &m_fallback; return true; }

namespace {
// the wide-character allocation, widening copy and
// clone the wide reference class uses. Names [INFERRED].
wchar_t* allocateWide(unsigned int characters)
{ return static_cast<wchar_t*>(::operator new(characters * 2)); }
void widenInto(wchar_t* output, const char* text)
{
    while (*text) *output++ = static_cast<unsigned char>(*text++);
    *output = 0;
}
wchar_t* cloneWide(const wchar_t* text)
{
    if (!text) return nullptr;
    unsigned int length = 0;
    while (text[length]) ++length;
    wchar_t* copy = allocateWide(length + 1);
    for (unsigned int i = 0; i <= length; ++i) copy[i] = text[i];
    return copy;
}
// the wide answer to atof; it walks the wide digits itself.
float wideToFloat(const wchar_t* text)
{
    char narrow[64];
    unsigned int i = 0;
    while (i != 63 && text[i]) { narrow[i] = static_cast<char>(text[i]); ++i; }
    narrow[i] = 0;
    return static_cast<float>(std::atof(narrow));
}
}

StringRefData::StringRefData(const char** value, const char* name)
    : Data(name), m_value(value), m_fallback(nullptr)
{
    // The same aliasing the other reference classes use: with no slot the
    // pointer is aimed at the object's own storage so nothing is null.
    if (!m_value) m_value = &m_fallback;
}
StringRefData::~StringRefData() {}                          // 829800, owns nothing
TypeDescriptor* StringRefData::getClassType()
{
    static ClassTypeDescriptor<StringRefData, Data> type("dice::meme::StringRefData");
    return &type;
}
TypeDescriptor* StringRefData::getType() { return getClassType(); }
unsigned int StringRefData::getObjectSize() { return sizeof(StringRefData); }
bool StringRefData::setString(System*, const char* value)
{
    // The slot's current text is freed and replaced with this object's own
    // clone, so the information layer hands over ownership on the first write.
    ::operator delete(const_cast<char*>(*m_value));
    *m_value = cloneName(value);
    return true;
}
bool StringRefData::setFloat(System* system, float value)
{
    char text[32];
    formatStringDataFloat(value, text);  // seven fractional digits
    return setString(system, text);      // through the virtual, as in retail
}
float StringRefData::getFloat(System*)
{ return *m_value ? static_cast<float>(std::atof(*m_value)) : 0.0f; }
int StringRefData::getCharacter(System*, int character)
{
    if (!*m_value) return 0;
    return static_cast<unsigned char>((*m_value)[character]); // movzx, unchecked index
}

WstringRefData::WstringRefData(const wchar_t** value, const char* name)
    : Data(name), m_value(value), m_fallback(nullptr)
{
    if (!m_value) m_value = &m_fallback;
}
WstringRefData::~WstringRefData() {}                        // 829830, owns nothing
TypeDescriptor* WstringRefData::getClassType()
{
    static ClassTypeDescriptor<WstringRefData, Data> type("dice::meme::WstringRefData");
    return &type;
}
TypeDescriptor* WstringRefData::getType() { return getClassType(); }
unsigned int WstringRefData::getObjectSize() { return sizeof(WstringRefData); }
bool WstringRefData::setString(System*, const char* value)
{
    ::operator delete(const_cast<wchar_t*>(*m_value));
    wchar_t* widened = allocateWide(
        static_cast<unsigned int>(std::strlen(value)) + 1);
    *m_value = widened;
    widenInto(widened, value);
    return true;
}
bool WstringRefData::setWideString(System*, const wchar_t* value)
{
    ::operator delete(const_cast<wchar_t*>(*m_value));
    *m_value = cloneWide(value);
    return true;
}
bool WstringRefData::setFloat(System* system, float value)  // 829360, the shared body
{
    char text[32];
    formatStringDataFloat(value, text);
    return setString(system, text);
}
float WstringRefData::getFloat(System*)
{ return *m_value ? wideToFloat(*m_value) : 0.0f; }
int WstringRefData::getCharacter(System*, int character)
{
    if (!*m_value) return 0;
    return static_cast<unsigned short>((*m_value)[character]); // movzx, unchecked
}

WstringData::WstringData(const wchar_t* value, const char* name)
    : Data(name), m_value(cloneWide(value)) {}
WstringData::~WstringData()
{
    ::operator delete(m_value);
    m_value = nullptr;
}
TypeDescriptor* WstringData::getClassType()
{
    static ClassTypeDescriptor<WstringData, Data> type("dice::meme::WstringData");
    return &type;
}
TypeDescriptor* WstringData::getType() { return getClassType(); }
unsigned int WstringData::getObjectSize() { return sizeof(WstringData); }
bool WstringData::streamProperties(System* system, Stream* stream)
{
    if (!Data::streamProperties(system, stream)) return false;
    return stream->streamWideString(&m_value, "Wstring <do not edit>") != false;
}
bool WstringData::setString(System*, const char* value)
{
    ::operator delete(m_value);
    m_value = allocateWide(static_cast<unsigned int>(std::strlen(value)) + 1);
    widenInto(m_value, value);
    return true; // the shipped body answers true without checking anything
}
bool WstringData::setWideString(System*, const wchar_t* value)
{
    ::operator delete(m_value);
    m_value = cloneWide(value);
    return true;
}
bool WstringData::setFloat(System* system, float value)
{
    char text[32];
    formatStringDataFloat(value, text);
    setString(system, text);
    return true; // the setter's own result is discarded here
}
float WstringData::getFloat(System*)
{ return wideToFloat(m_value); } // no null guard, unlike the reference twin
int WstringData::getCharacter(System*, int character)
{
    if (!m_value) return 0;
    return static_cast<unsigned short>(m_value[character]); // movzx, unchecked
}

IntRefData::IntRefData(int* value, const char* name)
    : Data(name), m_value(value), m_cachedValue(0), m_text(nullptr)
{
    // Same aliasing quirk as FloatRefData: with no external value the pointer
    // is aimed at the formatting cache, so the first getCharacter can never
    // observe a difference and the text is formatted exactly once.
    if (!m_value) m_value = &m_cachedValue;
}
IntRefData::~IntRefData() { ::operator delete(m_text); m_text = nullptr; }
bool IntRefData::setFloat(System*, float value)
{
    // Round half away from zero, then the ordinary truncating x86 cast
    // (fnstcw / or 0C00h / fistp / fldcw). A NaN takes the negative arm.
    *m_value = static_cast<int>(value < 0.0f ? value - 0.5f : value + 0.5f);
    return true;
}
bool IntRefData::setString(System* system, const char* value) // 82E110, shared body
{ return setFloat(system, static_cast<float>(std::atof(value))); }
float IntRefData::getFloat(System*) { return static_cast<float>(*m_value); }
int IntRefData::getCharacter(System*, int character)
{
    if (!m_text) {
        // Retail reserves only twelve bytes, without bounding the output.
        m_text = static_cast<char*>(::operator new(12));
        formatStringDataInt(*m_value, m_text);
        m_cachedValue = *m_value;
    } else if (*m_value != m_cachedValue) {
        m_cachedValue = *m_value;
        formatStringDataInt(*m_value, m_text);
    }
    // The index is unchecked and the narrow byte is sign-extended.
    return static_cast<signed char>(m_text[character]);
}
} }
#endif
