// Queued debug text, flushed by the writer manager before the device ends its scene.
// [INFERRED] Source placement.
// [RETIRE-WHEN: RW-58] Confirm the original source file before removing the placement uncertainty.

// The proportional and fixed-width writers share this class; the loaded font selects the appearance.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <list>
#include <vector>

namespace dice {
namespace hfe {
namespace geom {

// Class and interface IDs used by module registration and queryInterface.
extern const unsigned int IID_IDebugTextWriter = 80043u;
extern const unsigned int CID_DebugTextWriter  = 81041u;

// The DebugTextWriterManager global, dword_10247C58. Declared in ModuleGeomMisc.cpp.
extern IBase* g_DebugTextWriterManager;

// FontSystem/FontSystem.cpp - the constructor builds one directly, not through the registry.
IBase* createFontPortedDirect();
bool fontPortedLoad(IBase*, const vc71::string&);
int fontPortedGetHeight(IBase*);
int fontPortedGetTextWidth(IBase*, const vc71::string&);
unsigned fontPortedGetColour(IBase*);
void fontPortedSetColour(IBase*, unsigned);
void fontPortedDrawText(IBase*, float, float, const vc71::string&);
class DebugTextWriter;
void debugTextWriterManagerAdd(DebugTextWriter*);
void debugTextWriterManagerRemove(DebugTextWriter*);

struct DebugTextLine
{
    int x, y;
    vc71::string text;
    unsigned colour;
    int alignment;
    DebugTextLine(int px, int py, unsigned c, const vc71::string& s, int a)
        : x(px), y(py), text(s), colour(c), alignment(a) {}
};

struct DebugTextGroup
{
    int x, y;
    std::vector<vc71::string> lines;
    int spacing;
    unsigned colour;
    int alignment, bottomAligned;
};


// dice::hfe::geom::DebugTextWriter - 0x34 bytes (52), vtable 0x1020CD10 with 19 slots.

// The slot count is read: the table runs 0x1020CD10..0x1020CD58 and the next dword is 0.

class DebugTextWriter : public IBase
{
public:
    DebugTextWriter();
    ~DebugTextWriter();

    virtual void  addRef();                                   // +0x00  0x1000CB20 (folded)
    virtual int   getRef();                                   // +0x04  0x101A9A10 (folded)
    virtual int   release();                                  // +0x08
    virtual void* queryInterface(unsigned int iid) const;     // +0x0C  0x1009BCD0   read

    virtual bool setFont(const vc71::string& path);           // +0x10  0x1009B160   read
    virtual bool isEnabled() const;
    virtual void setEnabled(bool enabled);
    virtual unsigned getColour() const;
    virtual void setColour(unsigned colour);
    virtual void slot_0x24();
    virtual void slot_0x28();
    virtual void slot_0x2C();
    virtual void queueTextColoured(int x, int y, unsigned colour, const vc71::string& text, int alignment);
    virtual void slot_0x34();
    virtual void slot_0x38();
    virtual void slot_0x3C();
    virtual void queueText(int x, int y, const vc71::string& text, int alignment);
    virtual void draw();
    virtual void clear();

private:
    int            m_refCount;      // +0x04  ctor: 1
    void*          m_font;          // +0x08  a geom::Font::Ported, 0xC58 bytes - see below
    unsigned int   m_vectorPadding; // allocator island; native vectors keep the following offsets
    std::vector<DebugTextLine*> m_lines;
    unsigned int   m_groupPadding;
    std::vector<DebugTextGroup*> m_groups;
    unsigned char  m_flag2C;        // +0x2C  ctor: 1
    unsigned char  m_fontLoaded;    // +0x2D  ctor: 0; setFont writes 1 on success, 0 on failure
    unsigned char  m_pad2E[2];
    unsigned int   m_colour;        // +0x30  ctor: 0xFF000000 - opaque black, argb
};

static_assert(sizeof(DebugTextWriter) == 0x34,
              "RefFactory_geom_DebugTextWriter allocates 0x34 - see 0x1007CE00");


// geom_DebugTextWriter_ctor, 0x1009B7C0.

DebugTextWriter::DebugTextWriter()
    : m_refCount(1)
    , m_font(0)
    , m_vectorPadding(0)
    , m_groupPadding(0)
    , m_flag2C(1)
    , m_fontLoaded(0)
    , m_colour(0xFF000000u)     // the ctor stores -16777216
{
    m_pad2E[0] = 0;
    m_pad2E[1] = 0;

    // The original does
    //       v2 = bf_malloc(0xC58); if (v2) v3 = geom_Font_Ported_ctor(v2); else v3 = 0;
    //       *(this + 2) = v3;
    // and Font::Ported now exists - see FontSystem/FontSystem.cpp.
    m_font = createFontPortedDirect();

    // The manager registration is reproduced, guard included: `if (dword_10247C58) manager->+0x10(this)`.
    // The manager owns its registration list, but does not retain the writer itself.
    if (g_DebugTextWriterManager)
    {
        debugTextWriterManagerAdd(this);
    }
}

void DebugTextWriter::addRef()
{
    ++m_refCount;
}

int DebugTextWriter::getRef()
{
    return m_refCount;
}

int DebugTextWriter::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~DebugTextWriter();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// Two ids, like the other leaf classes in this image.
void* DebugTextWriter::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IDebugTextWriter)
        return const_cast<DebugTextWriter*>(this);
    return 0;
}


// Loads the requested font and stores its ready state. Returns false when no font exists or loading fails.

bool DebugTextWriter::setFont(const vc71::string& path)
{
    RD9_TRACE("dice::hfe::geom::DebugTextWriter::setFont", 0x10);

    if (!m_font)
    {
        // Cannot happen now that the constructor builds one; kept because the original tolerates a
        // failed allocation here and this is that arm.
        m_fontLoaded = 0;
        return false;
    }

    // font->vslot+0x10(path), by raw offset as the original does it.
    void** vtbl = *reinterpret_cast<void***>(m_font);
    typedef bool (__thiscall* LoadFn)(void*, const vc71::string&);
    const bool ok = reinterpret_cast<LoadFn>(vtbl[0x10 / 4])(m_font, path);

    m_fontLoaded = ok ? 1 : 0;
    return ok;
}

DebugTextWriter::~DebugTextWriter()
{
    clear();
    static_cast<IBase*>(m_font)->release();
    if (g_DebugTextWriterManager)
        debugTextWriterManagerRemove(this);
}

bool DebugTextWriter::isEnabled() const { return m_flag2C != 0; }
void DebugTextWriter::setEnabled(bool enabled)
{
    m_flag2C = enabled;
    if (!enabled)
        clear();
}
unsigned DebugTextWriter::getColour() const { return m_colour; }
void DebugTextWriter::setColour(unsigned colour) { m_colour = colour; }

void DebugTextWriter::queueTextColoured(int x, int y, unsigned colour,
                                      const vc71::string& text, int alignment)
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugTextLine));
    DebugTextLine* line = storage ? new (storage) DebugTextLine(x, y, colour, text, alignment) : 0;
    m_lines.push_back(line);
}

void DebugTextWriter::queueText(int x, int y, const vc71::string& text, int alignment)
{
    queueTextColoured(x, y, m_colour, text, alignment);
}

void DebugTextWriter::draw()
{
    IBase* const font = static_cast<IBase*>(m_font);
    if (!m_fontLoaded)
        m_fontLoaded = fontPortedLoad(font, vc71::string("Fonts/BF1942.font"));
    if (!m_fontLoaded)
        return;
    const unsigned previousColour = fontPortedGetColour(font);
    for (DebugTextLine* line : m_lines)
    {
        const int offset = line->alignment == 1 ? -fontPortedGetTextWidth(font, line->text) : 0;
        fontPortedSetColour(font, line->colour);
        fontPortedDrawText(font, static_cast<float>(line->x + offset),
                          static_cast<float>(line->y), line->text);
    }
    for (DebugTextGroup* group : m_groups)
    {
        const int advance = fontPortedGetHeight(font) + group->spacing + 1;
        const int top = group->bottomAligned ? -advance * static_cast<int>(group->lines.size()) : 0;
        int y = 0;
        for (const vc71::string& line : group->lines)
        {
            const float offset = group->alignment == 1 ? -static_cast<float>(fontPortedGetTextWidth(font, line)) : 0.0f;
            fontPortedSetColour(font, group->colour);
            fontPortedDrawText(font, static_cast<float>(group->x) + offset,
                              static_cast<float>(group->y + y + top), line);
            y += advance;
        }
    }
    fontPortedSetColour(font, previousColour);
    clear();
}

void DebugTextWriter::clear()
{
    for (DebugTextLine* line : m_lines)
        if (line) { line->~DebugTextLine(); ::renddx9abi::engineFree(line); }
    for (DebugTextGroup* group : m_groups)
        if (group) { group->~DebugTextGroup(); ::renddx9abi::engineFree(group); }
    std::vector<DebugTextLine*>().swap(m_lines);
    std::vector<DebugTextGroup*>().swap(m_groups);
}

#define RD9_DTW_FRONTIER(method, slotOffset)                                        \
    void DebugTextWriter::method()                                                  \
    {                                                                               \
        RD9_FRONTIER("dice::hfe::geom::DebugTextWriter::" #method, slotOffset);     \
    }

RD9_DTW_FRONTIER(slot_0x24, 0x24)
RD9_DTW_FRONTIER(slot_0x28, 0x28)
RD9_DTW_FRONTIER(slot_0x2C, 0x2C)
RD9_DTW_FRONTIER(slot_0x34, 0x34)
RD9_DTW_FRONTIER(slot_0x38, 0x38)
RD9_DTW_FRONTIER(slot_0x3C, 0x3C)

#undef RD9_DTW_FRONTIER

// RefFactory_geom_DebugTextWriter, 0x1007CE00. Same shape and same __fastcall convention as the
// other three factories in this image.
IBase* __fastcall createDebugTextWriter(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    DebugTextWriter* self = new DebugTextWriter();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

// The list is private to this native renderer. Registration does not retain writers.
class DebugTextWriterManager : public IBase
{
public:
    DebugTextWriterManager() : m_refCount(1) {}
    virtual void addRef() { ++m_refCount; }
    virtual int getRef() { return m_refCount; }
    virtual int release()
    {
        const int refs = --m_refCount;
        if (!refs) { this->~DebugTextWriterManager(); ::renddx9abi::engineFree(this); }
        return refs;
    }
    virtual void* queryInterface(unsigned iid) const
    { return iid == RD9_IID_IBASE || iid == 80044u ? const_cast<DebugTextWriterManager*>(this) : 0; }
    virtual void add(DebugTextWriter* writer) { if (writer) m_writers.push_back(writer); }
    virtual void remove(DebugTextWriter* writer) { if (writer) m_writers.remove(writer); }
    virtual void draw()
    {
        for (DebugTextWriter* writer : m_writers)
            if (writer->isEnabled()) writer->draw(); else writer->clear();
    }
private:
    int m_refCount;
    std::list<DebugTextWriter*> m_writers;
};

IBase* __fastcall createDebugTextWriterManager(unsigned iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugTextWriterManager));
    DebugTextWriterManager* manager = storage ? new (storage) DebugTextWriterManager() : 0;
    void* result = manager->queryInterface(iid);
    if (!result) manager->release();
    return static_cast<IBase*>(result);
}

void debugTextWriterManagerAdd(DebugTextWriter* writer)
{ static_cast<DebugTextWriterManager*>(g_DebugTextWriterManager)->add(writer); }
void debugTextWriterManagerRemove(DebugTextWriter* writer)
{ static_cast<DebugTextWriterManager*>(g_DebugTextWriterManager)->remove(writer); }
void debugTextWriterManagerDraw(IBase* manager)
{ static_cast<DebugTextWriterManager*>(manager)->draw(); }
void debugTextWriterSetColour(IBase* writer, unsigned colour)
{ static_cast<DebugTextWriter*>(writer)->setColour(colour); }
void debugTextWriterQueueText(IBase* writer, int x, int y, const vc71::string& text, int alignment)
{ static_cast<DebugTextWriter*>(writer)->queueText(x, y, text, alignment); }

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
